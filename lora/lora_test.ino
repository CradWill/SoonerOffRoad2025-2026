#include <SPI.h>
#include <LoRa.h>

// Pin definitions
#define LORA_CS   10
#define LORA_RST   9
#define LORA_IRQ   2

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("LoRa Initializing...");
  

  // Start SPI manually (optional, LoRa.begin handles it too)
  SPI.begin(13, 12, 11, LORA_CS);

  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  if (!LoRa.begin(915E6)) {   // Change to 433E6 / 868E6 if needed
    Serial.println("LoRa init failed. Check wiring!");
    while (1);
  }

  


  Serial.println("LoRa init OK!");
}

void loop() {
  Serial.println("Sending packet...");
  LoRa.beginPacket();
  LoRa.print("Hello from Nano ESP32");
  LoRa.endPacket();

  delay(2000);
}
