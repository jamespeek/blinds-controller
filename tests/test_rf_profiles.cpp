#include <cassert>
#include <string>
#include "../src/rf_profiles.h"

int main() {
  constexpr RfProfilePlan start = rfProfilePlan(RfProfile::Start, 700, 1);
  static_assert(start.durationMs == 700 && start.maxCycles == 0);
  constexpr RfProfilePlan stop = rfProfilePlan(RfProfile::Stop, 700, 1);
  static_assert(stop.durationMs == 0 && stop.maxCycles == 1);
  assert(std::string(rfProfileName(RfProfile::Start)) == "start");
  assert(std::string(rfProfileName(RfProfile::Stop)) == "stop");
}
