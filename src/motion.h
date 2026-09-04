#pragma once
#include "types.h"

struct ZoneConfig;

void motionInit();
bool ownsZone(Zone z);
Zone zoneFromName(const String& name);
const ZoneConfig* zoneConfig(Zone z);
const char* controllerName();
bool controllerConfigured();

void startMove(Zone z, uint8_t blind, Action a, int targetPos);
void tickAllMovement();

uint8_t blindToMask(uint8_t blind);

void motionPublishAllStates();
