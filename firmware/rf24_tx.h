#pragma once
#include <RF24.h>
#include "types.h"

void rfInit();
bool rfOk();
void rfSendCmd(int remote, uint8_t blindMask, bool open, int durationMs);
