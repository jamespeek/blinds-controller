#pragma once

#include <stddef.h>
#include <limits.h>

inline bool isUnsignedDecimal(const char* value, size_t length) {
  if (!value || length == 0) return false;
  for (size_t i = 0; i < length; i++) {
    if (value[i] < '0' || value[i] > '9') return false;
  }
  return true;
}

inline bool parseUnsignedDecimal(const char* value, size_t length, int& result) {
  if (!isUnsignedDecimal(value, length)) return false;
  int parsed = 0;
  for (size_t i = 0; i < length; i++) {
    const int digit = value[i] - '0';
    if (parsed > (INT_MAX - digit) / 10) return false;
    parsed = parsed * 10 + digit;
  }
  result = parsed;
  return true;
}
