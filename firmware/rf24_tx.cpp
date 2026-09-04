#include <SPI.h>
#include "config.h"
#include "rf24_tx.h"

static RF24 radio(RF_CE_PIN, RF_CS_PIN);
static bool rf_ready = false;

static inline void logLine(const String& s) {
  if (DEBUG_LOG) Serial.println(s);
}

bool rfOk() { return rf_ready; }

void rfInit() {
  // ESP32: include SS pin for completeness
  SPI.begin(RF24_SCK, RF24_MISO, RF24_MOSI, RF_CS_PIN);

  if (!radio.begin()) {
    rf_ready = false;
    return;
  }

  // Core radio config
  radio.stopListening();
  radio.setAutoAck(false);
  radio.setRetries(0, 0);

  radio.setAddressWidth(3);
  radio.openWritingPipe(RF_ADDR);

  radio.setPayloadSize(5);
  radio.disableDynamicPayloads();
  radio.setCRCLength(RF24_CRC_16);

  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_MAX);

  radio.setChannel(RF_CHANNEL_0);

  // Clean TX state
  radio.flush_tx();
  radio.txStandBy();

  rf_ready = true;
  logLine("RF24 initialised");
}

static inline void sendTrain(uint8_t ch, const uint8_t* pkt, int count, int& ok, int& fail) {
  radio.setChannel(ch);
  delayMicroseconds(RF_CH_SETTLE_US);

  for (int i = 0; i < count; i++) {
    bool r = radio.writeFast(pkt, 5);
    if (r) ok++; else fail++;
    delayMicroseconds(RF_TRAIN_DELAY_US);
  }

  // Ensure TX FIFO drained before next channel
  radio.txStandBy();
}

void rfSendCmd(const uint8_t remoteId[2], uint8_t blindMask, bool open, int durationMs) {
  if (!rf_ready) return;
  if (!remoteId) return;
  if (durationMs <= 0) return;

  const char* cmdStr = open ? "OPEN" : "CLOSE";

  logLine(String("[RF] TX remote=0x") + String(remoteId[0], HEX) + String(remoteId[1], HEX) +
          " mask=0x" + String(blindMask, HEX) +
          " cmd=" + cmdStr +
          " durationMs=" + durationMs +
          " trains=" + RF_TRAIN_PACKETS_0 + "/" + RF_TRAIN_PACKETS_1 + "/" + RF_TRAIN_PACKETS_2 +
          " dlyUs=" + RF_TRAIN_DELAY_US +
          " gapMs=" + RF_SEND_GAP_MS);

  radio.stopListening();
  radio.setPayloadSize(5);
  radio.setCRCLength(RF24_CRC_16);
  radio.flush_tx();

  uint8_t pkt[5] = {0x00, 0x00, 0x20, 0x00, 0x00};

  pkt[0] = remoteId[0];
  pkt[1] = remoteId[1];
  pkt[3] = blindMask;
  pkt[4] = open ? 0xA1 : 0xA2;

  int ok = 0, fail = 0;

  const uint32_t tStart = millis();
  int cyclesDone = 0;

  while ((int)(millis() - tStart) < durationMs) {
    // One "press cycle" = hop across 3 channels with configured trains
    sendTrain(RF_HOP_ORDER[0], pkt, RF_TRAIN_PACKETS_0, ok, fail);
    if ((int)(millis() - tStart) >= durationMs) break;

    sendTrain(RF_HOP_ORDER[1], pkt, RF_TRAIN_PACKETS_1, ok, fail);
    if ((int)(millis() - tStart) >= durationMs) break;

    sendTrain(RF_HOP_ORDER[2], pkt, RF_TRAIN_PACKETS_2, ok, fail);
    cyclesDone++;

    // Optional gap between cycles (helps match "held press" timing)
    if (RF_SEND_GAP_MS > 0) {
      // Don't oversleep past the deadline
      int remaining = durationMs - (int)(millis() - tStart);
      if (remaining <= 0) break;
      delay((RF_SEND_GAP_MS < remaining) ? RF_SEND_GAP_MS : remaining);
    }

    // Keep WiFi/RTOS healthy (and watchdog happy)
    delay(0);
  }

  radio.flush_tx();

  logLine(String("[RF] done durationMs=") + (millis() - tStart) +
          " cycles=" + cyclesDone +
          " ok=" + ok + " fail=" + fail);
}
