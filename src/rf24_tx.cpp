#include <SPI.h>
#include "config.h"
#include "rf_gesture.h"
#include "rf24_tx.h"

static RF24 radio(RF_CE_PIN, RF_CS_PIN);
static bool rf_ready = false;
static uint32_t rfSequence = 0;
static uint8_t listenChannelIndex = 0;
static uint32_t listenChannelStartedMs = 0;

static inline void logLine(const String& s) {
  if (DEBUG_LOG || RF_DRY_RUN) Serial.println(s);
}

bool rfOk() { return rf_ready; }

static void startListening() {
  radio.setChannel(RF_HOP_ORDER[listenChannelIndex]);
  radio.startListening();
  listenChannelStartedMs = millis();
}

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
  radio.openReadingPipe(1, RF_ADDR);

  radio.setPayloadSize(5);
  radio.disableDynamicPayloads();
  radio.setCRCLength(RF24_CRC_16);

  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_MAX);

  // Clean radio state, then remain passively listening until a command needs
  // transmission.
  radio.flush_tx();
  radio.flush_rx();
  startListening();

  rf_ready = true;
  logLine("RF24 initialised");
}

bool rfReceivePacket(uint8_t packet[5]) {
  if (!rf_ready || !packet) return false;

  const uint32_t now = millis();
  if (now - listenChannelStartedMs >= RF_LISTEN_CHANNEL_DWELL_MS) {
    radio.stopListening();
    listenChannelIndex = (listenChannelIndex + 1) % 3;
    startListening();
  }

  uint8_t pipe = 0;
  if (!radio.available(&pipe)) return false;
  radio.read(packet, 5);
  return true;
}

static inline void sendPacketOnChannel(uint8_t ch, const uint8_t* pkt, int& ok, int& fail) {
  radio.setChannel(ch);
  delayMicroseconds(RF_CH_SETTLE_US);

  bool r = radio.writeFast(pkt, 5);
  if (r) ok++; else fail++;

  // Ensure the single packet is sent before changing channel.
  radio.txStandBy();
}

static int sendForDuration(const uint8_t* pkt, uint16_t durationMs, int& ok, int& fail) {
  const uint32_t startedAt = millis();
  int cycles = 0;
  while ((uint32_t)(millis() - startedAt) < durationMs) {
    // The remote round-robins a packet across every channel without a
    // dwell train or an inter-cycle pause.
    sendPacketOnChannel(RF_HOP_ORDER[0], pkt, ok, fail);
    sendPacketOnChannel(RF_HOP_ORDER[1], pkt, ok, fail);
    sendPacketOnChannel(RF_HOP_ORDER[2], pkt, ok, fail);
    cycles++;
    delay(0);
  }
  return cycles;
}

void rfSendCmd(const uint8_t remoteId[2], uint8_t blindMask, bool open, RfProfile profile) {
  if (!rf_ready) return;
  if (!remoteId) return;
  const RfProfilePlan plan = rfProfilePlan(profile, RF_START_ACTIVE_MS, RF_START_RELEASE_MS,
                                           RF_STOP_ACTIVE_MS, RF_STOP_RELEASE_MS);
  if (plan.activeDurationMs == 0 || plan.releaseDurationMs == 0) return;

  const char* cmdStr = open ? "OPEN" : "CLOSE";
  const uint32_t sequence = ++rfSequence;

  logLine(String("[RF #") + sequence + "] profile=" + rfProfileName(profile) +
          " remote=0x" + String(remoteId[0], HEX) + String(remoteId[1], HEX) +
          " mask=0x" + String(blindMask, HEX) +
          " cmd=" + cmdStr +
          " active=" + String(plan.activeDurationMs) + "ms" +
          " release=" + String(plan.releaseDurationMs) + "ms" +
          " dryRun=" + String(RF_DRY_RUN ? "true" : "false"));

#if RF_DRY_RUN
  logLine(String("[RF #") + sequence + "] complete actual=0ms (not transmitted)");
  return;
#endif

  radio.stopListening();
  radio.setPayloadSize(5);
  radio.setCRCLength(RF24_CRC_16);
  radio.flush_tx();
  radio.flush_rx();

  uint8_t pkt[5] = {0x00, 0x00, 0x20, 0x00, 0x00};

  pkt[0] = remoteId[0];
  pkt[1] = remoteId[1];
  pkt[3] = blindMask;
  pkt[4] = rfActiveCommand(open);

  int ok = 0, fail = 0;

  const uint32_t tStart = millis();
  const int activeCycles = sendForDuration(pkt, plan.activeDurationMs, ok, fail);
  pkt[4] = rfReleaseCommand(open);
  const int releaseCycles = sendForDuration(pkt, plan.releaseDurationMs, ok, fail);

  radio.flush_tx();
  startListening();

  logLine(String("[RF #") + sequence + "] complete actual=" + (millis() - tStart) + "ms" +
          " activeCycles=" + activeCycles +
          " releaseCycles=" + releaseCycles +
          " ok=" + ok + " fail=" + fail);
}
