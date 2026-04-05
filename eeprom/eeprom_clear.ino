#include <EEPROM.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  Serial.println("\n--- EEPROM Clear Utility ---");

  // Adjust this if your EEPROM size differs — 512 is safe for your program
  int eepromSize = 512;  
  EEPROM.begin(eepromSize);

  // Write 0xFF (erased state) to every address
  for (int i = 0; i < eepromSize; i++) {
    EEPROM.write(i, 0xFF);
  }
  EEPROM.commit();

  Serial.println("EEPROM cleared successfully!");
  Serial.println("Now re-upload your main strain gauge + BLE code.");
}

void loop() {
  // Do nothing
}
