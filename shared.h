// This is included in both Arduino projects (left & right)
// It's not really a 'clean' way to do this, but I'm just hacking & slashing...

#include <bluefruit.h>
#define DEBUG 1

#if DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

#define MANUFACTURER "FreikyStuff"
#define MODEL "BlErgoDox"
#define BT_NAME "BlErgoDox"
#define HW_REV "0000"

// These numbers correspond to the *port pin* numbers in the nRF52 documentation
// not the physical pin numbers...

constexpr uint8_t colPins[] = {16, 15, 7, 11, 30, 27, 26, 25};
constexpr uint8_t rowPins[] = {2, 3, 4, 5, 28};
constexpr uint8_t numcols = sizeof(colPins) / sizeof(*colPins);
constexpr uint8_t numrows = sizeof(rowPins) / sizeof(*rowPins);

constexpr int VBAT = 31; // pin 31 is available for sampling the battery

// Some globals for both sides of the keybaord...
uint8_t battery_level = 0;
uint32_t last_bat_time = 0;

uint8_t readBattery(uint32_t now, const struct hwstate& prev);

template <typename T>
struct matrix_type {
  T rows[numrows];
  matrix_type() {
    memset(&rows, 0, sizeof(rows));
  };
  T get_switch(uint8_t row, uint8_t col) const {
    return rows[row] & (1 << col);
  }
  static matrix_type<T> read(matrix_type<T> matrix = matrix_type<T>{}) {
    for (uint8_t colNum = 0; colNum < numcols; ++colNum) {
      digitalWrite(colPins[colNum], LOW);
      for (uint8_t rowNum = 0; rowNum < numrows; ++rowNum) {
        if (!digitalRead(rowPins[rowNum])) {
          matrix.rows[rowNum] |= 1 << colNum;
        }
      }
      digitalWrite(colPins[colNum], HIGH);
    }
    return matrix;
  }
};

struct hwstate {
  uint8_t switches[numrows];
  uint8_t battery_level;

  hwstate(uint8_t bl = 0) : battery_level(bl) {
    memset(switches, 0, sizeof(switches));
  }
  hwstate(uint32_t now, const hwstate& prev)
      : battery_level(readBattery(now, prev)) {
    memset(switches, 0, sizeof(switches));
    for (uint8_t colNum = 0; colNum < numcols; ++colNum) {
      digitalWrite(colPins[colNum], LOW);
      for (uint8_t rowNum = 0; rowNum < numrows; ++rowNum) {
        if (!digitalRead(rowPins[rowNum])) {
          switches[rowNum] |= 1 << colNum;
        }
      }
      digitalWrite(colPins[colNum], HIGH);
    }
  }
  hwstate(BLEClientUart& clientUart, const hwstate& prev) {
    if (clientUart.available()) {
      int size = clientUart.read((uint8_t*)this, sizeof(*this));
#if DEBUG
      if (size != sizeof(hwstate)) {
        Serial.print("Incorrect datagram size:");
        Serial.print(" expected ");
        Serial.print(sizeof(hwstate));
        Serial.print(" got ");
        Serial.println(size);
      }
#endif
    } else {
      memcpy(this, &prev, sizeof(*this));
    }
  }

  bool operator==(const hwstate& o) {
    return o.battery_level == battery_level &&
           !memcmp(o.switches, switches, numrows);
  }

  bool operator!=(const hwstate& o) {
    return !((*this) == o);
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

uint8_t readBattery(uint32_t now, const hwstate& prev) {
  if (prev.battery_level && now - last_bat_time <= 30000) {
    // There's a lot of variance in the reading, so no need to over-report it.
    return prev.battery_level;
  }
  last_bat_time = now;
  float measuredvbat = analogRead(VBAT) * 6.6 / 1024;
  uint8_t bat_percentage = (uint8_t)round((measuredvbat - 3.7) * 200);
  bat_percentage = min(bat_percentage, 100);
  if (prev.battery_level != bat_percentage) {
    DBG(Serial.print("Battery level: "));
    DBG(Serial.println(battery_level));
  }
  return bat_percentage;
}

using scancode_t = uint8_t;
using wscancode_t = uint16_t;

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
