#include <cassert>

#include "../firmware/queue.h"

static Cmd command(Zone zone, uint8_t blind, Action action, int target = -1) {
  return Cmd{zone, blind, action, target};
}

static Cmd dequeue() {
  Cmd result{};
  assert(qDequeue(result));
  return result;
}

int main() {
  queueInit();
  assert(qEnqueue(command(0, 1, A_OPEN)));
  assert(qEnqueue(command(1, 1, A_CLOSE)));
  assert(qEnqueue(command(0, 1, A_CLOSE)));
  Cmd first = dequeue();
  assert(first.zone == 1 && first.action == A_CLOSE);
  Cmd second = dequeue();
  assert(second.zone == 0 && second.action == A_CLOSE);
  assert(qIsEmpty());

  queueInit();
  assert(qEnqueue(command(0, 1, A_OPEN)));
  assert(qEnqueue(command(0, 1, A_STOP)));
  assert(qEnqueue(command(0, 1, A_CLOSE)));
  Cmd stopped = dequeue();
  assert(stopped.action == A_STOP);
  assert(qIsEmpty());

  queueInit();
  assert(qEnqueue(command(0, 1, A_SET_POS, 25)));
  assert(qEnqueue(command(0, 2, A_OPEN)));
  assert(qEnqueue(command(0, 1, A_STOP)));
  assert(dequeue().blind == 2);
  assert(dequeue().action == A_STOP);
  assert(qIsEmpty());
}
