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

// Packets per channel "train" (dwell)
static const int RF_TRAIN_PACKETS_0 = 25;  // on RF_HOP_ORDER[0]
static const int RF_TRAIN_PACKETS_1 = 10;  // on RF_HOP_ORDER[1]
static const int RF_TRAIN_PACKETS_2 = 25;  // on RF_HOP_ORDER[2]

// Timing within a train
static const int RF_TRAIN_DELAY_US = 350;  // delay between packets in a train
static const int RF_CH_SETTLE_US = 200;    // delay after setChannel()

static const int RF_SEND_GAP_MS = 20;

// Motion-start gestures are intentionally redundant for reception reliability.
// A stop must be a single complete channel hop, otherwise a receiver may see
// repeated opposite-direction frames as a new movement request.
static const uint16_t RF_START_DURATION_MS = 700;
static const uint8_t RF_START_REPEAT_COUNT = 2;
static const uint16_t RF_START_REPEAT_GAP_MS = 200;
static const uint8_t RF_STOP_CYCLES = 1;

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
