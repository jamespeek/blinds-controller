#include <cassert>

#include "../src/post_stop_guard.h"

int main() {
  assert(isPostStopDuplicate(A_CLOSE, -1, A_CLOSE, -1, 1200, 1500));
  assert(!isPostStopDuplicate(A_OPEN, -1, A_CLOSE, -1, 1200, 1500));
  assert(!isPostStopDuplicate(A_CLOSE, 0, A_CLOSE, -1, 1200, 1500));
  assert(!isPostStopDuplicate(A_CLOSE, -1, A_CLOSE, -1, 1500, 1500));
  assert(isPostStopDuplicate(A_CLOSE, -1, A_CLOSE, -1, 0xfffffff0u, 20));
}
