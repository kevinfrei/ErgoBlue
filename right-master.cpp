#include "shared.h"

#include "keyhelpers.h"
#include "keymap.h"
#include "keystate.h"

#define STATUS_DUMP 1

// Globals
BLEDis dis;
BLEHidAdafruit hid;
BLEClientUart clientUart;
BLEBas battery;

hwstate leftSide{};
hwstate rightSide{};

#if STATUS_DUMP
// If you hold this configuration down, it types out status
constexpr uint8_t status_keys_left[] = {0, 0, 0, 0x80, 0x80};
constexpr uint8_t status_keys_right[] = {0, 0, 0, 1, 1};
// If you hold this down, just on the right keyboard, it should RHS status only
// (might be made helpful in the future...)
constexpr uint8_t just_right_stat[] = {0, 0, 0, 3, 3};
#endif

// Declarations
void resetKeyMatrix();
void cent_connect_callback(uint16_t conn_handle);
void cent_disconnect_callback(uint16_t conn_handle, uint8_t reason);
void scan_callback(ble_gap_evt_adv_report_t* report);
void startAdv();
void type_string(const char* str);
void type_number(uint32_t val);

// In Arduino world the 'setup' function is called to initialize the device.
// The 'loop' function is called over & over again, after setup completes.
void setup() {
  shared_setup();
  resetKeyMatrix();

  // Central and peripheral
  Bluefruit.begin(true, true);
  // Bluefruit.clearBonds();
  Bluefruit.autoConnLed(false);

  battery.begin();

  // I'm assuming that by dropping this power down, I'll save some battery life.
  // I should experiment to see how low I can get it and still communicate with
  // both my Mac and my PC reliably. They're each within a meter of the
  // keyboard... Acceptable values: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(-16);
  Bluefruit.setName(BT_NAME);

  Bluefruit.Central.setConnectCallback(cent_connect_callback);
  Bluefruit.Central.setDisconnectCallback(cent_disconnect_callback);

  dis.setManufacturer(MANUFACTURER);
  dis.setModel(MODEL);
  dis.setHardwareRev(HW_REV);
  dis.setSoftwareRev(__DATE__);
  dis.begin();

  clientUart.begin();
  // clientUart.setRxCallback(cent_bleuart_rx_callback);

  /* Start Central Scanning
   * - Enable auto scan if disconnected
   * - Interval = 100 ms, window = 80 ms
   * - Filter only accept bleuart service
   * - Don't use active scan
   * - Start(timeout) with timeout = 0 will scan forever (until connected)
   */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.restartOnDisconnect(true);
  Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
  Bluefruit.Scanner.filterUuid(BLEUART_UUID_SERVICE);
  Bluefruit.Scanner.useActiveScan(false);
  // I should probably stop advertising after a while if that's possible. I have
  // switches now, so if I need it to advertise, I can just punch the power.
  Bluefruit.Scanner.start(0); // 0 = Don't stop scanning after n seconds

  hid.begin();
  // delay(5000);
  // Bluefruit.printInfo();

  startAdv();
}

void cent_connect_callback(uint16_t conn_handle) {
  // TODO: Maybe make this more secure? I haven't looked into how secure this
  // is in the documentation :/
  char peer_name[32] = {0};
  Bluefruit.Gap.getPeerName(conn_handle, peer_name, sizeof(peer_name));
  // TODO:
  // I ought to at least make sure the peer_name is LHS_NAME, right?
  DBG(Serial.print("[Cent] Connected to "));
  DBG(Serial.println(peer_name));

  if (clientUart.discover(conn_handle)) {
    // Enable TXD's notify
    clientUart.enableTXD();
  } else {
    Bluefruit.Central.disconnect(conn_handle);
  }

  resetKeyMatrix();
}

void cent_disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  DBG(dumpVal(conn_handle, "Connection Handle Disconnected: "));
  DBG(dumpVal(reason, " Reason #"));
  DBG(Serial.println("[Cent] Disconnected"));
  resetKeyMatrix();
}

void scan_callback(ble_gap_evt_adv_report_t* report) {
  // Check if advertising contain BleUart service
  if (Bluefruit.Scanner.checkReportForService(report, clientUart)) {
    // Connect to device with bleuart service in advertising
    Bluefruit.Central.connect(report);
  }
}

void startAdv(void) {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(hid);

  Bluefruit.ScanResponse.addService(battery);

  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30); // number of seconds in fast mode
  Bluefruit.Advertising.start(0); // 0 = Don't stop advertising after n seconds
}

keystate keyStates[16];
constexpr layer_t layer_max = 7;
layer_t layer_stack[layer_max + 1];
layer_t layer_pos = 0;

// This is called when the LHS connects, disconnects, and when the system is
// initialized.  The idea is that it should just wipe everything clean.
void resetKeyMatrix() {
  layer_pos = 0;
  layer_stack[0] = 0;
  memset(&leftSide, 0, sizeof(leftSide));
  memset(&rightSide, 0, sizeof(rightSide));
  memset(keyStates, 0xff, sizeof(keyStates));

  hid.keyRelease();
}

// Look for a slot that is either already in use for this scan code, or vacant.
// If we don't have a vacant slot, return the oldest, but still in use, slot,
// but only for key-up states, as we're probably through with them anyway.
struct keystate* findStateSlot(uint8_t scanCode) {
  keystate *vacant = nullptr, *reap = nullptr;
  for (auto& s : keyStates) {
    // If we have the same scan code, huzzah!
    if (s.scanCode == scanCode) {
      return &s;
    }
    // If we found a vacancy, potentially use it. We have to keep looking to see
    // if we have the same scan code, though.
    if (s.scanCode == 0xff) {
      vacant = &s;
    } else if (!s.down) {
      if (!reap) {
        reap = &s;
      } else if (s.lastChange < reap->lastChange) {
        // Idle longer than the other reapable candidate; choose
        // the eldest of them
        reap = &s;
      }
    }
  }
  if (vacant) {
    return vacant;
  }
  return reap;
}

// Find the first specified action in the layer stack
action_t resolveActionForScanCodeOnActiveLayer(uint8_t scanCode) {
  layer_t s = layer_pos;
  while (s > 0 && keymap[layer_stack[s]][scanCode] == ___) {
    --s;
  }
  return keymap[layer_stack[s]][scanCode];
}

// Find last bit set in a long (BSD function, not available in Arduino)
uint8_t flsl(uint64_t val) {
  // GCC builtin function seems to work on Arduino :)
  return static_cast<uint8_t>(63 - __builtin_clzll(val));
}

// Given a delta mask, get the scan code, update the delta mask and set pressed
// while we're at it.
scancode_t getNextScanCode(uint64_t& delta, uint64_t curState, bool& pressed) {
  scancode_t sc = flsl(delta);
  uint64_t mask = ((uint64_t)1) << sc;
  pressed = curState & mask;
  delta ^= mask;
  return sc;
}

#if DEBUG
void dumpScanCode(uint8_t sc, bool pressed) {
  Serial.print("Scan Code ");
  Serial.print(sc, HEX);
  Serial.print(" was ");
  Serial.println(pressed ? "pressed" : "released");
}
void dumpLayers() {
  Serial.print("Layer stack: ");
  for (int i = 0; i <= layer_pos; i++) {
    Serial.print(layer_stack[i]);
    Serial.print(" ");
  }
  Serial.println("");
}
#endif

void layer_push(layer_t layer) {
  DBG(dumpVal(layer, "Push "));
  if (layer_pos < layer_max)
    layer_stack[++layer_pos] = layer;
  DBG(dumpLayers());
}

void layer_pop(layer_t layer) {
  DBG(dumpVal(layer, "Pop "));
  if (layer_pos > 0)
    --layer_pos;
  DBG(dumpLayers());
}

void layer_toggle(layer_t layer) {
  // Toggling a layer: If it exists *anywhere* in the layer stack, turn it
  // off (and fold the layer stack down) If it's *not* in the layer stack,
  // add it.
  for (layer_t l = layer_pos; l != 0; l--) {
    if (layer_stack[l] == layer) {
      DBG(dumpVal(layer, "Turning off layer "));
      DBG(dumpVal(l, "at location "));
      if (layer_pos != l) {
        DBG(dumpVal(layer_pos - l, "Shifting by "));
        memmove(&layer_stack[l], &layer_stack[l + 1], layer_pos - l);
      }
      layer_pos--;
      DBG(dumpLayers());
      return;
    }
  }
  DBG(Serial.print("(For Toggle) "));
  layer_push(layer);
}

void layer_switch(layer_t layer) {
  DBG(dumpVal(layer_stack[layer_pos], "Switching layer "));
  DBG(dumpVal(layer, "to layer "));
  layer_stack[layer_pos] = layer;
  DBG(dumpLayers());
}

void loop() {
  uint32_t now = millis();

  // Get the hardware state for the two sides...
  hwstate downRight{now, rightSide};
  hwstate downLeft{clientUart, leftSide};

  // Update the combined battery level
  if (downRight.battery_level != rightSide.battery_level ||
      downLeft.battery_level != leftSide.battery_level) {
    // We only get the battery level from the left side once you hit a key, so
    // only report it if we have something to actually report
    if (downLeft.battery_level) {
      battery.notify((downRight.battery_level + downLeft.battery_level) / 2);
      DBG(dumpVal((downRight.battery_level + downLeft.battery_level) / 2,
                  "battery avg: "));
    } else {
      DBG(dumpVal(downRight.battery_level, "right only battery: "));
      battery.notify(downRight.battery_level);
    }
  }

  // Get the before & after of each side into a 64 bit value
  uint64_t beforeLeft = leftSide.toUI64(), afterLeft = downLeft.toUI64();
  uint64_t beforeRight = rightSide.toUI64(), afterRight = downRight.toUI64();
  uint64_t deltaLeft = beforeLeft ^ afterLeft;
  uint64_t deltaRight = beforeRight ^ afterRight;
  bool keysChanged = deltaLeft || deltaRight;

  while (deltaLeft || deltaRight) {
    scancode_t sc;
    bool pressed;
    if (deltaLeft) {
      sc = getNextScanCode(deltaLeft, afterLeft, pressed);
    } else {
      // Add offset to the right scan code...
      sc = getNextScanCode(deltaRight, afterRight, pressed) + numcols * numrows;
    }
    DBG(dumpScanCode(sc, pressed));

    // Get a state slot for this scan code
    keystate* state = findStateSlot(sc);
    if (!state) {
      // If this is a keydown and we don't have an available state slot just
      // ignore it. If we chose to toss out older keydowns instead, things could
      // get pretty weird. If this is a keyup, and we still don't have a state
      // slot, that's a little bonkers, but there's not much we can do about it.
      continue;
    }
    // State update returns a layer action to perform...
    switch (state->update(sc, pressed, now)) {
      case kPushLayer:
        layer_push(state->get_layer());
        break;
      case kPopLayer:
        layer_pop(state->get_layer());
        break;
      case kToggleLayer:
        layer_toggle(state->get_layer());
        break;
      case kSwitchLayer:
        layer_switch(state->get_layer());
        break;
    }
  }

  if (keysChanged) {
    uint8_t report[6] = {0, 0, 0, 0, 0, 0};
    uint8_t repsize = 0;
    uint8_t mods = 0;

    for (auto& state : keyStates) {
      if (state.scanCode == 0xff)
        continue;
      if ((state.action & kConsumer) == kConsumer) {
        // For a consumer control button, there are no modifiers, it's
        // just a simple call. So just call it directly:
        if (state.down) {
          DBG(dumpHex(state.action & 0xff, "Consumer key press: "));
          hid.consumerKeyPress(state.action & 0xff);
        } else {
          DBG(dumpHex(state.action & 0xff, "Consumer key release: "));
          hid.consumerKeyRelease();
          // We have to clear this thing out when we're done, because we take
          // action on the key release as well. We don't do this for the normal
          // keyboardReport.
          state.scanCode = 0xff;
        }
      } else if (state.down) {
        switch (state.action & kMask) {
          case kTapHold:
            if (now - state.lastChange > 200) {
              // Holding
              mods |= (state.action >> 16) & 0xff;
            } else {
              // Tapping
              auto key = state.action & 0xff;
              if (key != 0 && repsize < 6) {
                report[repsize++] = key;
              }
            }
            break;
          case kKeyAndMod: {
            mods |= (state.action >> 16) & 0xff;
            auto key = state.action & 0xff;
            if (key != 0 && repsize < 6) {
              report[repsize++] = key;
            }
          } break;
          case kKeyPress: {
            auto key = state.action & 0xff;
            if (key != 0 && repsize < 6) {
              report[repsize++] = key;
            }
          } break;
          case kModifier:
            mods |= state.action & 0xff;
            break;
          case kToggleMod:
            mods ^= state.action & 0xff;
            break;
        }
      }
    }
#if DEBUG
    Serial.print("mods=");
    Serial.print(mods, HEX);
    Serial.print(" repsize=");
    Serial.print(repsize);
    for (int i = 0; i < repsize; i++) {
      Serial.print(" ");
      Serial.print(report[i], HEX);
    }
    Serial.println("");
#endif

    hid.keyboardReport(mods, report);

    DBG2(Serial.println("============================="));
    DBG2(Serial.print("Left side "));
    DBG2(downLeft.dump());
    DBG2(hwstate::swdmp(downLeft.switches));
    DBG2(Serial.print("Right side "));
    DBG2(downRight.dump());
    DBG2(hwstate::swdmp(downRight.switches));

    // Update the hardware previous state
    rightSide = downRight;
    leftSide = downLeft;

#if STATUS_DUMP
    bool justRight = !hwstate::swcmp(rightSide.switches, just_right_stat);
    bool leftCheck = !hwstate::swcmp(leftSide.switches, status_keys_left);
    bool rightCheck = !hwstate::swcmp(rightSide.switches, status_keys_right);
    // Check for hardware request thingamajig:
    // This is hard coded, mostly because I'm just hacking
    if (justRight || (leftCheck && rightCheck)) {
      if (!justRight) {
        type_string("Left battery level: ");
        type_number(leftSide.battery_level);
        type_string("%\n");
      }
      type_string("Right battery level: ");
      type_number(rightSide.battery_level);
      type_string("%\nLayer stack: ");
      for (int i = 0; i <= layer_pos; i++) {
        type_string(layer_names[layer_stack[i]]);
        type_string(" (");
        type_number(i);
        type_string(i == layer_pos ? ")" : "), ");
      }
      type_string("\n");
    }
#endif
  }
  waitForEvent(); // Request CPU enter low-power mode until an event occurs
}

#if STATUS_DUMP
// A very limited version of typing the string. It dumps lower case, nubmers,
// a few other things, defaults to '.' for everything else.
void type_string(const char* str) {
  uint8_t console[6] = {0, 0, 0, 0, 0, 0};
  char p = 0;
  bool shift;
  while (*str) {
    shift = false;
    char c = *str++;
    char n = 0;
    if (c >= 'a' && c <= 'z')
      n = HID_KEY_A + c - 'a';
    else if (c >= 'A' && c <= 'Z') {
      n = HID_KEY_A + c - 'A';
      shift = true;
    } else if (c >= '1' && c <= '9')
      n = HID_KEY_1 + c - '1';
    else {
      switch (c) {
        case '0':
          n = HID_KEY_0;
          break;
        case ' ':
          n = HID_KEY_SPACE;
          break;
        case '\n':
        case '\r':
          n = HID_KEY_RETURN;
          break;
        case ':':
          shift = true;
        case ';':
          n = HID_KEY_SEMICOLON;
          break;
        case ',':
          n = HID_KEY_COMMA;
          break;
        case '(':
          n = HID_KEY_9;
          shift = true;
          break;
        case ')':
          n = HID_KEY_0;
          shift = true;
          break;
        case '-':
          n = HID_KEY_MINUS;
          break;
        case '%':
          n = HID_KEY_5;
          shift = true;
          break;
        default:
          n = HID_KEY_PERIOD;
      }
    }
    if (n == p) {
      console[0] = 0;
      hid.keyboardReport(0, console);
    }
    console[0] = n;
    hid.keyboardReport(shift ? 2 : 0, console);
    p = n;
  }
  // Clear any final key out, just to be safe
  console[0] = 0;
  hid.keyboardReport(0, console);
}

void type_number(uint32_t val) {
  int p = -1;
  char buffer[25];
  int curPos = 0;
  do {
    int v;
    int digit = val % 10;
    val = val / 10;
    if (digit == 0) {
      v = HID_KEY_0;
    } else {
      v = HID_KEY_1 - 1 + digit;
    }
    if (v == p) {
      buffer[curPos++] = 0;
    }
    buffer[curPos++] = v;
    p = v;
  } while (val);
  uint8_t console[6] = {0, 0, 0, 0, 0, 0};
  do {
    console[0] = buffer[--curPos];
    hid.keyboardReport(0, console);
  } while (curPos);
  // Clear any final key out, just to be safe
  console[0] = 0;
  hid.keyboardReport(0, console);
}
#endif
