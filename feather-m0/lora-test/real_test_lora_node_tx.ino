#include <SPI.h>
#include <RH_RF95.h>
#include <RHReliableDatagram.h>
#include "TestProtocol.h"

// -------- Pin setup for Feather M0/M4 RFM95 style boards --------
#define RFM95_CS   8
#define RFM95_RST  4
#define RFM95_INT  3

// Change this if your radios are using a different band
#define RF95_FREQ 915.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHReliableDatagram manager(rf95, NODE_ID);

uint8_t seqNum = 0;
unsigned long lastHelloMs = 0;
const unsigned long HELLO_INTERVAL_MS = 2000;

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

bool sendHelloAndWaitForAck() {
  TestPacket pkt = {};
  pkt.msgType = MSG_HELLO;
  pkt.seq = seqNum++;
  snprintf(pkt.text, sizeof(pkt.text), "HELLO FROM NODE");

  bool ok = manager.sendtoWait(
    reinterpret_cast<uint8_t*>(&pkt),
    sizeof(pkt),
    BASE_ID
  );

  Serial.print("Sent HELLO -> ");
  printPacket(pkt);

  if (ok) {
    Serial.println("Base ACKed packet.");
  } else {
    Serial.println("No ACK from base.");
  }

  return ok;
}

void handleIncoming() {
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

  if (pkt.msgType == MSG_PING) {
    TestPacket reply = {};
    reply.msgType = MSG_PONG;
    reply.seq = pkt.seq;
    snprintf(reply.text, sizeof(reply.text), "PONG FROM NODE");

    bool ok = manager.sendtoWait(
      reinterpret_cast<uint8_t*>(&reply),
      sizeof(reply),
      from
    );

    Serial.print("Sent PONG -> ");
    printPacket(reply);
    Serial.println(ok ? "Base ACKed PONG." : "Base did not ACK PONG.");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("\nNode booting...");

  resetRadio();

  if (!manager.init()) {
    Serial.println("Radio init failed.");
    while (1) delay(100);
  }

  rf95.setFrequency(RF95_FREQ);
  rf95.setTxPower(20, false);

  // A good starting point for easy testing
  rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);

  Serial.println("Node ready.");
}

void loop() {
  handleIncoming();

  unsigned long now = millis();
  if (now - lastHelloMs >= HELLO_INTERVAL_MS) {
    lastHelloMs = now;
    sendHelloAndWaitForAck();
  }
}
