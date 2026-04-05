/*
  HX711 → Arduino Pro Micro → Torque (N·m) display
  Wiring: HX711 VCC ↔ VCC (5V or 3.3V), GND ↔ GND, DT↔D3, SCK↔D2
          Bridge E+↔HX E+, E-↔HX E-, A+↔HX A+, A-↔HX A-
  Serial commands:
    t  : tare (zero)
    c  : calibrate (it will prompt for known torque, in N·m)
    s  : print current scale factor (N·m per count)
    z  : set scale factor manually (type value in N·m per count)
*/

#include <HX711.h>
#include <EEPROM.h>

#define HX_DT_PIN   3   // DOUT/DT
#define HX_SCK_PIN  2   // SCK/CLK

// Sampling / smoothing
const uint16_t SAMPLES_PER_READING = 10;    // averaged per measurement
const uint8_t  MOVING_AVG_LEN      = 8;     // simple moving average on torque

// EEPROM address to store scale factor and tare
const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_SCALE = EEPROM_ADDR_MAGIC + sizeof(uint32_t);
const int EEPROM_ADDR_TARE  = EEPROM_ADDR_SCALE + sizeof(double);
const uint32_t MAGIC = 0xDEADBEEF;

HX711 hx;

double g_scaleNmPerCount = 0.000001;  // default; will be replaced by calibration
long   g_tareCount       = 0;

double torqMA[MOVING_AVG_LEN];
uint8_t maIdx = 0;

long readCountsAveraged(uint16_t n) {
  long sum = 0;
  for (uint16_t i = 0; i < n; i++) {
    // blocking read; HX711 output rate depends on RATE pad (10 SPS preferred)
    sum += hx.read();
  }
  return sum / (long)n;
}

void doTare() {
  delay(50);
  uint16_t n = (SAMPLES_PER_READING > 10) ? SAMPLES_PER_READING : 10;
  g_tareCount = readCountsAveraged(n);

  saveTareToEEPROM();
  Serial.print(F("Tare set: ")); Serial.println(g_tareCount);
}

void doCalibrate() {
  Serial.println(F("\n--- Calibration ---"));
  Serial.println(F("1) Ensure tare = current zero torque (press 't' first if needed)."));
  Serial.println(F("2) Apply a known torque (N·m) using a lever arm & weight."));
  Serial.println(F("3) Type the torque value in N·m and press ENTER."));
  Serial.print(F("Known torque [N·m]: "));

  // Read a line as a double
  while (!Serial.available()) { /* wait */ }
  String line = Serial.readStringUntil('\n');
  line.trim();
  double knownNm = line.toFloat();

  if (knownNm <= 0.0) {
    Serial.println(F("Invalid torque. Aborting calibration."));
    return;
  }

  // Capture reading under that known torque
  long counts = readCountsAveraged(5 * SAMPLES_PER_READING);
  long netCounts = counts - g_tareCount;
  if (netCounts == 0) {
    Serial.println(F("Net counts = 0; increase load or check wiring."));
    return;
  }

  g_scaleNmPerCount = knownNm / (double)netCounts;
  saveScaleToEEPROM();

  Serial.print(F("Calibration complete.\nScale (N·m per count): "));
  Serial.println(g_scaleNmPerCount, 10);
}

double filteredTorque(double tNm) {
  torqMA[maIdx] = tNm;
  maIdx = (maIdx + 1) % MOVING_AVG_LEN;
  double s = 0;
  for (uint8_t i = 0; i < MOVING_AVG_LEN; i++) s += torqMA[i];
  return s / MOVING_AVG_LEN;
}

void saveScaleToEEPROM() {
  EEPROM.put(EEPROM_ADDR_MAGIC, MAGIC);
  EEPROM.put(EEPROM_ADDR_SCALE, g_scaleNmPerCount);
}

void saveTareToEEPROM() {
  EEPROM.put(EEPROM_ADDR_MAGIC, MAGIC);
  EEPROM.put(EEPROM_ADDR_TARE, g_tareCount);
}

void loadFromEEPROM() {
  uint32_t magicRead = 0;
  EEPROM.get(EEPROM_ADDR_MAGIC, magicRead);
  if (magicRead == MAGIC) {
    EEPROM.get(EEPROM_ADDR_SCALE, g_scaleNmPerCount);
    EEPROM.get(EEPROM_ADDR_TARE,  g_tareCount);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { /* wait for USB */ }

  Serial.println(F("\nBaja Axle Torque – HX711"));
  hx.begin(HX_DT_PIN, HX_SCK_PIN);   // default gain = 128 on channel A

  // Optional: give HX711 a moment to settle
  delay(500);

  loadFromEEPROM();
  Serial.print(F("Scale (N·m/count): ")); Serial.println(g_scaleNmPerCount, 10);
  Serial.print(F("Tare (counts): "));     Serial.println(g_tareCount);

  // Zero the moving average buffer
  for (uint8_t i = 0; i < MOVING_AVG_LEN; i++) torqMA[i] = 0;
  Serial.println(F("Commands: t=tare, c=calibrate, s=show scale, z=set scale"));
}

void loop() {
  // Handle simple serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 't') doTare();
    else if (cmd == 'c') doCalibrate();
    else if (cmd == 's') {
      Serial.print(F("Scale (N·m/count): "));
      Serial.println(g_scaleNmPerCount, 10);
    } else if (cmd == 'z') {
      Serial.println(F("Enter new scale (N·m per count), then ENTER:"));
      while (!Serial.available()) {}
      String v = Serial.readStringUntil('\n'); v.trim();
      double newScale = v.toFloat();
      if (newScale > 0) {
        g_scaleNmPerCount = newScale;
        saveScaleToEEPROM();
        Serial.print(F("Scale set to ")); Serial.println(g_scaleNmPerCount, 10);
      } else {
        Serial.println(F("Invalid value."));
      }
    }
  }

  // Read HX711 and compute torque
  long counts = readCountsAveraged(SAMPLES_PER_READING);
  long netCounts = counts - g_tareCount;
  double tNm = (double)netCounts * g_scaleNmPerCount;

  // Optional additional smoothing
  double tNmFilt = filteredTorque(tNm);

  //ft*lbs conversion factor 
  const double nm_to_ftlb = 0.73756; 

  //compute ft*lbs torque
  double tFtlb = tNmFilt * nm_to_ftlb; 

  // Print
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 100) {  // ~10 Hz printout
    lastPrint = millis();
    Serial.print(F("Torque [N·m]: "));
    Serial.print(tNmFilt, 3);
    Serial.print(F(" Torque [Ft*lbs]: ")); 
    Serial.print(tFtlb, 3); 
    Serial.print(F("   (raw counts: "));
    Serial.print(netCounts);
    Serial.println(F(")"));
  }
}
