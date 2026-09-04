#include <WiFi.h>
#include "config.h"
#include "http_api.h"
#include "motion.h"
#include "queue.h"

static inline void logLine(const String& s) {
  if (DEBUG_LOG) Serial.println(s);
}

static Zone parseZoneName(const String& s) {
  if (s == "front") return Z_FRONT;
  if (s == "kitchen") return Z_KITCHEN;
  if (s == "back") return Z_BACK;
  return Z_UNKNOWN;
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

  if (WiFi.macAddress() == FRONT_ESP_MAC) {
    msg += "Zone: front\n";
  } else if (WiFi.macAddress() == BACK_ESP_MAC) {
    msg += "Zone: back\n";
  } else {
    msg += "Zone: unknown\n";
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

  if (z == Z_UNKNOWN || blind < 1 || blind > 4) {
    server.send(404, "text/plain", "invalid zone or blind");
    return;
  }
  if (!ownsZone(z)) {
    server.send(403, "text/plain", "zone not owned by this controller");
    return;
  }
  if (z == Z_KITCHEN && blind != 1) {
    server.send(404, "text/plain", "kitchen only has blind 1");
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
