#include <WiFi.h>
#include "config.h"
#include "firmware_info.h"
#include "http_api.h"
#include "motion.h"
#include "queue.h"
#include "command_validation.h"
#include "rf24_tx.h"

static inline void logLine(const String& s) {
  if (DEBUG_LOG) Serial.println(s);
}

static Zone parseZoneName(const String& s) {
  return zoneFromName(s);
}

static bool parseHttpAction(const String& s, Action& action) {
  if (s == "up" || s == "open") { action = A_OPEN; return true; }
  if (s == "down" || s == "close") { action = A_CLOSE; return true; }
  if (s == "stop") { action = A_STOP; return true; }
  return false;
}

static void handleRoot(WebServer& server) {
  String msg;
  msg += "Controller: ";
  msg += controllerName();
  msg += "\n";
  msg += "IP: " + WiFi.localIP().toString() + "\n";
  msg += "MAC: " + WiFi.macAddress() + "\n\n";
  msg += "Firmware: " + String(FIRMWARE_VERSION) + "\n\n";
  const ZoneConfig* firstOwnedZone = nullptr;
  if (!controllerConfigured()) {
    msg += "RF commands disabled: MAC is not configured\n";
  } else if (!rfOk()) {
    msg += "RF commands disabled: transmitter is unavailable\n";
  } else {
    msg += "Zones:\n";
    for (Zone z = 0; z < ZONE_COUNT; z++) {
      const ZoneConfig* config = zoneConfig(z);
      if (config && ownsZone(z)) {
        if (!firstOwnedZone) firstOwnedZone = config;
        msg += "- ";
        msg += config->name;
        msg += " (blinds 1-";
        msg += config->blindCount;
        msg += ")\n";
      }
    }
  }

  msg += "\nCommand format: /<zone>/<blind>/<command-or-position>\n";
  msg += "Commands: open, close, or stop. Position: whole number from 0 to 100.\n";
  msg += "\nExamples:\n";
  if (firstOwnedZone) {
    const String base = "http://" + WiFi.localIP().toString() + "/" + firstOwnedZone->name + "/1/";
    msg += base + "open\n";
    msg += base + "50\n";
  } else {
    msg += "http://<controller-ip>/<zone>/1/open\n";
    msg += "http://<controller-ip>/<zone>/1/50\n";
  }

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
  int blind = 0;
  if (!parseUnsignedDecimal(blindStr.c_str(), blindStr.length(), blind)) {
    server.send(404, "text/plain", "invalid zone or blind");
    return;
  }

  const ZoneConfig* config = zoneConfig(z);
  if (!config || blind < 1 || blind > config->blindCount) {
    server.send(404, "text/plain", "invalid zone or blind");
    return;
  }
  if (!ownsZone(z)) {
    server.send(403, "text/plain", "zone not owned by this controller");
    return;
  }
  if (!rfOk()) {
    server.send(503, "text/plain", "RF transmitter unavailable");
    return;
  }
  Cmd c;
  c.zone = z;
  c.blind = (uint8_t)blind;
  c.targetPos = -1;

  int position = 0;
  bool isNum = parseUnsignedDecimal(cmdStr.c_str(), cmdStr.length(), position);

  if (isNum) {
    if (position > 100) {
      server.send(400, "text/plain", "position must be 0..100");
      return;
    }
    c.action = A_SET_POS;
    c.targetPos = position;
    logLine("[HTTP] RX " + uri + " -> SET_POS " + String(position));
  } else {
    if (!parseHttpAction(cmdStr, c.action)) {
      server.send(400, "text/plain", "command must be up, down, stop, or a position from 0 to 100");
      return;
    }
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
