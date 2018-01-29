#if !defined(HWSTATE_H)
#define HWSTATE_H

#include <bluefruit.h>

#include "dbgcfg.h"

// These numbers correspond to the *port pin* numbers in the nRF52 documentation
// not the physical pin numbers...

constexpr uint8_t colPins[] = {16, 15, 7, 11, 30, 27, 26, 25};
constexpr uint8_t rowPins[] = {2, 3, 4, 5, 28};
constexpr uint8_t numcols = sizeof(colPins) / sizeof(*colPins);
constexpr uint8_t numrows = sizeof(rowPins) / sizeof(*rowPins);

namespace sw {
bool cmp(const uint8_t (&swa)[numrows], const uint8_t (&swb)[numrows]);
void cpy(uint8_t (&swdst)[numrows], const uint8_t (&swsrc)[numrows]);
void clr(uint8_t (&sw)[numrows]);
DBG(void dmp(const uint8_t (&sw)[numrows]));
} // namespace sw

struct hwstate {
  uint8_t switches[numrows];
  uint8_t battery_level;

  hwstate(uint8_t bl = 0);
  hwstate(uint32_t now, const hwstate& prev);
  hwstate(BLEClientUart& clientUart, const hwstate& prev);
  hwstate(const hwstate& c);
  void readSwitches();
  // Send the relevant data over the wire
  void send(BLEUart& bleuart, const hwstate& prev) const;
  // Try to receive any relevant switch data from the wire.
  // Returns true if something was received
  bool receive(BLEClientUart& clientUart, const hwstate& prev);
  bool operator==(const hwstate& o) const;
  bool operator!=(const hwstate& o) const;
  uint64_t toUI64() const;
  DBG(void dump() const);
};

#endif
