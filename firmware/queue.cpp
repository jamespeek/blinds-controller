#include "queue.h"

static const int QMAX = 32;
static Cmd qBuf[QMAX];
static volatile int qHead = 0;
static volatile int qTail = 0;

void queueInit() { qHead = 0; qTail = 0; }

static bool qFull()  { return ((qTail + 1) % QMAX) == qHead; }
bool qIsEmpty() { return qHead == qTail; }

bool qEnqueue(const Cmd& c) {
  if (qFull()) return false;
  qBuf[qTail] = c;
  qTail = (qTail + 1) % QMAX;
  return true;
}

bool qDequeue(Cmd& c) {
  if (qIsEmpty()) return false;
  c = qBuf[qHead];
  qHead = (qHead + 1) % QMAX;
  return true;
}
