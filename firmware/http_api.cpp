#include <WiFi.h>
#include "config.h"
#include "http_api.h"
#include "motion.h"
#include "queue.h"

static inline void logLine(const String& s) {
  if (DEBUG_LOG) Serial.println(s);
}

static Zone parseZoneName(const String& s) {
  return zoneFromName(s);
}

static Action parseHttpAction(const String& s) {
  if (s == "up" || s == "open") return A_OPEN;
  if (s == "down" || s == "close") return A_CLOSE;
  if (s == "stop") return A_STOP;
  return A_STOP;
}

static void handleRoot(WebServer& server) {
  String msg;
  msg += "Blind controller online\n\n";

  msg += "IP: " + WiFi.localIP().toString() + "\n";
  msg += "MAC: " + WiFi.macAddress() + "\n\n";

  msg += "Controller: ";
  msg += controllerName();
  msg += "\n";
  if (controllerConfigured()) {
    msg += "Owned zones: ";
    bool first = true;
    for (Zone z = 0; z < ZONE_COUNT; z++) {
      const ZoneConfig* config = zoneConfig(z);
      if (config && ownsZone(z)) {
        if (!first) msg += ", ";
        msg += config->name;
        first = false;
      }
    }
    msg += "\n";
  } else {
    msg += "RF commands disabled: MAC is not configured\n";
  }

  msg += "\nCommand format: /<zone>/<blind>/<cmd-or-pos>\n";

  server.send(200, "text/plain", msg);
}

static void handleApi(WebServer& server) {
  String uri = server.uri(); // /front/1/up OR /front/1/50
  int p1 = uri.indexOf('/', 1);
  int p2 = (p1 >= 0) ? uri.indexOf('/', p1 + 1) : -1;
  if (p1 < 0 || p2 < 0) {
    server.send(404, "text/plain", "expected /<zone>/<blind>/<cmd-or-pos>");
    return;
  }

  String zoneStr = uri.substring(1, p1);
  String blindStr = uri.substring(p1 + 1, p2);
  String cmdStr = uri.substring(p2 + 1);

  Zone z = parseZoneName(zoneStr);
  int blind = blindStr.toInt();

  const ZoneConfig* config = zoneConfig(z);
  if (!config || blind < 1 || blind > config->blindCount) {
    server.send(404, "text/plain", "invalid zone or blind");
    return;
  }
  if (!ownsZone(z)) {
    server.send(403, "text/plain", "zone not owned by this controller");
    return;
  }
  Cmd c;
  c.zone = z;
  c.blind = (uint8_t)blind;
  c.targetPos = -1;

  bool isNum = cmdStr.length() > 0;
  for (int i = 0; i < (int)cmdStr.length(); i++) {
    if (!isDigit(cmdStr[i])) { isNum = false; break; }
  }

  if (isNum) {
    int v = cmdStr.toInt();
    if (v < 0 || v > 100) {
      server.send(400, "text/plain", "position must be 0..100");
      return;
    }
    c.action = A_SET_POS;
    c.targetPos = v;
    logLine("[HTTP] RX " + uri + " -> SET_POS " + String(v));
  } else {
    c.action = parseHttpAction(cmdStr);
    logLine("[HTTP] RX " + uri + " -> " + cmdStr);
  }

  if (!qEnqueue(c)) {
    server.send(503, "text/plain", "queue full");
    return;
  }

  server.send(200, "text/plain", "OK\n");
}

void httpInit(WebServer& server) {
  server.on("/", HTTP_GET, [&server](){ handleRoot(server); });
  server.onNotFound([&server](){ handleApi(server); });
  server.begin();
  if (DEBUG_LOG) Serial.println("HTTP server started");
}

void httpLoop(WebServer& server) {
  server.handleClient();
}
