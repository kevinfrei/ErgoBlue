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

// I have 8 columns:
constexpr uint8_t colPins[] = {16, 15, 7, 11, 30, 27, 26, 25};
constexpr uint8_t rowPins[] = {2, 3, 4, 5, 28};
constexpr uint8_t numcols = sizeof(colPins) / sizeof(*colPins);
constexpr uint8_t numrows = sizeof(rowPins) / sizeof(*rowPins);
constexpr uint8_t numreps = 16;

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
