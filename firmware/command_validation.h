#pragma once

#include <stddef.h>

inline bool isUnsignedDecimal(const char* value, size_t length) {
  if (!value || length == 0) return false;
  for (size_t i = 0; i < length; i++) {
    if (value[i] < '0' || value[i] > '9') return false;
  }
  return true;
}
