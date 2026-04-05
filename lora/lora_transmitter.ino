#include <SPI.h>
#include <LoRa.h>

#define LORA_CS   10
#define LORA_RST   9
#define LORA_IRQ   4

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("LoRa Transmitter");

  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

if (!LoRa.begin(915E6)) {
  Serial.println("LoRa init failed!");
  while (1);
}
 

  LoRa.setSpreadingFactor(7);     // SF7
  LoRa.setSignalBandwidth(125E3); // BW 125 kHz
  LoRa.setCodingRate4(5);         // CR 4/5
  LoRa.setSyncWord(0x34);         // keeps packets "private" to your network

  Serial.println("LoRa init OK");
}

void loop() {
  Serial.println("Sending message...");

  LoRa.beginPacket();
  LoRa.print("Hello from transmitter! ");
  LoRa.print(millis());
  LoRa.endPacket();

  delay(2000);
}
