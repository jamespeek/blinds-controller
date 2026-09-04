#include "queue.h"

static const int QMAX = 32;
static Cmd qBuf[QMAX];
static volatile int qHead = 0;
static volatile int qTail = 0;

void queueInit() { qHead = 0; qTail = 0; }

static bool qFull()  { return ((qTail + 1) % QMAX) == qHead; }
bool qIsEmpty() { return qHead == qTail; }

static bool sameBlind(const Cmd& a, const Cmd& b) {
  return a.zone == b.zone && a.blind == b.blind;
}

bool qEnqueue(const Cmd& c) {
  bool hasSameBlind = false;
  for (int i = qHead; i != qTail; i = (i + 1) % QMAX) {
    if (sameBlind(qBuf[i], c)) {
      hasSameBlind = true;
      break;
    }
  }
  if (!hasSameBlind && qFull()) return false;

  // Commands have a latest-intent meaning per blind. A STOP supersedes every
  // pending movement for that blind; while it is queued, later movement is
  // ignored so a duplicate CLOSE/OPEN cannot turn a completed stop into a
  // reversal. Commands for other blinds remain FIFO.
  Cmd compacted[QMAX];
  int count = 0;
  bool stopAlreadyQueued = false;
  for (int i = qHead; i != qTail; i = (i + 1) % QMAX) {
    const Cmd& pending = qBuf[i];
    if (!sameBlind(pending, c)) {
      compacted[count++] = pending;
      continue;
    }
    if (c.action == A_STOP) continue;
    if (pending.action == A_STOP) {
      compacted[count++] = pending;
      stopAlreadyQueued = true;
    }
    // A later non-STOP command replaces pending movement for this blind.
  }

  if (!stopAlreadyQueued) compacted[count++] = c;
  qHead = 0;
  qTail = count;
  for (int i = 0; i < count; i++) qBuf[i] = compacted[i];
  return true;
}

bool qDequeue(Cmd& c) {
  if (qIsEmpty()) return false;
  c = qBuf[qHead];
  qHead = (qHead + 1) % QMAX;
  return true;
}
