#include <SPI.h>
#include <RF24.h>

// Passive, fixed-channel RF timing recorder. It has no RF write calls and
// disables auto-acknowledgements, so it cannot transmit or move a blind.
#ifndef CAPTURE_CHANNEL
#define CAPTURE_CHANNEL 52
#endif

static constexpr int CE_PIN = 9;
static constexpr int CS_PIN = 11;
static constexpr int SCK_PIN = 12;
static constexpr int MOSI_PIN = 18;
static constexpr int MISO_PIN = 16;
static constexpr uint8_t RF_ADDR[3] = {0xE7, 0xE7, 0xE7};
static constexpr uint32_t TRAIN_GAP_US = 1500;
static constexpr uint32_t PHASE_IDLE_US = 10000;

static RF24 radio(CE_PIN, CS_PIN);
static bool receiving = false;
static uint8_t phasePacket[5] = {};
static uint32_t phaseStartedUs = 0;
static uint32_t lastPacketUs = 0;
static uint32_t packetCount = 0;
static uint32_t trainCount = 0;
static uint16_t currentTrainPackets = 0;
static uint16_t minTrainPackets = UINT16_MAX;
static uint16_t maxTrainPackets = 0;
static uint32_t interTrainGapTotalUs = 0;
static uint32_t minInterTrainGapUs = UINT32_MAX;
static uint32_t maxInterTrainGapUs = 0;
static uint32_t lastStatusMs = 0;

static void finishTrain() {
  if (currentTrainPackets == 0) return;
  trainCount++;
  if (currentTrainPackets < minTrainPackets) minTrainPackets = currentTrainPackets;
  if (currentTrainPackets > maxTrainPackets) maxTrainPackets = currentTrainPackets;
  currentTrainPackets = 0;
}

static void printPhase() {
  if (!receiving) return;
  finishTrain();
  const uint32_t durationUs = lastPacketUs - phaseStartedUs;
  const uint32_t averageTrainPackets = trainCount ? packetCount / trainCount : 0;
  const uint32_t gapCount = trainCount > 0 ? trainCount - 1 : 0;
  const uint32_t averageGapUs = gapCount ? interTrainGapTotalUs / gapCount : 0;

  Serial.printf("[CHANNEL %u] phase=%02X%02X%02X%02X%02X duration=%lums packets=%lu "
                "trains=%lu train-packets=%u..%u avg=%lu gap-us=%lu..%lu avg=%lu\n",
                CAPTURE_CHANNEL,
                phasePacket[0], phasePacket[1], phasePacket[2], phasePacket[3], phasePacket[4],
                static_cast<unsigned long>(durationUs / 1000),
                static_cast<unsigned long>(packetCount),
                static_cast<unsigned long>(trainCount),
                minTrainPackets == UINT16_MAX ? 0 : minTrainPackets, maxTrainPackets,
                static_cast<unsigned long>(averageTrainPackets),
                minInterTrainGapUs == UINT32_MAX ? 0 : minInterTrainGapUs,
                static_cast<unsigned long>(maxInterTrainGapUs),
                static_cast<unsigned long>(averageGapUs));
  receiving = false;
}

static void beginPhase(const uint8_t packet[5], uint32_t nowUs) {
  memcpy(phasePacket, packet, sizeof(phasePacket));
  phaseStartedUs = nowUs;
  lastPacketUs = nowUs;
  packetCount = 0;
  trainCount = 0;
  currentTrainPackets = 0;
  minTrainPackets = UINT16_MAX;
  maxTrainPackets = 0;
  interTrainGapTotalUs = 0;
  minInterTrainGapUs = UINT32_MAX;
  maxInterTrainGapUs = 0;
  receiving = true;
}

static void recordPacket(const uint8_t packet[5]) {
  const uint32_t nowUs = micros();
  if (receiving && memcmp(packet, phasePacket, sizeof(phasePacket)) != 0) printPhase();
  if (!receiving) beginPhase(packet, nowUs);

  const uint32_t gapUs = nowUs - lastPacketUs;
  if (packetCount > 0 && gapUs > TRAIN_GAP_US) {
    finishTrain();
    interTrainGapTotalUs += gapUs;
    if (gapUs < minInterTrainGapUs) minInterTrainGapUs = gapUs;
    if (gapUs > maxInterTrainGapUs) maxInterTrainGapUs = gapUs;
  }
  currentTrainPackets++;
  packetCount++;
  lastPacketUs = nowUs;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("[CHANNEL %u] Passive fixed-channel receiver starting; it will not transmit.\n", CAPTURE_CHANNEL);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  if (!radio.begin()) {
    Serial.println("[CHANNEL] RF24 initialisation failed. Check wiring and power.");
    return;
  }
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setAddressWidth(3);
  radio.setPayloadSize(5);
  radio.disableDynamicPayloads();
  radio.setCRCLength(RF24_CRC_16);
  radio.setDataRate(RF24_2MBPS);
  radio.openReadingPipe(1, RF_ADDR);
  radio.setChannel(CAPTURE_CHANNEL);
  radio.startListening();
  Serial.println("[CHANNEL] Ready for remote presses.");
}

void loop() {
  uint8_t pipe = 0;
  while (radio.available(&pipe)) {
    uint8_t packet[5];
    radio.read(packet, sizeof(packet));
    recordPacket(packet);
  }

  const uint32_t nowUs = micros();
  if (receiving && nowUs - lastPacketUs >= PHASE_IDLE_US) printPhase();

  const uint32_t nowMs = millis();
  if (nowMs - lastStatusMs >= 5000) {
    Serial.printf("[CHANNEL %u] Waiting for packets.\n", CAPTURE_CHANNEL);
    lastStatusMs = nowMs;
  }
  delay(0);
}
