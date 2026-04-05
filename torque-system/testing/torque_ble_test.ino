#include <HX711.h>
#include <EEPROM.h>
#include <ArduinoBLE.h>

// -------------------- HX711 SETUP --------------------
#define HX_DT_PIN   3
#define HX_SCK_PIN  2

const uint16_t SAMPLES_PER_READING = 10;
const uint8_t  MOVING_AVG_LEN      = 8;

const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_SCALE = EEPROM_ADDR_MAGIC + sizeof(uint32_t);
const int EEPROM_ADDR_TARE  = EEPROM_ADDR_SCALE + sizeof(double);
const uint32_t MAGIC = 0xDEADBEEF;

HX711 hx;
double g_scaleNmPerCount = 0.0000051962;
double g_scaleNmPerCount_new = 0.0000051962;
long   g_tareCount       = 0;

double torqMA[MOVING_AVG_LEN];
uint8_t maIdx = 0;

// -------------------- BLE SETUP --------------------
BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"); 
BLECharacteristic rxChar("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWrite, 512);
BLECharacteristic txChar("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLENotify, 512);

BLEDevice central;
String bleInputBuffer = "";

// State flags
bool waitingForCalTorque = false;
bool waitingForNewScale = false;

// -------------------- UTILITY FUNCTIONS --------------------
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
    EEPROM.get(EEPROM_ADDR_TARE, g_tareCount);
  }
}

long readCountsAveraged(uint16_t n) {
  long sum = 0;
  for (uint16_t i = 0; i < n; i++) sum += hx.read();
  return sum / (long)n;
}

double filteredTorque(double tNm) {
  torqMA[maIdx] = tNm;
  maIdx = (maIdx + 1) % MOVING_AVG_LEN;
  double s = 0;
  for (uint8_t i = 0; i < MOVING_AVG_LEN; i++) s += torqMA[i];
  return s / MOVING_AVG_LEN;
}

// -------------------- COMMANDS --------------------
void doTare() {
  delay(50);
  g_tareCount = readCountsAveraged(SAMPLES_PER_READING);
  saveTareToEEPROM();
  String msg = "Tare set: " + String(g_tareCount);
  Serial.println(msg);
  txChar.writeValue(msg.c_str());
}

void doCalibrate(double knownNm) {
  knownNm = fabs(knownNm);
  String msg;

  if (knownNm <= 0.0) {
    msg = "Invalid torque. Aborting calibration.";
    Serial.println(msg);
    txChar.writeValue(msg.c_str());
    waitingForCalTorque = false;
    return;
  }

  msg = "Calibrating... please keep the torque steady.";
  Serial.println(msg);
  txChar.writeValue(msg.c_str());

  long counts = readCountsAveraged(5 * SAMPLES_PER_READING);
  long netCounts = labs(counts - g_tareCount);

  if (netCounts == 0) {
    msg = "Net counts = 0; increase load or check wiring.";
    Serial.println(msg);
    txChar.writeValue(msg.c_str());
    waitingForCalTorque = false;
    return;
  }

  g_scaleNmPerCount = knownNm / (double)netCounts;
  saveScaleToEEPROM();

  msg = "Calibration complete. Scale (N·m/count): " + String(g_scaleNmPerCount, 10);
  Serial.println(msg);
  txChar.writeValue(msg.c_str());
  waitingForCalTorque = false;
}

void setScale(double newScale) {
  if (newScale > 0) {
    g_scaleNmPerCount = newScale;
    saveScaleToEEPROM();
    String msg = "Scale set to " + String(g_scaleNmPerCount, 10);
    Serial.println(msg);
    txChar.writeValue(msg.c_str());
  } else {
    String msg = "Invalid scale value.";
    Serial.println(msg);
    txChar.writeValue(msg.c_str());
  }
  waitingForNewScale = false;
}

// -------------------- BLE CALLBACK --------------------
void onRXWrite(BLEDevice central, BLECharacteristic characteristic) {
  String input = String((char*)characteristic.value());
  input.trim();
  if (input.length() == 0) return;

  // If in waiting mode, treat input as a numeric response
  if (waitingForCalTorque) {
    doCalibrate(input.toFloat());
    return;
  }
  if (waitingForNewScale) {
    setScale(input.toFloat());
    return;
  }

  // Normal command handling
  char cmd = input.charAt(0);
  switch (cmd) {
    case 't':
      doTare();
      break;

    case 'c': {
      String msg = "\n--- Calibration ---\n1) Ensure tare = zero torque ('t' first).\n2) Apply known torque (N·m).\n3) Enter torque value now:";
      Serial.println(msg);
      txChar.writeValue(msg.c_str());
      waitingForCalTorque = true;  // 🔹 Set waiting flag
      break;
    }

    case 's': {
      String msg = "Scale (N·m/count): " + String(g_scaleNmPerCount, 10);
      Serial.println(msg);
      txChar.writeValue(msg.c_str());
      break;
    }

    case 'z': {
      String prompt = "Enter new scale (N·m/count):";
      Serial.println(prompt);
      txChar.writeValue(prompt.c_str());
      waitingForNewScale = true;  // 🔹 Set waiting flag
      break;
    }

    default: {
      String msg = "Unknown command.";
      Serial.println(msg);
      txChar.writeValue(msg.c_str());
      break;
    }
  }
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\nBaja Axle Torque – HX711 + BLE");

  hx.begin(HX_DT_PIN, HX_SCK_PIN, 128);
  delay(500);

  //loadFromEEPROM();
  Serial.print("Scale (N·m/count): "); Serial.println(g_scaleNmPerCount, 10);
  Serial.print("Tare (counts): "); Serial.println(g_tareCount);

  for (uint8_t i = 0; i < MOVING_AVG_LEN; i++) torqMA[i] = 0;

  // BLE setup
  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  BLE.setLocalName("TorqueSensor-BLE");
  BLE.setAdvertisedService(uartService);
  uartService.addCharacteristic(rxChar);
  uartService.addCharacteristic(txChar);
  BLE.addService(uartService);
  rxChar.setEventHandler(BLEWritten, onRXWrite);
  BLE.advertise();

  Serial.println("BLE ready. Connect with nRF Connect or similar.");
  Serial.println("Commands: t=tare, c=calibrate, s=show scale, z=set scale");
}

// -------------------- MAIN LOOP --------------------
void loop() {
  BLE.poll();

  // Read HX711 and compute torque
  long counts = readCountsAveraged(SAMPLES_PER_READING);
  long netCounts = counts - g_tareCount;
  double tNm = (double)netCounts * g_scaleNmPerCount;
  double tNmFilt = filteredTorque(tNm);

  const double nm_to_ftlb = 0.73756;
  double tFtlb = tNmFilt * nm_to_ftlb;

  // Send torque readings periodically
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 50) {
    lastPrint = millis();
    String msg = "Torque [N·m]: " + String(tNmFilt, 3) +
                 " | [Ft·lb]: " + String(tFtlb, 3) +
                 " | counts: " + String(netCounts);
    Serial.println(msg);
    txChar.writeValue(msg.c_str());
  }

  // Also allow Serial input to perform same calibration/tare
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (waitingForCalTorque) {
      doCalibrate(line.toFloat());
    } else if (waitingForNewScale) {
      setScale(line.toFloat());
    } else if (line.equalsIgnoreCase("t")) {
      doTare();
    } else if (line.equalsIgnoreCase("c")) {
      String msg = "\n--- Calibration ---\n1) Ensure tare = zero torque ('t' first).\n2) Apply known torque (N·m).\n3) Enter torque value now:";
      Serial.println(msg);
      txChar.writeValue(msg.c_str());
      waitingForCalTorque = true;
    } else if (line.equalsIgnoreCase("s")) {
      String msg = "Scale (N·m/count): " + String(g_scaleNmPerCount, 10);
      Serial.println(msg);
      txChar.writeValue(msg.c_str());
    } else if (line.equalsIgnoreCase("z")) {
      String prompt = "Enter new scale (N·m/count):";
      Serial.println(prompt);
      txChar.writeValue(prompt.c_str());
      waitingForNewScale = true;
    }
  }
}
