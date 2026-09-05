#include <cassert>
#include <string>
#include "../src/rf_profiles.h"

int main() {
  constexpr RfProfilePlan start = rfProfilePlan(RfProfile::Start, 350, 300, 250, 300);
  static_assert(start.activeDurationMs == 350 && start.releaseDurationMs == 300);
  constexpr RfProfilePlan stop = rfProfilePlan(RfProfile::Stop, 350, 300, 250, 300);
  static_assert(stop.activeDurationMs == 250 && stop.releaseDurationMs == 300);
  static_assert(rfGestureCount(RfProfile::Start, 2) == 2);
  static_assert(rfGestureCount(RfProfile::Stop, 2) == 1);
  assert(std::string(rfProfileName(RfProfile::Start)) == "start");
  assert(std::string(rfProfileName(RfProfile::Stop)) == "stop");
}
