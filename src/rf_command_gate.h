#pragma once

inline bool canTransmitCommand(bool rfReady, bool zoneOwned) {
  return rfReady && zoneOwned;
}
