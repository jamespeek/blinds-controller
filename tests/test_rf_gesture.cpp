#include <cassert>

#include "../src/rf_gesture.h"

int main() {
  static_assert(rfActiveCommand(true) == 0xA1);
  static_assert(rfReleaseCommand(true) == 0x21);
  static_assert(rfActiveCommand(false) == 0xA2);
  static_assert(rfReleaseCommand(false) == 0x22);
}
