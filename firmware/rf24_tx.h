#pragma once
#include <RF24.h>
#include "types.h"

void rfInit();
bool rfOk();
void rfSendCmd(const uint8_t remoteId[2], uint8_t blindMask, bool open, int durationMs);
