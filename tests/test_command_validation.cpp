#include <cassert>
#include "../src/command_validation.h"

int main() {
  assert(isUnsignedDecimal("0", 1));
  assert(isUnsignedDecimal("100", 3));
  assert(!isUnsignedDecimal("", 0));
  assert(!isUnsignedDecimal("1blind", 6));
  assert(!isUnsignedDecimal("-1", 2));
  assert(!isUnsignedDecimal(nullptr, 2));

  int parsed = 0;
  assert(parseUnsignedDecimal("257", 3, parsed) && parsed == 257);
  assert(!parseUnsignedDecimal("2147483648", 10, parsed));
  assert(!parseUnsignedDecimal("257blind", 8, parsed));
}
