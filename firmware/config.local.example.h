#pragma once

// Copy this file to config.local.h and replace each placeholder. Never commit
// config.local.h: it contains credentials for the controller's local network.
static const char* WIFI_SSID = "your-wifi-ssid";
static const char* WIFI_PASS = "your-wifi-password";

static const char* MQTT_HOST = "192.168.1.25";
static const uint16_t MQTT_PORT = 1883;
static const char* MQTT_USER = "mqtt-user";
static const char* MQTT_PASS = "mqtt-password";
static const char* MQTT_BASE = "blinds";
static const char* HOMEASSISTANT_STATUS_TOPIC = "homeassistant/status";

// Zones are shared by all controllers. The RF protocol supports up to four
// blinds per zone; unused travel entries should be { 0, 0 }.
static const ZoneConfig ZONE_CONFIGS[] = {
  { "living", { 0x01, 0xE1 }, 2,
    { { 30, 30 }, { 30, 30 }, { 0, 0 }, { 0, 0 } } },
  { "bedroom", { 0x56, 0x75 }, 1,
    { { 20, 20 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
};

// A controller owns commands for the listed zone names. An unknown MAC owns
// no zones and will never transmit RF commands.
static const char* const LIVING_CONTROLLER_ZONES[] = { "living", "bedroom" };

// Each entry is: { Wi-Fi MAC address, controller name, owned zone-name list,
//                  number of names in that list }.
// Use the MAC printed in the controller's startup serial log. Zone names must
// exactly match names in ZONE_CONFIGS above.
static const ControllerConfig CONTROLLER_CONFIGS[] = {
  { "AA:BB:CC:DD:EE:FF", "living-controller", LIVING_CONTROLLER_ZONES, 2 },
};
