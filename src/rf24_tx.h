#pragma once
#include <RF24.h>
#include "types.h"
#include "rf_profiles.h"

void rfInit();
bool rfOk();
void rfSendCmd(const uint8_t remoteId[2], uint8_t blindMask, bool open, RfProfile profile);
