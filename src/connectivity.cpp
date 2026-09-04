#include <WiFi.h>

#include "config.h"
#include "connectivity.h"
#include "mqtt_io.h"

static uint32_t wifiOutageStartMs = 0;
static uint32_t mqttOutageStartMs = 0;
static uint32_t nextWifiAttemptMs = 0;
static uint32_t wifiRetryDelayMs = NETWORK_RETRY_INITIAL_MS;
static uint32_t lastWifiRadioResetMs = 0;

static bool due(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

static void startWifi(bool resetRadio) {
  if (resetRadio) {
    Serial.println("[NET] Restarting Wi-Fi radio");
    WiFi.disconnect(true, false);
  } else {
    Serial.println("[NET] Retrying Wi-Fi connection");
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

static void recordWifiRetry(uint32_t now) {
  nextWifiAttemptMs = now + wifiRetryDelayMs;
  wifiRetryDelayMs = min(wifiRetryDelayMs * 2, NETWORK_RETRY_MAX_MS);
}

void connectivityInit() {
  wifiOutageStartMs = 0;
  mqttOutageStartMs = 0;
  nextWifiAttemptMs = 0;
  wifiRetryDelayMs = NETWORK_RETRY_INITIAL_MS;
  lastWifiRadioResetMs = millis();
}

void connectivityLoop() {
  uint32_t now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiOutageStartMs == 0) {
      wifiOutageStartMs = now;
      nextWifiAttemptMs = now;
      wifiRetryDelayMs = NETWORK_RETRY_INITIAL_MS;
      Serial.println("[NET] Wi-Fi disconnected");
    }

    uint32_t outageMs = now - wifiOutageStartMs;
    if (outageMs >= NETWORK_DEVICE_REBOOT_MS) {
      Serial.println("[NET] Wi-Fi unavailable for 30 minutes; rebooting");
      ESP.restart();
      return;
    }

    bool resetRadio = (now - lastWifiRadioResetMs) >= NETWORK_WIFI_RESET_MS;
    if (resetRadio || due(now, nextWifiAttemptMs)) {
      startWifi(resetRadio);
      if (resetRadio) lastWifiRadioResetMs = now;
      recordWifiRetry(now);
    }
    return;
  }

  if (wifiOutageStartMs != 0) {
    Serial.println("[NET] Wi-Fi reconnected");
    wifiOutageStartMs = 0;
    wifiRetryDelayMs = NETWORK_RETRY_INITIAL_MS;
    nextWifiAttemptMs = 0;
    lastWifiRadioResetMs = now;
  }

  if (mqttIsConnected()) {
    mqttOutageStartMs = 0;
    return;
  }

  if (mqttOutageStartMs == 0) {
    mqttOutageStartMs = now;
    Serial.println("[NET] MQTT disconnected");
  }

  uint32_t mqttOutageMs = now - mqttOutageStartMs;
  if (mqttOutageMs >= NETWORK_DEVICE_REBOOT_MS) {
    Serial.println("[NET] MQTT unavailable for 30 minutes; rebooting");
    ESP.restart();
    return;
  }

  if ((now - lastWifiRadioResetMs) >= NETWORK_WIFI_RESET_MS) {
    startWifi(true);
    lastWifiRadioResetMs = now;
    wifiOutageStartMs = now;
    nextWifiAttemptMs = now + NETWORK_RETRY_INITIAL_MS;
    wifiRetryDelayMs = NETWORK_RETRY_INITIAL_MS;
  }
}
