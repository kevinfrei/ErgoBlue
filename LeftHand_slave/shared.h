// This is included in both Arduino projects (left & right)
// It's not really a 'clean' way to do this, but I'm just hacking & slashing...

#include <bluefruit.h>
#define DEBUG 0

#define MANUFACTURER "FreikyStuff"
#define MODEL "BlErgoDox"
#define BT_NAME "BlErgoDox"
#define HW_REV "0000"

// These numbers correspond to the *port pin* numbers in the nRF52 documentation
// not the physical pin numbers...

// I have 8 columns:
static const int colPins[] = {16, 15, 7, 11, 30, 27, 26, 25};
static const int rowPins[] = {2, 3, 4, 5, 28};
static const int numcols = sizeof(colPins) / sizeof(*colPins);
static const int numrows = sizeof(rowPins) / sizeof(*rowPins);

void shared_setup() {
  // Is this correct? Maybe my matrix is the other direction?
  // I removed the references, because that wasn't necessary...
  for (auto pin : rowPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  for (auto pin : colPins) {
    pinMode(pin, INPUT_PULLUP);
  }

#if DEBUG
  Serial.begin(115200);
#endif
}
