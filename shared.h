// This is included in both Arduino projects (left & right)
// It's not really a 'clean' way to do this, but I'm just hacking & slashing...

#include <bluefruit.h>
#define DEBUG 1

#if DEBUG
#define DBG(a) a

void dumpVal(unsigned long v, const char* header = nullptr) {
  if (header)
    Serial.print(header);
  Serial.println(v);
}
void dumpHex(unsigned long v, const char* header = nullptr) {
  if (header)
    Serial.print(header);
  Serial.println(v, HEX);
}
#if DEBUG == 2
#define DBG2(a) a
#else
#define DBG2(a)
#endif
#else
#define DBG(a)
#define DBG2(a)
#endif

#define MANUFACTURER "FreikyStuff"
#define MODEL "BlErgoDox"
#define BT_NAME "BlErgoDox"
#define HW_REV "0000"
#define LHS_NAME BT_NAME "-LHS"

// These numbers correspond to the *port pin* numbers in the nRF52 documentation
// not the physical pin numbers...

constexpr uint8_t colPins[] = {16, 15, 7, 11, 30, 27, 26, 25};
constexpr uint8_t rowPins[] = {2, 3, 4, 5, 28};
constexpr uint8_t numcols = sizeof(colPins) / sizeof(*colPins);
constexpr uint8_t numrows = sizeof(rowPins) / sizeof(*rowPins);

constexpr uint8_t VBAT = 31; // pin 31 is available for sampling the battery

constexpr uint8_t DEBOUNCE_COUNT = 8;

// Some globals used by both halves
uint8_t battery_level = 0;
uint32_t last_bat_time = 0;

uint8_t readBattery(uint32_t now, uint8_t prev) {
  if (prev && now - last_bat_time <= 30000000) {
    // There's a lot of variance in the reading, so no need to over-report it.
    return prev;
  }
  last_bat_time = now;
  float measuredvbat = analogRead(VBAT) * 6.6 / 1024;
  uint8_t bat_percentage = (uint8_t)round((measuredvbat - 3.7) * 200);
  bat_percentage = min(bat_percentage, 100);
  if (prev != bat_percentage) {
    DBG2(Serial.print("Battery level: "));
    DBG2(Serial.println(battery_level));
  }
  return bat_percentage;
}

uint8_t stableCount;
uint8_t recordedChange[numrows];

struct hwstate {
  static bool swcmp(const uint8_t (&swa)[numrows],
                    const uint8_t (&swb)[numrows]) {
    return memcmp(swa, swb, numrows);
  }
  static void swcpy(uint8_t (&swdst)[numrows],
                    const uint8_t (&swsrc)[numrows]) {
    memcpy(swdst, swsrc, numrows);
  }
  static void swclr(uint8_t (&sw)[numrows]) {
    memset(sw, 0, numrows);
  }
#if DEBUG
  static void swdmp(const uint8_t (&sw)[numrows]) {
    Serial.print("Sw: ");
    for (uint8_t i : sw) {
      Serial.print(i, HEX);
    }
    Serial.println("");
  }
#endif

  uint8_t switches[numrows];
  uint8_t battery_level;
  hwstate(uint8_t bl = 0) : battery_level(bl) {
    swclr(switches);
  }
  hwstate(uint32_t now, const hwstate& prev)
      : battery_level(readBattery(now, prev.battery_level)) {
    swcpy(switches, prev.switches);
    readSwitches();
  }
  hwstate(BLEClientUart& clientUart, const hwstate& prev) {
    if (!receive(clientUart))
      memcpy(reinterpret_cast<uint8_t*>(this),
             reinterpret_cast<const uint8_t*>(&prev),
             sizeof(hwstate));
  }

  void readSwitches() {
    uint8_t newSwitches[numrows];
    swclr(newSwitches);
    for (uint8_t colNum = 0; colNum < numcols; ++colNum) {
      uint8_t val = 1 << colNum;
      digitalWrite(colPins[colNum], LOW);
      for (uint8_t rowNum = 0; rowNum < numrows; ++rowNum) {
        if (!digitalRead(rowPins[rowNum])) {
          newSwitches[rowNum] |= val;
        }
      }
      digitalWrite(colPins[colNum], HIGH);
    }
    // Debouncing: This just waits for stable DEBOUNCE_COUNT reads before
    // reporting
    if (swcmp(newSwitches, stableCount ? recordedChange : switches)) {
      // We've observed a change (in either the delta or the reported: doesn't
      // matter which) record the change and (re)start the timer
      stableCount = 1;
      swcpy(recordedChange, newSwitches);
    } else if (stableCount > DEBOUNCE_COUNT) {
      // We've had a stable reading for long enough: report it & clear the timer
      // If the micros() timer wraps around while waiting for stable readings,
      // we just wind up with a shorter debounce timer, which shouldn't really
      // hurt anything
      swcpy(switches, newSwitches);
      stableCount = 0;
    } else if (stableCount) {
      stableCount++;
    }
  }

  // Send the relevant data over the wire
  void send(BLEUart& bleuart, const hwstate& prev) {
    bleuart.write((uint8_t*)&switches, sizeof(switches) + 1);
  }

  // Try to receive any relevant switch data from the wire.
  // Returns true if something was received
  bool receive(BLEClientUart& clientUart) {
    if (clientUart.available()) {
      uint8_t buffer[sizeof(switches) + 1];
      int size = clientUart.read(buffer, sizeof(switches) + 1);
      if (size == sizeof(switches) + 1) {
        memcpy(reinterpret_cast<uint8_t*>(this), buffer, sizeof(switches) + 1);
      } else {
#if DEBUG
        // NOTE: This fires occasionally when button mashing on the left, so
        // perhaps I shouldn't have changed this just on a whim. Wez clearly
        // knew what he was doing :)
        Serial.print("Incorrect datagram size:");
        Serial.print(" expected ");
        Serial.print(sizeof(hwstate));
        Serial.print(" got ");
        Serial.println(size);
#endif
      }
      return true;
    }
    return false;
  }

  bool operator==(const hwstate& o) {
    return o.battery_level == battery_level && !swcmp(o.switches, switches);
  }

  bool operator!=(const hwstate& o) {
    return !((*this) == o);
  }

  uint64_t toUI64() {
    uint64_t res = 0;
    for (int i = numrows - 1; i >= 0; i--)
      res = (res * 256) + switches[i];
    return res;
  }

#if DEBUG
  void dump() {
    Serial.print("Battery Level:");
    Serial.println(battery_level);
    for (int r = 0; r < numrows; r++) {
      for (int c = numcols - 1; c >= 0; c--) {
        unsigned int mask = 1 << c;
        if (switches[r] & mask) {
          Serial.print("X ");
        } else {
          Serial.print("- ");
        }
      }
      Serial.println("");
    }
  }
#endif
};

using scancode_t = uint8_t;

void shared_setup() {
  static_assert(numcols <= 8, "No more than 8 columns, please.");

  // For my wiring, the columns are output, and the rows are input...
  for (auto pin : colPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  for (auto pin : rowPins) {
    pinMode(pin, INPUT_PULLUP);
  }

  DBG(Serial.begin(115200));
}

void rtos_idle_callback(void) {}

scancode_t key_down(scancode_t val) {
  return val & 0x80;
}

scancode_t get_key(scancode_t val) {
  return val & ~0x80;
}

bool is_key_down(scancode_t val) {
  return val & 0x80;
}

scancode_t make_scan_code(uint8_t row, uint8_t col, bool pressed) {
  return (row * numcols + col) | (pressed ? 0x80 : 0);
}
