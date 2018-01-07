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
#define HW_VAL 0
#define HW_REV "0000"

// These numbers correspond to the *port pin* numbers in the nRF52 documentation
// not the physical pin numbers...
constexpr uint8_t colPins[] = {16, 15, 7, 11, 30, 27, 26, 25};
constexpr uint8_t rowPins[] = {2, 3, 4, 5, 28};
constexpr uint8_t numcols = sizeof(colPins) / sizeof(*colPins);
constexpr uint8_t numrows = sizeof(rowPins) / sizeof(*rowPins);

constexpr uint8_t VBAT = 31; // The battery sampling pin
constexpr uint32_t BAT_READ_FREQ = 30000; // How many msec between reads

constexpr uint8_t numreps = 16;
constexpr uint8_t max_layer_depth = 8;

using scancode_t = uint8_t;

// Globals:
BLEBas battery;

#if DEBUG
void printVal(int value, const char* header = nullptr) {
  if (header) {
    Serial.print(header);
  }
  Serial.println(value);
}

void printHex(int value, const char* header = nullptr) {
  if (header) {
    Serial.print(header);
  }
  Serial.println(value, HEX);
}
#endif

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

class switch_state {
  friend class keyboard_state;

 protected:
  uint8_t rows[numrows];
  void reset_matrix() {
    memset(&rows, 0, sizeof(rows));
  }

 public:
  switch_state(const switch_state& copy) {
    memcpy(&rows[0], &copy.rows[0], sizeof(rows) / sizeof(rows[0]));
  }
  switch_state(bool readstate = false) {
    if (readstate) {
      this->read();
    } else {
      reset_matrix();
    }
  };
  bool get_switch(uint8_t row, uint8_t col) const {
    return this->rows[row] & (1 << col);
  }
  void read() {
    reset_matrix();
    for (uint8_t colNum = 0; colNum < numcols; ++colNum) {
      digitalWrite(colPins[colNum], LOW);
      for (uint8_t rowNum = 0; rowNum < numrows; ++rowNum) {
        if (!digitalRead(rowPins[rowNum])) {
          this->rows[rowNum] |= 1 << colNum;
        }
      }
      digitalWrite(colPins[colNum], HIGH);
    }
  }
  bool operator==(const switch_state& other) const {
    for (uint8_t i = 0; i < numrows; i++) {
      if (rows[i] != other.rows[i]) {
        return false;
      }
    }
    return true;
  }
  bool operator!=(const switch_state& other) const {
    return !(*this == other);
  }
#if DEBUG
  void dump() const {
    for (uint8_t row = 0; row < numrows; row++) {
      for (uint8_t col = 0; col < numcols; col++) {
        Serial.print(!!get_switch(row, col), HEX);
        Serial.print(" ");
      }
      Serial.println("");
    }
  }
#endif
};

uint8_t moreSum(uint8_t sum, uint8_t val) {
  uint8_t newVal = sum << 1 | !!(sum & 0x80);
  return newVal ^ val;
}

class keyboard_state {
  uint32_t last_read;
  switch_state switches;
  uint8_t battery_level;

  void update_battery(uint32_t now) {
    if (now - this->last_read >= BAT_READ_FREQ) {
      float measuredvbat = analogRead(VBAT) * 6.6 / 1024;
      uint8_t bat_percentage = (uint8_t)round((measuredvbat - 3.7) * 200);
      this->battery_level = min(bat_percentage, 100);
      this->last_read = now;
    }
  }

 public:
  // Constructor for an 'unread' keyboard state:
  keyboard_state() : switches{}, battery_level{0}, last_read{0} {}

  // Constructor for reading keyboard state
  // This always requires a previously read keyboard_state
  keyboard_state(const keyboard_state& last, uint32_t now)
      : switches{true},
        battery_level{last.battery_level},
        last_read{last.last_read} {
    update_battery(now);
    DBG(dump());
  }

  // Copy Constructor
  keyboard_state(const keyboard_state& copy)
      : switches{copy.switches},
        last_read{copy.last_read},
        battery_level{copy.battery_level} {}

  bool operator==(const keyboard_state& s) const {
    return this->battery_level == s.battery_level &&
           this->switches == s.switches;
  }

  bool operator!=(const keyboard_state& s) const {
    return !(*this == s);
  }

  // This is used from the LHS to send data to the RHS You could just send scan
  // codes, but this is a whopping total of 8 bytes, which hardly seems worth
  // trying to better optimize
  // I'm sending a data packet formed like this:
  // byte count (including self)
  // (byte count - 2) bytes of data
  // a checksum of the data
  void send(BLEUart& uart) const {
    uint8_t data[numrows + 2];
    uint8_t cksum = 0x3a ^ HW_VAL;
    data[0] = sizeof(data) / sizeof(data[0]);
    for (uint8_t i = 0; i < numrows; i++) {
      uint8_t v = this->switches.rows[i];
      data[i + 1] = v;
      cksum = moreSum(cksum, v);
    }
    data[1 + numrows] = battery_level;
    data[2 + numrows] = moreSum(cksum, battery_level);
    uart.write(data, sizeof(data) / sizeof(data[0]));
    DBG(dump());
  }

  void receive(BLEClientUart& uart) {
    if (uart.available()) {
      uint8_t data[numrows + 2];
      uint8_t cksum = 0x3a ^ HW_VAL;
      uint8_t offset = 0;
      int val = uart.read();
      if (val == EOF) {
        // Well, shit
        DBG(Serial.println("Unexpected EOF on Client Uart :("));
        return;
      }
      if (val != sizeof(data) / sizeof(data[0])) {
        DBG(printVal(val, "Unexpected packet size from remote Uart :"));
        return;
      }
      for (uint8_t i = 0; i < numrows; i++) {
        val = uart.read();
        if (val == EOF) {
          // Well, shit
          DBG(printVal(i, "Unexpected EOF on Client Uart before byte "));
          return;
        }
        cksum = moreSum(cksum, val);
        data[i + i] = (uint8_t)val;
      }
      val = uart.read();
      if (val == EOF) {
        DBG(Serial.println("Missing battery level in Client Uart packet."));
      }
      this->battery_level = (uint8_t)val;
      val = uart.read();
      if (val == EOF) {
        DBG(Serial.println("Missing checksum value."));
        return;
      }
      cksum = moreSum(cksum, this->battery_level);
      if (val != cksum) {
        DBG(printHex(val, "Bad checksum for data. Got "));
        DBG(printHex(cksum, "But expected "));
      }
    }
  }

  uint8_t notify_battery(uint8_t prevLevel) const {
    if (prevLevel != this->battery_level) {
      battery.notify(this->battery_level);
    }
    return this->battery_level;
  }
#if DEBUG
  void dump() const {
    Serial.println("Switch matrix:");
    switches.dump();
    printVal(battery_level, "Battery Level: ");
    printVal(last_read, "Last Read:     ");
  }
#endif
};

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
  battery.begin();

  DBG(Serial.begin(115200));
}

void rtos_idle_callback(void) {}
