#include <SPI.h>
#include <LoRa.h>

#define LORA_CS   10
#define LORA_RST   9
#define LORA_IRQ   2

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("LoRa Receiver");

  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  


  //if (!LoRa.begin(915E6)) {
    //Serial.println("LoRa init failed!");
   // while (1);
  //}

  LoRa.begin(800E6); 

  LoRa.setSpreadingFactor(7);     // SF7
  LoRa.setSignalBandwidth(125E3); // BW 125 kHz
  LoRa.setCodingRate4(5);         // CR 4/5
  LoRa.setSyncWord(0x34);         // keeps packets "private" to your network

  Serial.println("LoRa init OK");
}

void loop() {
  int packetSize = LoRa.parsePacket();
if (packetSize) {
  Serial.print("Received: ");
  while (LoRa.available()) {
    Serial.print((char)LoRa.read());
  }
  Serial.print(" | RSSI: ");
  Serial.println(LoRa.packetRssi());
}

  }
