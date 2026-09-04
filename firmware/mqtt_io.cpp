#include <WiFi.h>
#include "config.h"
#include "mqtt_io.h"
#include "queue.h"
#include "motion.h"

static PubSubClient* mqtt = nullptr;
static String devId;
static uint32_t nextConnectAttemptMs = 0;
static uint32_t connectRetryDelayMs = NETWORK_RETRY_INITIAL_MS;

static inline void logLine(const String& s) {
  if (DEBUG_LOG) Serial.println(s);
}

static String topicState(Zone z, uint8_t blind) {
  const ZoneConfig* config = zoneConfig(z);
  return config ? String(MQTT_BASE) + "/" + config->name + "/" + blind + "/state" : "";
}
static String topicPos(Zone z, uint8_t blind) {
  const ZoneConfig* config = zoneConfig(z);
  return config ? String(MQTT_BASE) + "/" + config->name + "/" + blind + "/position" : "";
}

void mqttPublishState(Zone z, uint8_t blind, MoveState s, int pos) {
  if (!mqtt) return;

  const char* st =
    (s == S_OPEN) ? "open" :
    (s == S_CLOSED) ? "closed" :
    (s == S_OPENING) ? "opening" : "closing";

  mqtt->publish(topicState(z, blind).c_str(), st, false);

  char buf[8];
  snprintf(buf, sizeof(buf), "%d", pos);
  mqtt->publish(topicPos(z, blind).c_str(), buf, false);
}

static Zone parseZoneFromTopic(const String& t, uint8_t& blind) {
  // blinds/<zone>/<blind>/set
  String prefix = String(MQTT_BASE) + "/";
  if (!t.startsWith(prefix)) return Z_UNKNOWN;

  int baseLen = prefix.length();
  int p1 = t.indexOf('/', baseLen);
  if (p1 < 0) return Z_UNKNOWN;
  int p2 = t.indexOf('/', p1 + 1);
  if (p2 < 0) return Z_UNKNOWN;

  String zone = t.substring(baseLen, p1);
  String blindStr = t.substring(p1 + 1, p2);
  String leaf = t.substring(p2 + 1);

  if (leaf != "set") return Z_UNKNOWN;

  blind = (uint8_t)blindStr.toInt();

  return zoneFromName(zone);
}

static Action parseAction(const String& payload, int& targetPosOut) {
  targetPosOut = -1;
  if (payload == "OPEN") return A_OPEN;
  if (payload == "CLOSE") return A_CLOSE;
  if (payload == "STOP") return A_STOP;

  bool isNum = payload.length() > 0;
  for (int i = 0; i < (int)payload.length(); i++) {
    if (!isDigit(payload[i])) { isNum = false; break; }
  }
  if (isNum) {
    int v = payload.toInt();
    if (v >= 0 && v <= 100) { targetPosOut = v; return A_SET_POS; }
  }
  return A_STOP;
}

static void callback(char* topicC, byte* payloadB, unsigned int len) {
  if (!mqtt) return;

  String topic(topicC);
  String payload;
  payload.reserve(len);
  for (unsigned int i=0; i<len; i++) payload += (char)payloadB[i];
  payload.trim();

  logLine("[MQTT] RX topic=" + topic + " payload='" + payload + "'");

  if (topic == HOMEASSISTANT_STATUS_TOPIC) {
    if (payload == "online") {
      Serial.println("[MQTT] Home Assistant online -> republish all states");
      motionPublishAllStates();
    }
    return;
  }

  uint8_t blind = 0;
  Zone z = parseZoneFromTopic(topic, blind);
  const ZoneConfig* config = zoneConfig(z);
  if (!config || blind < 1 || blind > config->blindCount) return;
  if (!ownsZone(z)) return;

  int targetPos = -1;
  Action a = parseAction(payload, targetPos);

  Cmd c{z, blind, a, targetPos};
  if (!qEnqueue(c)) logLine("[MQTT] Queue FULL - dropped");
}

static bool connectOnce() {
  String clientId = String(MQTT_BASE) + "-" + devId;
  String will = String(MQTT_BASE) + "/" + devId + "/status";

  bool ok;
  if (strlen(MQTT_USER) == 0) {
    ok = mqtt->connect(clientId.c_str(), will.c_str(), 1, true, "offline");
  } else {
    ok = mqtt->connect(clientId.c_str(), MQTT_USER, MQTT_PASS, will.c_str(), 1, true, "offline");
  }

  if (ok) {
    mqtt->publish(will.c_str(), "online", true);
    String sub = String(MQTT_BASE) + "/+/+/set";
    mqtt->subscribe(sub.c_str());

    mqtt->subscribe(HOMEASSISTANT_STATUS_TOPIC);
  }
  return ok;
}

void mqttInit(PubSubClient& client, const String& deviceId) {
  mqtt = &client;
  devId = deviceId;
  mqtt->setServer(MQTT_HOST, MQTT_PORT);
  mqtt->setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt->setCallback(callback);
}

bool mqttIsConnected() {
  return mqtt && mqtt->connected();
}

void mqttLoopEnsure() {
  if (!mqtt) return;

  if (WiFi.status() != WL_CONNECTED) return;

  if (!mqtt->connected()) {
    uint32_t now = millis();
    if ((int32_t)(now - nextConnectAttemptMs) < 0) return;

    Serial.print("Connecting to MQTT broker ");
    Serial.print(MQTT_HOST);
    Serial.print(" ... ");

    if (connectOnce()) {
      Serial.println("connected");
      motionPublishAllStates();
      connectRetryDelayMs = NETWORK_RETRY_INITIAL_MS;
      nextConnectAttemptMs = 0;

    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt->state());
      Serial.print(" retrying in ");
      Serial.print(connectRetryDelayMs / 1000.0f, 1);
      Serial.println("s");

      nextConnectAttemptMs = now + connectRetryDelayMs;
      connectRetryDelayMs = min(connectRetryDelayMs * 2, NETWORK_RETRY_MAX_MS);
      return;
    }
  }

  mqtt->loop();
}
