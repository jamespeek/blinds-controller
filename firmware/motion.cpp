#include <WiFi.h>
#include "motion.h"
#include "config.h"
#include "rf24_tx.h"
#include "mqtt_io.h"

static TravelTime travel_front[4];
static TravelTime travel_kitchen[4];
static TravelTime travel_back[4];

static BlindRuntime rt_front[4];
static BlindRuntime rt_kitchen[4];
static BlindRuntime rt_back[4];

static bool OWN_FRONT = true;
static bool OWN_KITCHEN = true;
static bool OWN_BACK = true;

static uint32_t lastRfStartMs = 0;
static const uint32_t RF_MIN_GAP_MS = 120; // spacing between RF starts (non-blocking)

static inline void logLine(const String& s) {
  if (DEBUG_LOG) Serial.println(s);
}

static void initRtArray(BlindRuntime* a) {
  for (int i = 0; i < 4; i++) {
    a[i].state = S_OPEN;
    a[i].position = 50;
    a[i].startPos = 50;
    a[i].targetPos = 50;
    a[i].moveStartMs = 0;
    a[i].moveDurationMs = 0;
    a[i].moving = false;
    a[i].partialMove = false;
    a[i].openingDir = false;
    a[i].lastPubMs = 0; // ✅ per-blind throttle
  }
}

static BlindRuntime* getRt(Zone z, uint8_t blind) {
  if (blind < 1 || blind > 4) return nullptr;
  if (z == Z_FRONT) return &rt_front[blind-1];
  if (z == Z_KITCHEN) return &rt_kitchen[blind-1];
  if (z == Z_BACK) return &rt_back[blind-1];
  return nullptr;
}

static TravelTime* getTravel(Zone z, uint8_t blind) {
  if (blind < 1 || blind > 4) return nullptr;
  if (z == Z_FRONT) return &travel_front[blind-1];
  if (z == Z_KITCHEN) return &travel_kitchen[blind-1];
  if (z == Z_BACK) return &travel_back[blind-1];
  return nullptr;
}

uint8_t blindToMask(uint8_t blind) {
  switch (blind) {
    case 1: return 0x01;
    case 2: return 0x02;
    case 3: return 0x04;
    case 4: return 0x08;
    default: return 0x00;
  }
}

int zoneToRemote(Zone z) {
  if (z == Z_FRONT) return 1;
  if (z == Z_KITCHEN) return 2;
  if (z == Z_BACK) return 3;
  return -1;
}

bool ownsZone(Zone z) {
  if (z == Z_FRONT) return OWN_FRONT;
  if (z == Z_KITCHEN) return OWN_KITCHEN;
  if (z == Z_BACK) return OWN_BACK;
  return false;
}

void motionInit() {
  // copy config travel times
  for (int i=0;i<4;i++) {
    travel_front[i]   = {(uint16_t)TRAVEL_FRONT[i].open_s,   (uint16_t)TRAVEL_FRONT[i].close_s};
    travel_kitchen[i] = {(uint16_t)TRAVEL_KITCHEN[i].open_s, (uint16_t)TRAVEL_KITCHEN[i].close_s};
    travel_back[i]    = {(uint16_t)TRAVEL_BACK[i].open_s,    (uint16_t)TRAVEL_BACK[i].close_s};
  }

  initRtArray(rt_front);
  initRtArray(rt_kitchen);
  initRtArray(rt_back);

  String mac = WiFi.macAddress();
  if (mac == FRONT_ESP_MAC) {
    OWN_FRONT = true; OWN_KITCHEN = true; OWN_BACK = false;
  } else if (mac == BACK_ESP_MAC) {
    OWN_FRONT = false; OWN_KITCHEN = false; OWN_BACK = true;
  } else {
    OWN_FRONT = true; OWN_KITCHEN = true; OWN_BACK = true;
  }

  logLine("Zone ownership:");
  logLine(String("  Front:   ") + (OWN_FRONT ? "YES" : "NO"));
  logLine(String("  Kitchen: ") + (OWN_KITCHEN ? "YES" : "NO"));
  logLine(String("  Back:    ") + (OWN_BACK ? "YES" : "NO"));
}

static void publishState(Zone z, uint8_t blind, MoveState s, int pos) {
  mqttPublishState(z, blind, s, pos);
}

static void rfSendSpaced(int remote, uint8_t mask, bool open, bool longPress) {
  uint32_t now = millis();

  uint32_t elapsed = now - lastRfStartMs;
  if (elapsed < RF_MIN_GAP_MS) {
    delay(RF_MIN_GAP_MS - elapsed);
  }

  lastRfStartMs = millis();
 
  if (longPress) {
    rfSendCmd(remote, mask, open, 700);
    delay(200);
    rfSendCmd(remote, mask, open, 700);
  } else {
    rfSendCmd(remote, mask, open, 500);
  }
}

void startMove(Zone z, uint8_t blind, Action a, int targetPos) {
  if (!ownsZone(z)) return;
  if (z == Z_KITCHEN && blind != 1) return;

  int remote = zoneToRemote(z);
  uint8_t mask = blindToMask(blind);
  BlindRuntime* rt = getRt(z, blind);
  TravelTime* tt = getTravel(z, blind);
  if (!rt || !tt || remote < 0 || mask == 0) return;

  uint32_t now = millis();

  // If already moving and user requests the opposite direction,
  // behave like the remote: first opposite press = STOP only.
  if (rt->moving) {
    bool requestedOpposite = false;

    if (a == A_OPEN) {
      requestedOpposite = (rt->state == S_CLOSING);   // moving down, got open
    } else if (a == A_CLOSE) {
      requestedOpposite = (rt->state == S_OPENING);   // moving up, got close
    } else if (a == A_SET_POS) {
      int target = constrain(targetPos, 0, 100);

      // Apply your clamp rule so setpos maps to open/close in the edges
      if (target <= SETPOS_MIN) target = 0;
      else if (target >= SETPOS_MAX) target = 100;

      // If target implies opposite direction relative to current *movement*
      if (target != rt->position) {
        bool wantOpen = (target > rt->position);
        bool movingOpen = (rt->state == S_OPENING);
        requestedOpposite = (wantOpen != movingOpen);
      }
    }

    if (requestedOpposite) {
      logLine(String("[MOVE] Opposite cmd while moving -> STOP only zone=") +
              ZONE_NAMES[z] + " blind=" + blind);

      // Call STOP branch (updates position estimate, sends opposite burst, publishes state)
      startMove(z, blind, A_STOP, 0);
      return;
    }
  }

  // SET_POS clamp rule
  if (a == A_SET_POS) {
    int target = constrain(targetPos, 0, 100);

    if (target <= SETPOS_MIN) { a = A_CLOSE; target = 0; }
    else if (target >= SETPOS_MAX) { a = A_OPEN; target = 100; }
    else {
      int cur = rt->position;
      if (abs(target - cur) <= 1) return;

      bool opening = (target > cur);
      uint32_t fullMs = (opening ? tt->open_s : tt->close_s) * 1000UL;
      uint32_t span = (uint32_t)abs(target - cur);
      uint32_t durMs = (fullMs * span) / 100UL;
      if (durMs < 300) durMs = 300;

      logLine(String("[MOVE] SET_POS zone=") + ZONE_NAMES[z] +
              " blind=" + blind +
              " cur=" + cur +
              " tgt=" + target +
              " dir=" + (opening ? "OPEN" : "CLOSE") +
              " dur=" + String(durMs/1000.0f, 2) + "s");

      rfSendSpaced(remote, mask, opening, true);

      rt->moving = true;
      rt->partialMove = true;
      rt->openingDir = opening;
      rt->state = opening ? S_OPENING : S_CLOSING;
      rt->startPos = cur;
      rt->targetPos = target;
      rt->moveStartMs = now;
      rt->moveDurationMs = durMs;

      publishState(z, blind, rt->state, rt->position);
      return;
    }
  }

  if (a == A_OPEN || a == A_CLOSE) {
    bool opening = (a == A_OPEN);
    int cur = rt->position;
    int tgt = opening ? 100 : 0;

    uint32_t fullMs = (opening ? tt->open_s : tt->close_s) * 1000UL;
    uint32_t span = (uint32_t)abs(tgt - cur);
    uint32_t durMs = (fullMs * span) / 100UL;
    if (durMs < 300) durMs = 300;

    logLine(String("[MOVE] ") + (opening ? "OPEN" : "CLOSE") +
            " zone=" + ZONE_NAMES[z] +
            " blind=" + blind +
            " cur=" + cur +
            " tgt=" + tgt +
            " dur=" + String(durMs/1000.0f, 2) + "s");

    rfSendSpaced(remote, mask, opening, true);

    rt->moving = true;
    rt->partialMove = false; // ✅ full moves do not send stop burst
    rt->openingDir = opening;
    rt->state = opening ? S_OPENING : S_CLOSING;
    rt->startPos = cur;
    rt->targetPos = tgt;
    rt->moveStartMs = now;
    rt->moveDurationMs = durMs;

    publishState(z, blind, rt->state, rt->position);
    return;
  }

  if (a == A_STOP && rt->moving) {
    uint32_t elapsed = now - rt->moveStartMs;
    float frac = rt->moveDurationMs ? (float)elapsed / (float)rt->moveDurationMs : 0.0f;
    frac = constrain(frac, 0.0f, 1.0f);

    int newPos = rt->startPos + (int)((rt->targetPos - rt->startPos) * frac);
    rt->position = constrain(newPos, 0, 100);

    bool wasOpening = (rt->state == S_OPENING);
    logLine(String("[MOVE] STOP zone=") + ZONE_NAMES[z] + " blind=" + blind + " pos=" + rt->position);

    rfSendSpaced(remote, mask, !wasOpening, false);

    rt->moving = false;
    rt->partialMove = false;
    rt->state = (rt->position <= 0) ? S_CLOSED : S_OPEN;

    publishState(z, blind, rt->state, rt->position);
  }
}

static void tickOne(Zone z, uint8_t blind) {
  BlindRuntime* rt = getRt(z, blind);
  if (!rt || !rt->moving) return;

  uint32_t now = millis();
  uint32_t elapsed = now - rt->moveStartMs;

  float frac = rt->moveDurationMs ? (float)elapsed / (float)rt->moveDurationMs : 1.0f;
  frac = constrain(frac, 0.0f, 1.0f);

  int pos = rt->startPos + (int)((rt->targetPos - rt->startPos) * frac);
  pos = constrain(pos, 0, 100);

  // ✅ per-blind publish throttle (fix for "other blind catches up")
  if (now - rt->lastPubMs > 500) {
    publishState(z, blind, rt->state, pos);
    rt->lastPubMs = now;
  }

  if (elapsed >= rt->moveDurationMs) {
    int remote = zoneToRemote(z);
    uint8_t mask = blindToMask(blind);

    if (rt->partialMove) {
      logLine(String("[MOVE] Partial complete -> STOP burst zone=") + ZONE_NAMES[z] + " blind=" + blind);
      rfSendSpaced(remote, mask, !rt->openingDir, false);
    } else {
      logLine(String("[MOVE] Full complete zone=") + ZONE_NAMES[z] + " blind=" + blind);
    }

    rt->moving = false;
    rt->position = rt->targetPos;

    if (rt->position <= 0) rt->state = S_CLOSED;
    else if (rt->position >= 100) rt->state = S_OPEN;
    else rt->state = S_OPEN;

    rt->partialMove = false;
    publishState(z, blind, rt->state, rt->position);
  }
}

void motionPublishAllStates() {
  Serial.println("[STATE] Publishing startup state");

  for (int b = 1; b <= 4; b++) {
    if (ownsZone(Z_FRONT)) {
      BlindRuntime* rt = getRt(Z_FRONT, b);
      if (rt) publishState(Z_FRONT, b, rt->state, rt->position);
    }

    if (ownsZone(Z_BACK)) {
      BlindRuntime* rt = getRt(Z_BACK, b);
      if (rt) publishState(Z_BACK, b, rt->state, rt->position);
    }
  }

  // kitchen only blind 1
  if (ownsZone(Z_KITCHEN)) {
    BlindRuntime* rt = getRt(Z_KITCHEN, 1);
    if (rt) publishState(Z_KITCHEN, 1, rt->state, rt->position);
  }
}

void tickAllMovement() {
  for (int i = 1; i <= 4; i++) {
    tickOne(Z_FRONT, i);
    tickOne(Z_KITCHEN, i);
    tickOne(Z_BACK, i);
  }
}
