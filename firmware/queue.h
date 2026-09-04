#pragma once
#include "types.h"

void queueInit();
bool qEnqueue(const Cmd& c);
bool qDequeue(Cmd& c);
bool qIsEmpty();
