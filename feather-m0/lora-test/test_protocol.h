#ifndef TELEMETRY_PROTOCOL_H
#define TELEMETRY_PROTOCOL_H

#include <Arduino.h>

static const uint8_t BASE_ID = 1;
static const uint8_t NODE_ID = 2;

enum MsgType : uint8_t {
  MSG_HELLO = 1,
  MSG_ACK   = 2,
  MSG_PING  = 3,
  MSG_PONG  = 4
};

struct TestPacket {
  uint8_t msgType;
  uint8_t seq;
  char text[24];
};

#endif
