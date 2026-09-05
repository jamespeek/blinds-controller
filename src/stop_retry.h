#pragma once

struct StopRfDirection {
  bool shouldSend;
  bool opening;
};

// A repeated STOP needs the same opposite-direction gesture as the original
// one. Once stationary, the current state no longer encodes travel direction,
// so retain the most recent known direction for a retry.
constexpr StopRfDirection stopRfDirection(bool moving, bool movingOpening,
                                         bool hasLastDirection, bool lastOpening) {
  if (moving) return {true, !movingOpening};
  if (hasLastDirection) return {true, !lastOpening};
  return {false, false};
}
