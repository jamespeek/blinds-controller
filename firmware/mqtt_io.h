#pragma once
#include <PubSubClient.h>
#include "types.h"

void mqttInit(PubSubClient& client, const String& deviceId);
void mqttLoopEnsure();

void mqttPublishState(Zone z, uint8_t blind, MoveState s, int pos);
