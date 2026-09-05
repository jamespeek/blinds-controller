#pragma once

#include <stdint.h>

// Captured from the physical Hunter Douglas remote. The high bit marks the
// active button press; the matching lower value is its release phase.
constexpr uint8_t rfActiveCommand(bool opening) {
  return opening ? 0xA1 : 0xA2;
}

constexpr uint8_t rfReleaseCommand(bool opening) {
  return opening ? 0x21 : 0x22;
}
