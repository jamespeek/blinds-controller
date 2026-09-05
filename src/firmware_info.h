#pragma once

// Supplied by the Makefile for normal firmware builds. Keeping a fallback
// makes direct Arduino IDE builds identifiable too.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
