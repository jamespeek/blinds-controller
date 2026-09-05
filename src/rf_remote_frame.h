#pragma once

#include <stdint.h>

enum class RemoteDirection : uint8_t {
  Up,
  Down,
};

struct RemoteRfFrame {
  uint8_t remoteId[2];
  uint8_t blindMask;
  RemoteDirection direction;
};

// The remote's release frames (0x21/0x22) intentionally do not decode as
// commands. Only the active direction frames affect controller state.
constexpr bool decodeRemoteRfFrame(const uint8_t packet[5], RemoteRfFrame& frame) {
  if (packet[2] != 0x20) return false;
  if (packet[4] == 0xA1) {
    frame = {{packet[0], packet[1]}, packet[3], RemoteDirection::Up};
    return true;
  }
  if (packet[4] == 0xA2) {
    frame = {{packet[0], packet[1]}, packet[3], RemoteDirection::Down};
    return true;
  }
  return false;
}
