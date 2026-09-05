#include <WiFi.h>
#include <cstring>

#include "motion.h"
#include "config.h"
#include "rf24_tx.h"
#include "mqtt_io.h"
#include "post_stop_guard.h"
#include "rf_command_gate.h"
#include "remote_motion.h"
#include "stop_retry.h"

static TravelTime travel[ZONE_COUNT][MAX_BLINDS_PER_ZONE];
static BlindRuntime runtime[ZONE_COUNT][MAX_BLINDS_PER_ZONE];
static const ControllerConfig* activeController = nullptr;
static uint32_t lastRfStartMs = 0;
static const uint32_t RF_MIN_GAP_MS = 120;

static inline void logLine(const String& s) { if (DEBUG_LOG) Serial.println(s); }

const ZoneConfig* zoneConfig(Zone z) {
  if (z >= ZONE_COUNT) return nullptr;
  const ZoneConfig* config = &ZONE_CONFIGS[z];
  if (!config->name || config->name[0] == '\0' || config->blindCount == 0 || config->blindCount > MAX_BLINDS_PER_ZONE) return nullptr;
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
    items[i].partialMove = false; items[i].openingDir = false; items[i].hasMotionDirection = false; items[i].lastPubMs = 0;
    items[i].postStopAction = A_STOP; items[i].postStopTargetPos = -1; items[i].postStopUntilMs = 0;
    items[i].lastObservedAction = A_STOP; items[i].lastObservedMs = 0;
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

static bool invalidTopology(const String& reason) {
  Serial.println(String("[CONFIG] Invalid topology: ") + reason);
  return false;
}

static bool validateTopology() {
  bool claimed[ZONE_COUNT] = {};
  for (Zone z = 0; z < ZONE_COUNT; z++) {
    const ZoneConfig* zone = zoneConfig(z);
    if (!zone) return invalidTopology(String("invalid zone at index ") + String(z));
    for (Zone earlier = 0; earlier < z; earlier++) {
      const ZoneConfig* other = zoneConfig(earlier);
      if (other && strcmp(zone->name, other->name) == 0) {
        return invalidTopology(String("duplicate zone name: ") + zone->name);
      }
    }
    for (uint8_t blind = 0; blind < zone->blindCount; blind++) {
      if (zone->travel[blind].open_s == 0 || zone->travel[blind].close_s == 0) {
        return invalidTopology(String("zero travel time for ") + zone->name + " blind " + String(blind + 1));
      }
    }
  }

  for (size_t i = 0; i < CONTROLLER_COUNT; i++) {
    const ControllerConfig& controller = CONTROLLER_CONFIGS[i];
    if (!controller.mac || controller.mac[0] == '\0' || !controller.name || controller.name[0] == '\0') {
      return invalidTopology(String("controller ") + String(i) + " needs a MAC and name");
    }
    if (!controller.zoneNames || controller.zoneCount == 0) {
      return invalidTopology(String("controller ") + controller.name + " owns no zones");
    }
    for (size_t earlier = 0; earlier < i; earlier++) {
      const char* earlierMac = CONTROLLER_CONFIGS[earlier].mac;
      if (earlierMac && String(controller.mac).equalsIgnoreCase(earlierMac)) {
        return invalidTopology(String("duplicate controller MAC: ") + controller.mac);
      }
    }
    for (uint8_t n = 0; n < controller.zoneCount; n++) {
      const char* name = controller.zoneNames[n];
      if (!name || name[0] == '\0') return invalidTopology(String("empty zone for controller ") + controller.name);
      Zone z = zoneFromName(name);
      if (z == Z_UNKNOWN) return invalidTopology(String("unknown zone ") + name + " for controller " + controller.name);
      if (claimed[z]) return invalidTopology(String("zone owned more than once: ") + name);
      claimed[z] = true;
    }
  }
  for (Zone z = 0; z < ZONE_COUNT; z++) {
    if (!claimed[z]) return invalidTopology(String("zone has no controller: ") + zoneConfig(z)->name);
  }
  return true;
}

void motionInit() {
  activeController = nullptr;
  if (!validateTopology()) {
    Serial.println("[CONFIG] RF commands disabled until topology is corrected");
    return;
  }
  for (Zone z = 0; z < ZONE_COUNT; z++) {
    const ZoneConfig* config = zoneConfig(z);
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

static void updateEstimatedPosition(BlindRuntime& rt, uint32_t now) {
  const uint32_t elapsed = now - rt.moveStartMs;
  const float fraction = rt.moveDurationMs ? (float)elapsed / rt.moveDurationMs : 0.0f;
  rt.position = constrain(rt.startPos + (int)((rt.targetPos - rt.startPos) * constrain(fraction, 0.0f, 1.0f)), 0, 100);
}

static void observeRemoteDirection(Zone z, uint8_t blind, bool opening) {
  const ZoneConfig* config = zoneConfig(z);
  BlindRuntime* rt = getRt(z, blind);
  TravelTime* tt = getTravel(z, blind);
  if (!config || !rt || !tt) return;

  const uint32_t now = millis();
  const Action action = opening ? A_OPEN : A_CLOSE;
  if (rt->lastObservedAction == action && now - rt->lastObservedMs < RF_REMOTE_DEDUP_MS) return;
  rt->lastObservedAction = action;
  rt->lastObservedMs = now;

  const bool movingUp = rt->state == S_OPENING;
  switch (remoteMotionEffect(rt->moving, movingUp, opening)) {
    case RemoteMotionEffect::Continue:
      return;
    case RemoteMotionEffect::Stop:
      updateEstimatedPosition(*rt, now);
      rt->moving = false;
      rt->partialMove = false;
      rt->state = rt->position <= 0 ? S_CLOSED : S_OPEN;
      Serial.println(String("[RF RX] Remote STOP zone=") + config->name + " blind=" + blind + " pos=" + rt->position);
      publishState(z, blind, rt->state, rt->position);
      return;
    case RemoteMotionEffect::Start:
      break;
  }

  const int target = opening ? 100 : 0;
  if (rt->position == target) return;
  const uint32_t fullMs = (opening ? tt->open_s : tt->close_s) * 1000UL;
  uint32_t durationMs = (fullMs * (uint32_t)abs(target - rt->position)) / 100UL;
  if (durationMs < 300) durationMs = 300;
  rt->moving = true;
  rt->partialMove = false;
  rt->openingDir = opening;
  rt->hasMotionDirection = true;
  rt->state = opening ? S_OPENING : S_CLOSING;
  rt->startPos = rt->position;
  rt->targetPos = target;
  rt->moveStartMs = now;
  rt->moveDurationMs = durationMs;
  Serial.println(String("[RF RX] Remote ") + (opening ? "OPEN" : "CLOSE") + " zone=" + config->name + " blind=" + blind);
  publishState(z, blind, rt->state, rt->position);
}

void motionObserveRemoteFrame(const RemoteRfFrame& frame) {
  for (Zone z = 0; z < ZONE_COUNT; z++) {
    const ZoneConfig* config = zoneConfig(z);
    if (!config || !ownsZone(z)) continue;
    if (config->remoteId[0] != frame.remoteId[0] || config->remoteId[1] != frame.remoteId[1]) continue;
    for (uint8_t blind = 1; blind <= config->blindCount; blind++) {
      if ((frame.blindMask & blindToMask(blind)) == 0) continue;
      observeRemoteDirection(z, blind, frame.direction == RemoteDirection::Up);
    }
  }
}

static void rfSendSpaced(const ZoneConfig& config, uint8_t mask, bool open, RfProfile profile) {
  uint32_t elapsed = millis() - lastRfStartMs;
  if (elapsed < RF_MIN_GAP_MS) delay(RF_MIN_GAP_MS - elapsed);
  lastRfStartMs = millis();
  const uint8_t gestureCount = rfGestureCount(profile, RF_START_GESTURE_COUNT);
  for (uint8_t i = 0; i < gestureCount; i++) {
    rfSendCmd(config.remoteId, mask, open, profile);
    if (i + 1 < gestureCount) delay(RF_START_GESTURE_GAP_MS);
  }
}

void startMove(Zone z, uint8_t blind, Action action, int targetPos) {
  const ZoneConfig* config = zoneConfig(z);
  if (!config || !canTransmitCommand(rfOk(), ownsZone(z))) return;
  uint8_t mask = blindToMask(blind);
  BlindRuntime* rt = getRt(z, blind);
  TravelTime* tt = getTravel(z, blind);
  if (!rt || !tt || mask == 0) return;
  uint32_t now = millis();

  if (isPostStopDuplicate(action, targetPos, rt->postStopAction, rt->postStopTargetPos,
                          now, rt->postStopUntilMs)) {
    logLine(String("[MOVE] Ignoring duplicate command after stop zone=") + config->name + " blind=" + blind);
    return;
  }

  if (rt->moving) {
    bool requestedOpposite = false;
    if (action == A_OPEN) requestedOpposite = rt->state == S_CLOSING;
    else if (action == A_CLOSE) requestedOpposite = rt->state == S_OPENING;
    else if (action == A_SET_POS) {
      int target = constrain(targetPos, 0, 100);
      if (target <= SETPOS_MIN) target = 0; else if (target >= SETPOS_MAX) target = 100;
      if (target != rt->position) requestedOpposite = (target > rt->position) != (rt->state == S_OPENING);
    }
    if (requestedOpposite) {
      logLine(String("[MOVE] Opposite command while moving; stopping zone=") + config->name + " blind=" + blind);
      rt->postStopAction = action;
      rt->postStopTargetPos = targetPos;
      rt->postStopUntilMs = now + POST_STOP_DEDUP_MS;
      startMove(z, blind, A_STOP, 0);
      return;
    }
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
      rfSendSpaced(*config, mask, opening, RfProfile::Start);
      rt->moving = true; rt->partialMove = true; rt->openingDir = opening; rt->hasMotionDirection = true; rt->state = opening ? S_OPENING : S_CLOSING;
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
    rfSendSpaced(*config, mask, opening, RfProfile::Start);
    rt->moving = true; rt->partialMove = false; rt->openingDir = opening; rt->hasMotionDirection = true; rt->state = opening ? S_OPENING : S_CLOSING;
    rt->startPos = cur; rt->targetPos = target; rt->moveStartMs = now; rt->moveDurationMs = durMs;
    publishState(z, blind, rt->state, rt->position); return;
  }

  if (action == A_STOP) {
    const StopRfDirection direction = stopRfDirection(rt->moving, rt->state == S_OPENING,
                                                       rt->hasMotionDirection, rt->openingDir);
    if (!direction.shouldSend) {
      logLine(String("[MOVE] Ignoring STOP without a known direction zone=") + config->name + " blind=" + blind);
      return;
    }
    if (rt->moving) {
      uint32_t elapsed = now - rt->moveStartMs;
      float frac = rt->moveDurationMs ? (float)elapsed / rt->moveDurationMs : 0.0f;
      frac = constrain(frac, 0.0f, 1.0f);
      rt->position = constrain(rt->startPos + (int)((rt->targetPos - rt->startPos) * frac), 0, 100);
      logLine(String("[MOVE] STOP zone=") + config->name + " blind=" + blind + " pos=" + rt->position);
    } else {
      logLine(String("[MOVE] Retrying STOP zone=") + config->name + " blind=" + blind);
    }
    rfSendSpaced(*config, mask, direction.opening, RfProfile::Stop);
    if (rt->moving) {
      rt->moving = false; rt->partialMove = false; rt->state = rt->position <= 0 ? S_CLOSED : S_OPEN;
      publishState(z, blind, rt->state, rt->position);
    }
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
  if (rt->partialMove) { logLine(String("[MOVE] Partial complete; stopping zone=") + config->name + " blind=" + blind); rfSendSpaced(*config, blindToMask(blind), !rt->openingDir, RfProfile::Stop); }
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
