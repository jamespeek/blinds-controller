#include <SPI.h>
#include <RF24.h>

// Passive Hunter Douglas RF capture tool for the ESP32-S2/nRF24L01+ wiring
// used by this project. It never calls an RF24 write method and disables
// auto-acknowledgements, so it cannot transmit or move a blind.
static constexpr int CE_PIN = 9;
static constexpr int CS_PIN = 11;
static constexpr int SCK_PIN = 12;
static constexpr int MOSI_PIN = 18;
static constexpr int MISO_PIN = 16;

static constexpr uint8_t RF_ADDR[3] = {0xE7, 0xE7, 0xE7};
static constexpr uint8_t CHANNELS[] = {52, 71, 33};
static constexpr uint32_t CHANNEL_DWELL_MS = 5;

static RF24 radio(CE_PIN, CS_PIN);
static uint8_t channelIndex = 0;
static uint32_t lastChannelChangeMs = 0;
static uint32_t lastStatusMs = 0;
static bool collecting = false;
static uint8_t collectedPacket[5] = {};
static uint8_t collectedChannel = 0;
static uint32_t collectedStartMs = 0;
static uint32_t collectedLastMs = 0;
static uint32_t collectedCount = 0;

static void beginListeningOn(uint8_t channel) {
  radio.stopListening();
  radio.setChannel(channel);
  radio.startListening();
}

static void printPacketSummary() {
  if (!collecting) return;
  Serial.printf("[RX] t=%lums duration=%lums packets=%lu first-ch=%u payload=",
                static_cast<unsigned long>(collectedStartMs),
                static_cast<unsigned long>(collectedLastMs - collectedStartMs),
                static_cast<unsigned long>(collectedCount), collectedChannel);
  for (uint8_t i = 0; i < sizeof(collectedPacket); i++) Serial.printf("%02X", collectedPacket[i]);
  Serial.println();
  collecting = false;
}

static void collectPacket(const uint8_t packet[5]) {
  const uint32_t now = millis();
  if (collecting && memcmp(packet, collectedPacket, sizeof(collectedPacket)) != 0) {
    printPacketSummary();
  }
  if (!collecting) {
    memcpy(collectedPacket, packet, sizeof(collectedPacket));
    collectedChannel = CHANNELS[channelIndex];
    collectedStartMs = now;
    collectedCount = 0;
    collecting = true;
  }
  collectedLastMs = now;
  collectedCount++;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[CAPTURE] Passive RF receiver starting; it will not transmit.");

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  if (!radio.begin()) {
    Serial.println("[CAPTURE] RF24 initialisation failed. Check wiring and power.");
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
  beginListeningOn(CHANNELS[channelIndex]);
  lastChannelChangeMs = millis();

  Serial.println("[CAPTURE] Listening on 52, 71, and 33; press remote buttons now.");
}

void loop() {
  uint8_t pipe = 0;
  while (radio.available(&pipe)) {
    uint8_t packet[5];
    radio.read(packet, sizeof(packet));
    collectPacket(packet);
  }

  const uint32_t now = millis();
  if (collecting && now - collectedLastMs >= 50) printPacketSummary();
  if (now - lastStatusMs >= 5000) {
    Serial.printf("[CAPTURE] Waiting for packets; scanning ch=%u\n", CHANNELS[channelIndex]);
    lastStatusMs = now;
  }
  if (now - lastChannelChangeMs >= CHANNEL_DWELL_MS) {
    channelIndex = (channelIndex + 1) % (sizeof(CHANNELS) / sizeof(CHANNELS[0]));
    beginListeningOn(CHANNELS[channelIndex]);
    lastChannelChangeMs = now;
  }

  delay(0);
}
