#include <cassert>
#include "../src/command_validation.h"

int main() {
  assert(isUnsignedDecimal("0", 1));
  assert(isUnsignedDecimal("100", 3));
  assert(!isUnsignedDecimal("", 0));
  assert(!isUnsignedDecimal("1blind", 6));
  assert(!isUnsignedDecimal("-1", 2));
  assert(!isUnsignedDecimal(nullptr, 2));
}
