#include <ArduinoBLE.h>

BLEService uartService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E"); // UART service
BLECharacteristic rxChar("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", BLEWrite, 512);
BLECharacteristic txChar("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLENotify, 512);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  BLE.setLocalName("NanoESP32-BLE");
  BLE.setAdvertisedService(uartService);
  uartService.addCharacteristic(rxChar);
  uartService.addCharacteristic(txChar);
  BLE.addService(uartService);
  BLE.advertise();

  Serial.println("BLE UART started. Open nRF Connect and connect to see messages!");
}

void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Connected to: ");
    Serial.println(central.address());

    while (central.connected()) {
      static unsigned long lastMillis = 0;
      if (millis() - lastMillis >= 1000) {
        lastMillis = millis();

        String msg = "Hello from Nano ESP32 (" + String(millis() / 1000) + "s)";
        txChar.writeValue(msg.c_str());  // ✅ convert to const char*

        Serial.println("Sent: " + msg);
      }
    }

    Serial.println("Disconnected");
  }
}
