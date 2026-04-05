#include <HX711.h>
#include <ArduinoBLE.h>

// ======================================================
// ----------- USER-DEFINED CONSTANT CALIBRATION --------
// ======================================================
// After calibration, update this value and recompile:
#define SCALE_NM_PER_COUNT   0.0000051962    // <-- update manually after calibrating

// ======================================================
// ----------------- HX711 SETUP ------------------------
// ======================================================
#define HX_DT_PIN   3
#define HX_SCK_PIN  2

const uint16_t SAMPLES_PER_READING = 10;
const uint8_t  MOVING_AVG_LEN      = 8;

HX711 hx;

double g_scaleNmPerCount = SCALE_NM_PER_COUNT;
long   g_tareCount = 0;

double torqMA[MOVING_AVG_LEN];
uint8_t maIdx = 0;

// ======================================================
// -------------------- BLE SETUP ------------------------
// ======================================================
BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic rxChar("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWrite, 512);
BLECharacteristic txChar("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLENotify, 512);

bool waitingForCalTorque = false;
bool waitingForNewScale = false;

// ======================================================
// ----------------- UTILITY FUNCTIONS -------------------
// ======================================================
long readCountsAveraged(uint16_t n) {
  long sum = 0;
  for (uint16_t i = 0; i < n; i++) sum += hx.read();
  return sum / (long)n;
}

double filteredTorque(double x) {
  torqMA[maIdx] = x;
  maIdx = (maIdx + 1) % MOVING_AVG_LEN;
  double s = 0;
  for (uint8_t i = 0; i < MOVING_AVG_LEN; i++) s += torqMA[i];
  return s / MOVING_AVG_LEN;
}

// ======================================================
// -------------------- COMMANDS -------------------------
// ======================================================
void doTare() {
  delay(50);
  g_tareCount = readCountsAveraged(SAMPLES_PER_READING);

  // clear filter memory
  for (uint8_t i = 0; i < MOVING_AVG_LEN; i++) torqMA[i] = 0;

  String msg = "Tare set: " + String(g_tareCount);
  Serial.println(msg);
  txChar.writeValue(msg.c_str());
}

void doCalibrate(double knownNm) {
  knownNm = fabs(knownNm);

  if (knownNm <= 0.0) {
    txChar.writeValue("Invalid torque value.");
    waitingForCalTorque = false;
    return;
  }

  txChar.writeValue("Calibrating… hold torque steady.");

  long avgCounts = readCountsAveraged(5 * SAMPLES_PER_READING);
  long netCounts = avgCounts - g_tareCount;
  netCounts = labs(netCounts);

  if (netCounts == 0) {
    txChar.writeValue("Net counts = 0. Check wiring or increase load.");
    waitingForCalTorque = false;
    return;
  }

  g_scaleNmPerCount = knownNm / (double)netCounts;

  // give the line you paste into code
  String line = "PASTE INTO CODE:\n#define SCALE_NM_PER_COUNT " + String(g_scaleNmPerCount, 10);
  Serial.println(line);
  txChar.writeValue(line.c_str());

  waitingForCalTorque = false;
}

void setScale(double newScale) {
  if (newScale <= 0) {
    txChar.writeValue("Invalid scale.");
    return;
  }

  g_scaleNmPerCount = newScale;

  String msg = "New scale applied (not saved): " + String(g_scaleNmPerCount, 10);
  Serial.println(msg);
  txChar.writeValue(msg.c_str());

  waitingForNewScale = false;
}

// ======================================================
// ---------------- BLE CALLBACK -------------------------
// ======================================================
void onRXWrite(BLEDevice central, BLECharacteristic characteristic) {
  String input = String((char*)characteristic.value());
  input.trim();
  if (input.length() == 0) return;

  if (waitingForCalTorque) { doCalibrate(input.toFloat()); return; }
  if (waitingForNewScale) { setScale(input.toFloat()); return; }

  char cmd = input.charAt(0);

  switch (cmd) {
    case 't': doTare(); break;

    case 'c': {
      String msg = "Apply known torque (N·m), then input value:";
      txChar.writeValue(msg.c_str());
      waitingForCalTorque = true;
      break;
    }

    case 's': {
      String msg = "Scale: " + String(g_scaleNmPerCount, 10);
      txChar.writeValue(msg.c_str());
      break;
    }

    case 'z': {
      txChar.writeValue("Enter new scale:");
      waitingForNewScale = true;
      break;
    }

    default:
      txChar.writeValue("Unknown command.");
  }
}

// ======================================================
// ------------------------ SETUP ------------------------
// ======================================================
void setup() {
  Serial.begin(115200);

  hx.begin(HX_DT_PIN, HX_SCK_PIN);
  delay(500);

  Serial.print("Using scale = ");
  Serial.println(g_scaleNmPerCount, 10);

  // init filter
  for (uint8_t i = 0; i < MOVING_AVG_LEN; i++) torqMA[i] = 0;

  //read counts and tare
  readCountsAveraged(SAMPLES_PER_READING); 
  doTare(); 

  // BLE
  BLE.begin();
  BLE.setLocalName("TorqueSensor-BLE");
  BLE.setAdvertisedService(uartService);
  uartService.addCharacteristic(rxChar);
  uartService.addCharacteristic(txChar);
  BLE.addService(uartService);
  rxChar.setEventHandler(BLEWritten, onRXWrite);
  BLE.advertise();

  Serial.println("BLE ready.");
}

// ======================================================
// ------------------------- LOOP ------------------------
// ======================================================
void loop() {
  BLE.poll();
  

  long counts = readCountsAveraged(SAMPLES_PER_READING);
  long netCounts = counts - g_tareCount; 
  double tNm = netCounts * g_scaleNmPerCount;
  double tNmFilt = filteredTorque(tNm);

  const double nm_to_ftlb = 0.73756;
  double tFtlb = tNmFilt * nm_to_ftlb;

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    String msg = "Torque [N·m]: " + String(tNmFilt, 3) +
                 " | [Ft·lb]: " + String(tFtlb, 3) +
                 " | counts: " + String(netCounts);
    Serial.println(msg);
    txChar.writeValue(msg.c_str());
  }
}
