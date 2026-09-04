#pragma once
#include <Arduino.h>

/* ========= WiFi / MQTT ========= */
#include "config.local.h"

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

/* ========= Topics ========= */
static const char* MQTT_BASE = "blinds";  // blinds/<zone>/<blind>/set

/* ========= Zones ========= */
static const char* ZONE_NAMES[] = { "front", "kitchen", "back" };

/* ========= Ownership by MAC =========
   - Front ESP: front + kitchen
   - Back ESP: back
   Put your back ESP MAC here.
*/
static const char* FRONT_ESP_MAC = "CC:8D:A2:8C:70:96";
static const char* BACK_ESP_MAC = "CC:8D:A2:8B:4F:40";

/* ========= Travel times (seconds) ========= */
struct TravelTimeCfg {
  uint16_t open_s;
  uint16_t close_s;
};

// { down time, up time }
static const TravelTimeCfg TRAVEL_FRONT[4] = { { 7, 4 }, { 30, 27 }, { 30, 27 }, { 30, 27 } };
static const TravelTimeCfg TRAVEL_KITCHEN[4] = { { 12, 10 }, { 20, 20 }, { 20, 20 }, { 20, 20 } };  // only [0] used
static const TravelTimeCfg TRAVEL_BACK[4] = { { 36, 32 }, { 30, 30 }, { 30, 25 }, { 30, 25 } }; // 3 not used

/* ========= Position clamp rules =========
   - set_position <= 15 -> CLOSE
   - set_position >= 85 -> OPEN
   - otherwise do partial move (15..85) and stop with opposite burst
*/
static const int SETPOS_MIN = 15;
static const int SETPOS_MAX = 85;

/* ========= RF remote ID bytes =========
   Replace if needed.
*/
static const uint8_t REMOTE_ID[4][2] = {
  { 0x00, 0x00 },  // unused index 0
  { 0x01, 0xE1 },  // remote 1 (front)
  { 0x56, 0x75 },  // remote 2 (kitchen)
  { 0x35, 0xF5 }   // remote 3 (back)
};

/* ========= RF protocol constants ========= */
static const uint8_t RF_ADDR[3] = { 0xE7, 0xE7, 0xE7 };
