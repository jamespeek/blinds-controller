#pragma once

#include <stdint.h>

// A profile describes one bounded RF transmission gesture.
enum class RfProfile : uint8_t {
  Start,
  Stop,
};

struct RfProfilePlan {
  uint16_t activeDurationMs;
  uint16_t releaseDurationMs;
};

constexpr RfProfilePlan rfProfilePlan(RfProfile profile, uint16_t startActiveDurationMs,
                                      uint16_t startReleaseDurationMs, uint16_t stopActiveDurationMs,
                                      uint16_t stopReleaseDurationMs) {
  return profile == RfProfile::Start
      ? RfProfilePlan{startActiveDurationMs, startReleaseDurationMs}
      : RfProfilePlan{stopActiveDurationMs, stopReleaseDurationMs};
}

constexpr uint8_t rfGestureCount(RfProfile profile, uint8_t startGestureCount) {
  return profile == RfProfile::Start ? startGestureCount : 1;
}

constexpr const char* rfProfileName(RfProfile profile) {
  return profile == RfProfile::Start ? "start" : "stop";
}
