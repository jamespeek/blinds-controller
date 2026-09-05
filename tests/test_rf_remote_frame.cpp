#include <cassert>

#include "../src/rf_remote_frame.h"

int main() {
  RemoteRfFrame frame = {};
  const uint8_t up[] = {0x01, 0xE1, 0x20, 0x04, 0xA1};
  assert(decodeRemoteRfFrame(up, frame));
  assert(frame.remoteId[0] == 0x01 && frame.remoteId[1] == 0xE1);
  assert(frame.blindMask == 0x04 && frame.direction == RemoteDirection::Up);

  const uint8_t down[] = {0x01, 0xE1, 0x20, 0x02, 0xA2};
  assert(decodeRemoteRfFrame(down, frame));
  assert(frame.blindMask == 0x02 && frame.direction == RemoteDirection::Down);

  const uint8_t release[] = {0x01, 0xE1, 0x20, 0x02, 0x22};
  assert(!decodeRemoteRfFrame(release, frame));
}
