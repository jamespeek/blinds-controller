#pragma once
#include <Arduino.h>

using Zone = uint8_t;
static constexpr Zone Z_UNKNOWN = 255;
enum Action : uint8_t { A_OPEN=0, A_CLOSE=1, A_STOP=2, A_SET_POS=3 };
enum MoveState : uint8_t { S_OPEN=0, S_CLOSED=1, S_OPENING=2, S_CLOSING=3 };

struct Cmd {
  Zone zone;
  uint8_t blind;     // 1..4
  Action action;
  int targetPos;     // 0..100 for A_SET_POS, else -1
};

struct TravelTime { uint16_t open_s; uint16_t close_s; };

struct BlindRuntime {
  MoveState state;
  int position;              // 0..100 (optimistic)
  int startPos;
  int targetPos;             // 0..100
  uint32_t moveStartMs;
  uint32_t moveDurationMs;
  bool moving;

  bool partialMove;          // only for 15..85 setpos
  bool openingDir;           // direction used for stop burst

  uint32_t lastPubMs;        // ✅ per-blind publish throttling (fix)
};
