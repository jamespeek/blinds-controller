#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

#include "config.h"
#include "types.h"
#include "queue.h"
#include "rf24_tx.h"
#include "motion.h"
#include "mqtt_io.h"
#include "http_api.h"
#include "connectivity.h"

#include "esp_task_wdt.h"
#include "esp_log.h"

static WebServer server(80);
static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

static String makeDeviceId() {
  String mac = WiFi.macAddress(); // "0C:DC:.."
  mac.replace(":", "");
  return mac.substring(mac.length() - 6);
}

static void initWatchdog() {
  esp_log_level_set("task_wdt", ESP_LOG_NONE);

  esp_task_wdt_config_t cfg = {
    .timeout_ms = WDT_TIMEOUT_S * 1000,
    .idle_core_mask = (1 << 0),
    .trigger_panic = true
  };

  esp_err_t err = esp_task_wdt_init(&cfg);
  if (err == ESP_ERR_INVALID_STATE) {
    if (DEBUG_LOG) Serial.println("Watchdog already initialised");
  } else if (err != ESP_OK) {
    Serial.printf("Watchdog init failed: %d\n", (int)err);
  }

  err = esp_task_wdt_add(NULL);
  if (err == ESP_ERR_INVALID_STATE) {
    if (DEBUG_LOG) Serial.println("Watchdog task already added");
  } else if (err != ESP_OK) {
    Serial.printf("Watchdog add failed: %d\n", (int)err);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" ESP32 S2 Mini Blind Controller starting");
  Serial.println("========================================");
  Serial.print("Reset reason: ");
  Serial.println((int)esp_reset_reason());

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - startAttempt > 15000) {
      Serial.println("\nWiFi connect timeout, rebooting...");
      ESP.restart();
    }
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Zone: ");
  if (WiFi.macAddress() == FRONT_ESP_MAC) {
    Serial.println("front");
  } else if (WiFi.macAddress() == BACK_ESP_MAC) {
    Serial.println("back");
  } else {
    Serial.println("unknown");
  }

  
  queueInit();

  rfInit();
  if (!rfOk()) {
    Serial.println("RF24 init failed (wiring/power).");
  }

  motionInit();

  String deviceId = makeDeviceId();
  mqttInit(mqtt, deviceId);
  connectivityInit();

  httpInit(server);

  initWatchdog();

  Serial.println("Setup complete");
  Serial.println("========================================");
}

void loop() {
  esp_task_wdt_reset();

  httpLoop(server);

  connectivityLoop();
  mqttLoopEnsure();

  tickAllMovement();

  // Drain one command per loop (RF send itself is still blocking but spaced internally)
  Cmd c;
  if (qDequeue(c)) {
    startMove(c.zone, c.blind, c.action, c.targetPos);
  }
}
