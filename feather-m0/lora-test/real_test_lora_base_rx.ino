#include <SPI.h>
#include <RH_RF95.h>
#include <RHReliableDatagram.h>
#include "TestProtocol.h"

// -------- Pin setup for Feather M0/M4 RFM95 style boards --------
#define RFM95_CS   8
#define RFM95_RST  4
#define RFM95_INT  3

#define RF95_FREQ 915.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHReliableDatagram manager(rf95, BASE_ID);

uint8_t pingSeq = 0;

void resetRadio() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);
}

void printPacket(const TestPacket &pkt) {
  Serial.print("type=");
  Serial.print(pkt.msgType);
  Serial.print(" seq=");
  Serial.print(pkt.seq);
  Serial.print(" text=\"");
  Serial.print(pkt.text);
  Serial.println("\"");
}

void receivePackets() {
  if (!manager.available()) return;

  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  uint8_t from = 0;

  if (!manager.recvfromAck(buf, &len, &from)) {
    return;
  }

  if (len < sizeof(TestPacket)) {
    Serial.println("Received short packet.");
    return;
  }

  TestPacket pkt;
  memcpy(&pkt, buf, sizeof(pkt));

  Serial.print("Received from ");
  Serial.print(from);
  Serial.print(" -> ");
  printPacket(pkt);

  Serial.print("RSSI: ");
  Serial.println(rf95.lastRssi(), DEC);
}

void sendPing() {
  TestPacket pkt = {};
  pkt.msgType = MSG_PING;
  pkt.seq = pingSeq++;
  snprintf(pkt.text, sizeof(pkt.text), "PING FROM BASE");

  bool ok = manager.sendtoWait(
    reinterpret_cast<uint8_t*>(&pkt),
    sizeof(pkt),
    NODE_ID
  );

  Serial.print("Sent PING -> ");
  printPacket(pkt);

  if (!ok) {
    Serial.println("Node did not ACK the PING packet.");
    return;
  }

  unsigned long start = millis();
  while (millis() - start < 1000) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    uint8_t from = 0;

    if (manager.available() && manager.recvfromAck(buf, &len, &from)) {
      if (len >= sizeof(TestPacket)) {
        TestPacket reply;
        memcpy(&reply, buf, sizeof(reply));

        Serial.print("Reply from ");
        Serial.print(from);
        Serial.print(" -> ");
        printPacket(reply);
        Serial.print("RSSI: ");
        Serial.println(rf95.lastRssi(), DEC);

        if (reply.msgType == MSG_PONG && reply.seq == pkt.seq) {
          Serial.println("Got matching PONG.");
          return;
        }
      }
    }
  }

  Serial.println("Timed out waiting for PONG.");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("\nBase booting...");

  resetRadio();

  if (!manager.init()) {
    Serial.println("Radio init failed.");
    while (1) delay(100);
  }

  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(20, false);
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);

  Serial.println("Base ready.");
  Serial.println("Type 'ping' in Serial Monitor to send a ping.");
}

void loop() {
  receivePackets();

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "ping") {
      sendPing();
    } else if (cmd.length() > 0) {
      Serial.println("Unknown command. Type: ping");
    }
  }
}
