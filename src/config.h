#pragma once
#include <Arduino.h>

static constexpr uint8_t MAX_BLINDS_PER_ZONE = 4;

struct TravelTimeCfg {
  uint16_t open_s;
  uint16_t close_s;
};

struct ZoneConfig {
  const char* name;
  uint8_t remoteId[2];
  uint8_t blindCount;
  TravelTimeCfg travel[MAX_BLINDS_PER_ZONE];
};

struct ControllerConfig {
  const char* mac;
  const char* name;
  const char* const* zoneNames;
  uint8_t zoneCount;
};

template <typename T, size_t N>
constexpr size_t countOf(const T (&)[N]) {
  return N;
}

/* ========= WiFi / MQTT ========= */
#include "config.local.h"

static constexpr size_t ZONE_COUNT = countOf(ZONE_CONFIGS);
static constexpr size_t CONTROLLER_COUNT = countOf(CONTROLLER_CONFIGS);
static_assert(ZONE_COUNT > 0, "At least one zone must be configured");
static_assert(CONTROLLER_COUNT > 0, "At least one controller must be configured");

/* ========= Debug ========= */
static const bool DEBUG_LOG = false;

/* ========= Watchdog ========= */
static const int WDT_TIMEOUT_S = 15;

/* ========= Connectivity recovery ========= */
static const uint32_t NETWORK_RETRY_INITIAL_MS = 2000;
static const uint32_t NETWORK_RETRY_MAX_MS = 60000;
static const uint32_t NETWORK_WIFI_RESET_MS = 5UL * 60UL * 1000UL;
static const uint32_t NETWORK_DEVICE_REBOOT_MS = 30UL * 60UL * 1000UL;
static const uint16_t MQTT_SOCKET_TIMEOUT_S = 5;

/* ========= RF24 wiring (ESP32-S2 Mini) ========= */
static const int RF_CE_PIN = 9;  // purple
static const int RF_CS_PIN = 11;  // blue
static const int RF24_SCK  = 12;  // green
static const int RF24_MOSI = 18;  // yellow
static const int RF24_MISO = 16;  // orange

/* ========= RF settings ========= */
static const int RF_CHANNEL_0 = 33;
static const int RF_CHANNEL_1 = 52;
static const int RF_CHANNEL_2 = 71;

// TX shaping (matches working remote-frame beacon)
// Hop order observed: 52 -> 71 -> 33 (i.e. CH1 -> CH2 -> CH0)
static const uint8_t RF_HOP_ORDER[3] = { RF_CHANNEL_1, RF_CHANNEL_2, RF_CHANNEL_0 };
static const uint16_t RF_LISTEN_CHANNEL_DWELL_MS = 3;
static const uint16_t RF_REMOTE_DEDUP_MS = 600;

// The physical remote sends one packet on each channel in rapid succession,
// then immediately repeats the round-robin sequence.
static const int RF_CH_SETTLE_US = 200;  // delay after setChannel()

// A physical remote press has an active direction phase followed by a matching
// release phase. These measured values are used for both movement and stopping.
static const uint16_t RF_START_ACTIVE_MS = 350;
static const uint16_t RF_START_RELEASE_MS = 300;
// A remote stop is a brief opposite-direction press followed by a longer
// release frame. Keeping the active phase short prevents it being treated as
// a new movement command after the blind has already stopped.
static const uint16_t RF_STOP_ACTIVE_MS = 190;
static const uint16_t RF_STOP_RELEASE_MS = 400;
static const uint8_t RF_START_GESTURE_COUNT = 2;
static const uint16_t RF_START_GESTURE_GAP_MS = 100;

// A command that stopped opposite movement is ignored briefly if it is
// repeated, preventing an HTTP/MQTT retry from immediately reversing motion.
static const uint16_t POST_STOP_DEDUP_MS = 1500;

// Set at build time with -DRF_DRY_RUN=1 to exercise timing and logging without
// writing any packets to the radio. This is safer than using another channel,
// which would still transmit RF energy.
#ifndef RF_DRY_RUN
#define RF_DRY_RUN 0
#endif

/* ========= Position clamp rules =========
   - set_position <= 15 -> CLOSE
   - set_position >= 85 -> OPEN
   - otherwise do partial move (15..85) and stop with opposite burst
*/
static const int SETPOS_MIN = 15;
static const int SETPOS_MAX = 85;

/* ========= RF protocol constants ========= */
static const uint8_t RF_ADDR[3] = { 0xE7, 0xE7, 0xE7 };
