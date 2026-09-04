#pragma once

#include <stdint.h>

// A profile describes one RF transmission gesture. Starting movement needs
// broad redundancy; stopping must be deliberately short so repeated packets
// cannot become a second, opposite-direction press at the blind receiver.
enum class RfProfile : uint8_t {
  Start,
  Stop,
};

struct RfProfilePlan {
  uint16_t durationMs;
  uint8_t maxCycles;
};

constexpr RfProfilePlan rfProfilePlan(RfProfile profile, uint16_t startDurationMs,
                                      uint8_t stopCycles) {
  return profile == RfProfile::Start
      ? RfProfilePlan{startDurationMs, 0}
      : RfProfilePlan{0, stopCycles};
}

constexpr const char* rfProfileName(RfProfile profile) {
  return profile == RfProfile::Start ? "start" : "stop";
}
