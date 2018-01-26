// This is included in both Arduino projects (left & right)
// It's not really a 'clean' way to do this, but I'm just hacking & slashing...

#include <bluefruit.h>
#include "dbgcfg.h"
#include "hwstate.h"

#define MANUFACTURER "FreikyStuff"
#define MODEL "BlErgoDox"
#define BT_NAME "BlErgoDox"
#define HW_REV "0000"
#define LHS_NAME BT_NAME "-LHS"


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
