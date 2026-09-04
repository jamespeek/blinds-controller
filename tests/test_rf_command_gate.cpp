#include <cassert>

#include "../src/rf_command_gate.h"

int main() {
  assert(canTransmitCommand(true, true));
  assert(!canTransmitCommand(false, true));
  assert(!canTransmitCommand(true, false));
  assert(!canTransmitCommand(false, false));
}
