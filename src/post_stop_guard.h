#pragma once

#include "types.h"

inline bool isPostStopDuplicate(Action action, int targetPos, Action stoppedAction,
                                int stoppedTargetPos, uint32_t now, uint32_t untilMs) {
  return action == stoppedAction && targetPos == stoppedTargetPos &&
         (int32_t)(now - untilMs) < 0;
}
