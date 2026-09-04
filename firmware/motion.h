#pragma once
#include "types.h"

void motionInit();
bool ownsZone(Zone z);

void startMove(Zone z, uint8_t blind, Action a, int targetPos);
void tickAllMovement();

uint8_t blindToMask(uint8_t blind);
int zoneToRemote(Zone z);

void motionPublishAllStates();