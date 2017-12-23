// This is included in both Arduino projects (left & right)
// It's not really a 'clean' way to do this, but I'm just hacking & slashing...

#include <bluefruit.h>
#define DEBUG 1

#define MANUFACTURER "FreikyStuff"
#define MODEL "BlErgoDox"
#define BT_NAME "BlErgoDox"
#define HW_REV "0000"

// These numbers correspond to the *port pin* numbers in the nRF52 documentation
// not the physical pin numbers...

// I have 8 columns:
static constexpr int colPins[] = {16, 15, 7, 11, 30, 27, 26, 25};
static constexpr int rowPins[] = {2, 3, 4, 5, 28};
static constexpr int numcols = sizeof(colPins) / sizeof(*colPins);
static constexpr int numrows = sizeof(rowPins) / sizeof(*rowPins);

using one_side_matrix_t = struct {
  uint8_t rows[numrows];
};

void shared_setup() {
  // Is this correct? Maybe my matrix is the other direction?
  // I removed the references, because that wasn't necessary...
  for (auto &pin : colPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  for (auto &pin : rowPins) {
    pinMode(pin, INPUT_PULLUP);
  }

#if DEBUG
  Serial.begin(115200);
#endif
}
