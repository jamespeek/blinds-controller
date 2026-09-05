#include <cassert>

#include "../src/stop_retry.h"

int main() {
  StopRfDirection stoppingUp = stopRfDirection(true, true, true, true);
  assert(stoppingUp.shouldSend && !stoppingUp.opening);

  StopRfDirection stoppingDown = stopRfDirection(true, false, true, false);
  assert(stoppingDown.shouldSend && stoppingDown.opening);

  StopRfDirection retryAfterUp = stopRfDirection(false, false, true, true);
  assert(retryAfterUp.shouldSend && !retryAfterUp.opening);

  StopRfDirection retryAfterDown = stopRfDirection(false, true, true, false);
  assert(retryAfterDown.shouldSend && retryAfterDown.opening);

  assert(!stopRfDirection(false, false, false, false).shouldSend);
}
