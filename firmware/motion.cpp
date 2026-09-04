#include <WiFi.h>
#include <cstring>

#include "motion.h"
#include "config.h"
#include "rf24_tx.h"
#include "mqtt_io.h"

static TravelTime travel[ZONE_COUNT][MAX_BLINDS_PER_ZONE];
static BlindRuntime runtime[ZONE_COUNT][MAX_BLINDS_PER_ZONE];
static const ControllerConfig* activeController = nullptr;
static uint32_t lastRfStartMs = 0;
static const uint32_t RF_MIN_GAP_MS = 120;

static inline void logLine(const String& s) { if (DEBUG_LOG) Serial.println(s); }

const ZoneConfig* zoneConfig(Zone z) {
  if (z >= ZONE_COUNT) return nullptr;
  const ZoneConfig* config = &ZONE_CONFIGS[z];
  if (!config->name || config->blindCount == 0 || config->blindCount > MAX_BLINDS_PER_ZONE) return nullptr;
  return config;
}

Zone zoneFromName(const String& name) {
  for (Zone z = 0; z < ZONE_COUNT; z++) {
    const ZoneConfig* config = zoneConfig(z);
    if (config && name == config->name) return z;
  }
  return Z_UNKNOWN;
}

const char* controllerName() { return activeController ? activeController->name : "unconfigured"; }
bool controllerConfigured() { return activeController != nullptr; }

static void initRuntime(BlindRuntime* items) {
  for (uint8_t i = 0; i < MAX_BLINDS_PER_ZONE; i++) {
    items[i].state = S_OPEN; items[i].position = 50; items[i].startPos = 50; items[i].targetPos = 50;
    items[i].moveStartMs = 0; items[i].moveDurationMs = 0; items[i].moving = false;
    items[i].partialMove = false; items[i].openingDir = false; items[i].lastPubMs = 0;
  }
}

static BlindRuntime* getRt(Zone z, uint8_t blind) {
  const ZoneConfig* config = zoneConfig(z);
  if (!config || blind < 1 || blind > config->blindCount) return nullptr;
  return &runtime[z][blind - 1];
}

static TravelTime* getTravel(Zone z, uint8_t blind) {
  const ZoneConfig* config = zoneConfig(z);
  if (!config || blind < 1 || blind > config->blindCount) return nullptr;
  return &travel[z][blind - 1];
}

uint8_t blindToMask(uint8_t blind) { return (blind >= 1 && blind <= MAX_BLINDS_PER_ZONE) ? (1 << (blind - 1)) : 0; }

bool ownsZone(Zone z) {
  const ZoneConfig* config = zoneConfig(z);
  if (!activeController || !config) return false;
  for (uint8_t i = 0; i < activeController->zoneCount; i++) {
    if (activeController->zoneNames[i] && strcmp(activeController->zoneNames[i], config->name) == 0) return true;
  }
  return false;
}

void motionInit() {
  for (Zone z = 0; z < ZONE_COUNT; z++) {
    const ZoneConfig* config = zoneConfig(z);
    if (!config) { Serial.printf("[CONFIG] Invalid zone at index %u\n", z); continue; }
    for (uint8_t blind = 0; blind < MAX_BLINDS_PER_ZONE; blind++) {
      travel[z][blind] = { config->travel[blind].open_s, config->travel[blind].close_s };
    }
    initRuntime(runtime[z]);
  }

  String mac = WiFi.macAddress();
  for (size_t i = 0; i < CONTROLLER_COUNT; i++) {
    if (CONTROLLER_CONFIGS[i].mac && mac.equalsIgnoreCase(CONTROLLER_CONFIGS[i].mac)) { activeController = &CONTROLLER_CONFIGS[i]; break; }
  }
  if (!activeController) { Serial.println("[CONFIG] Unrecognized controller MAC; RF commands disabled"); return; }
  Serial.print("[CONFIG] Controller: "); Serial.println(activeController->name);
  Serial.println("[CONFIG] Owned zones:");
  for (uint8_t i = 0; i < activeController->zoneCount; i++) { Serial.print("  "); Serial.println(activeController->zoneNames[i]); }
}

static void publishState(Zone z, uint8_t blind, MoveState state, int pos) { mqttPublishState(z, blind, state, pos); }

static void rfSendSpaced(const ZoneConfig& config, uint8_t mask, bool open, bool longPress) {
  uint32_t elapsed = millis() - lastRfStartMs;
  if (elapsed < RF_MIN_GAP_MS) delay(RF_MIN_GAP_MS - elapsed);
  lastRfStartMs = millis();
  if (longPress) { rfSendCmd(config.remoteId, mask, open, 700); delay(200); rfSendCmd(config.remoteId, mask, open, 700); }
  else rfSendCmd(config.remoteId, mask, open, 500);
}

void startMove(Zone z, uint8_t blind, Action action, int targetPos) {
  const ZoneConfig* config = zoneConfig(z);
  if (!config || !ownsZone(z)) return;
  uint8_t mask = blindToMask(blind);
  BlindRuntime* rt = getRt(z, blind);
  TravelTime* tt = getTravel(z, blind);
  if (!rt || !tt || mask == 0) return;
  uint32_t now = millis();

  if (rt->moving) {
    bool requestedOpposite = false;
    if (action == A_OPEN) requestedOpposite = rt->state == S_CLOSING;
    else if (action == A_CLOSE) requestedOpposite = rt->state == S_OPENING;
    else if (action == A_SET_POS) {
      int target = constrain(targetPos, 0, 100);
      if (target <= SETPOS_MIN) target = 0; else if (target >= SETPOS_MAX) target = 100;
      if (target != rt->position) requestedOpposite = (target > rt->position) != (rt->state == S_OPENING);
    }
    if (requestedOpposite) { logLine(String("[MOVE] Opposite command while moving; stopping zone=") + config->name + " blind=" + blind); startMove(z, blind, A_STOP, 0); return; }
  }

  if (action == A_SET_POS) {
    int target = constrain(targetPos, 0, 100);
    if (target <= SETPOS_MIN) { action = A_CLOSE; target = 0; }
    else if (target >= SETPOS_MAX) { action = A_OPEN; target = 100; }
    else {
      int cur = rt->position;
      if (abs(target - cur) <= 1) return;
      bool opening = target > cur;
      uint32_t fullMs = (opening ? tt->open_s : tt->close_s) * 1000UL;
      uint32_t durMs = (fullMs * (uint32_t)abs(target - cur)) / 100UL;
      if (durMs < 300) durMs = 300;
      logLine(String("[MOVE] SET_POS zone=") + config->name + " blind=" + blind + " cur=" + cur + " tgt=" + target);
      rfSendSpaced(*config, mask, opening, true);
      rt->moving = true; rt->partialMove = true; rt->openingDir = opening; rt->state = opening ? S_OPENING : S_CLOSING;
      rt->startPos = cur; rt->targetPos = target; rt->moveStartMs = now; rt->moveDurationMs = durMs;
      publishState(z, blind, rt->state, rt->position); return;
    }
  }

  if (action == A_OPEN || action == A_CLOSE) {
    bool opening = action == A_OPEN;
    int cur = rt->position; int target = opening ? 100 : 0;
    uint32_t fullMs = (opening ? tt->open_s : tt->close_s) * 1000UL;
    uint32_t durMs = (fullMs * (uint32_t)abs(target - cur)) / 100UL;
    if (durMs < 300) durMs = 300;
    logLine(String("[MOVE] ") + (opening ? "OPEN" : "CLOSE") + " zone=" + config->name + " blind=" + blind);
    rfSendSpaced(*config, mask, opening, true);
    rt->moving = true; rt->partialMove = false; rt->openingDir = opening; rt->state = opening ? S_OPENING : S_CLOSING;
    rt->startPos = cur; rt->targetPos = target; rt->moveStartMs = now; rt->moveDurationMs = durMs;
    publishState(z, blind, rt->state, rt->position); return;
  }

  if (action == A_STOP && rt->moving) {
    uint32_t elapsed = now - rt->moveStartMs;
    float frac = rt->moveDurationMs ? (float)elapsed / rt->moveDurationMs : 0.0f;
    frac = constrain(frac, 0.0f, 1.0f);
    rt->position = constrain(rt->startPos + (int)((rt->targetPos - rt->startPos) * frac), 0, 100);
    bool wasOpening = rt->state == S_OPENING;
    logLine(String("[MOVE] STOP zone=") + config->name + " blind=" + blind + " pos=" + rt->position);
    rfSendSpaced(*config, mask, !wasOpening, false);
    rt->moving = false; rt->partialMove = false; rt->state = rt->position <= 0 ? S_CLOSED : S_OPEN;
    publishState(z, blind, rt->state, rt->position);
  }
}

static void tickOne(Zone z, uint8_t blind) {
  const ZoneConfig* config = zoneConfig(z);
  BlindRuntime* rt = getRt(z, blind);
  if (!config || !rt || !rt->moving) return;
  uint32_t now = millis(); uint32_t elapsed = now - rt->moveStartMs;
  float frac = rt->moveDurationMs ? (float)elapsed / rt->moveDurationMs : 1.0f;
  frac = constrain(frac, 0.0f, 1.0f);
  int pos = constrain(rt->startPos + (int)((rt->targetPos - rt->startPos) * frac), 0, 100);
  if (now - rt->lastPubMs > 500) { publishState(z, blind, rt->state, pos); rt->lastPubMs = now; }
  if (elapsed < rt->moveDurationMs) return;
  if (rt->partialMove) { logLine(String("[MOVE] Partial complete; stopping zone=") + config->name + " blind=" + blind); rfSendSpaced(*config, blindToMask(blind), !rt->openingDir, false); }
  rt->moving = false; rt->position = rt->targetPos; rt->state = rt->position <= 0 ? S_CLOSED : S_OPEN; rt->partialMove = false;
  publishState(z, blind, rt->state, rt->position);
}

void motionPublishAllStates() {
  Serial.println("[STATE] Publishing startup state");
  for (Zone z = 0; z < ZONE_COUNT; z++) {
    const ZoneConfig* config = zoneConfig(z);
    if (!config || !ownsZone(z)) continue;
    for (uint8_t blind = 1; blind <= config->blindCount; blind++) { BlindRuntime* rt = getRt(z, blind); if (rt) publishState(z, blind, rt->state, rt->position); }
  }
}

void tickAllMovement() {
  for (Zone z = 0; z < ZONE_COUNT; z++) {
    const ZoneConfig* config = zoneConfig(z);
    if (!config || !ownsZone(z)) continue;
    for (uint8_t blind = 1; blind <= config->blindCount; blind++) tickOne(z, blind);
  }
}
