#include "shared.h"

// Globals
BLEDis dis;
BLEHidAdafruit hid;
BLEClientUart clientUart;
BLEBas battery;

hwstate leftSide{};
hwstate rightSide{};

// Declarations
void resetKeyMatrix();
void cent_connect_callback(uint16_t conn_handle);
void cent_disconnect_callback(uint16_t conn_handle, uint8_t reason);
void scan_callback(ble_gap_evt_adv_report_t* report);
void startAdv();
uint32_t resolveActionForScanCodeOnActiveLayer(uint8_t scanCode);

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

  Bluefruit.setTxPower(0);
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
  (void)conn_handle;
  (void)reason;
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

using action_t = uint32_t;

struct keystate {
  uint32_t lastChange;
  action_t action;
  scancode_t scanCode;
  bool down;
  bool update(scancode_t sc, bool pressed, uint32_t now) {
    if (scanCode == sc) {
      // Update the transition time, if any
      if (down != pressed) {
        lastChange = now;
        down = pressed;
        if (pressed) {
          action = resolveActionForScanCodeOnActiveLayer(scanCode);
        }
        return true;
      }
    } else {
      // We claimed a new slot, so set the transition
      // time to the current time.
      down = pressed;
      scanCode = sc;
      lastChange = now;
      if (pressed) {
        action = resolveActionForScanCodeOnActiveLayer(scanCode);
      }
      return true;
    }
    return false;
  };
#if DEBUG
  void dump() const {
    Serial.print("ScanCode=");
    Serial.print(scanCode, HEX);
    Serial.print(" down=");
    Serial.print(down);
    Serial.print(" lastChange=");
    Serial.print(lastChange);
    Serial.print(" action=");
    Serial.print(action, HEX);
    Serial.println("");
  };
#endif
};

constexpr uint32_t kMask = 0xf00;
constexpr uint32_t kKeyPress = 0x100;
constexpr uint32_t kModifier = 0x200;
constexpr uint32_t kLayer = 0x300;
constexpr uint32_t kTapHold = 0x400;
constexpr uint32_t kToggleMod = 0x500;
constexpr uint32_t kKeyAndMod = 0x600;
constexpr uint32_t kConsumer = 0x800;

#define PASTE(a, b) a##b

#define ___ 0
#define KEY(a) kKeyPress | PASTE(HID_KEY_, a)
#define MOD(a) kModifier | PASTE(KEYBOARD_MODIFIER_, a)
#define TMOD(a) kToggleMod | PASTE(KEYBOARD_MODIFIER_, a)
#define CONS(a) kConsumer | a

#define TAPH(a, b) \
  kTapHold | PASTE(HID_KEY_, a) | (PASTE(KEYBOARD_MODIFIER_, b) << 16)
#define KANDMOD(a, b) \
  kKeyAndMod | PASTE(HID_KEY_, a) | (PASTE(KEYBOARD_MODIFIER_, b) << 16)
#define LAYER(n) kLayer | n

#define LROW1(l00, l01, l02, l03, l04, l05, l06) \
  ___, l06, l05, l04, l03, l02, l01, l00
#define LROW2(l10, l11, l12, l13, l14, l15, l16) \
  ___, l16, l15, l14, l13, l12, l11, l10
#define LROW3(l20, l21, l22, l23, l24, l25, l26, lt01) \
  lt01, l26, l25, l24, l23, l22, l21, l20
#define LROW4(l30, l31, l32, l33, l34, l35, lt00, lt12) \
  lt12, lt00, l35, l34, l33, l32, l31, l30
#define LROW5(l40, l41, l42, l43, l44, lt10, lt11, lt22) \
  lt22, lt11, lt10, l44, l43, l42, l41, l40

#define RROW1(r00, r01, r02, r03, r04, r05, r06) \
  r06, r05, r04, r03, r02, r01, r00, ___
#define RROW2(r10, r11, r12, r13, r14, r15, r16) \
  r16, r15, r14, r13, r12, r11, r10, ___
#define RROW3(rt00, r20, r21, r22, r23, r24, r25, r26) \
  r26, r25, r24, r23, r22, r21, r20, rt00
#define RROW4(rt10, rt01, r31, r32, r33, r34, r35, r36) \
  r36, r35, r34, r33, r32, r31, rt01, rt10
#define RROW5(rt20, rt11, rt12, r42, r43, r44, r45, r46) \
  r46, r45, r44, r43, r42, rt12, rt11, rt20

keystate keyStates[16];
uint8_t layer_stack[8];
uint8_t layer_pos = 0;

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

// Some missing keycodes from the Arduino/AdaFruit API's that I need
// HINT: You can find these from the QMK firmware HIDClassCommon.h file :)
#define HID_KEY_M_PLAY 0xCD
#define HID_KEY_M_PREVIOUS_TRACK 0xB6
#define HID_KEY_M_NEXT_TRACK 0xB5

#define HID_KEY_M_VOLUME_UP 0x80
#define HID_KEY_M_VOLUME_DOWN 0x81
#define HID_KEY_M_MUTE 0x7F

#define HID_KEY_M_BACKWARD 0xF1
#define HID_KEY_M_FORWARD 0xF2
#define HID_KEY_M_SLEEP 0xF8
#define HID_KEY_M_LOCK 0xF9

// Some stuff to make the action maps prettier
#define LCMD MOD(LEFTGUI)
#define LSHFT MOD(LEFTSHIFT)
#define LCTL MOD(LEFTCTRL)
#define LOPT MOD(LEFTALT)
#define RCMD MOD(RIGHTGUI)
#define RSHFT MOD(LEFTSHIFT)
#define RCTL MOD(RIGHTSHIFT)
#define ROPT MOD(RIGHTALT)

#define GRV GRAVE
#define OBRC BRACKET_LEFT
#define CBRC BRACKET_RIGHT
#define BKSP BACKSPACE
#define DEL DELETE
#define PGUP PAGE_UP
#define PGDN PAGE_DOWN
#define SEMI_ KEY(SEMICOLON)
#define COMMA_ KEY(COMMA)
#define DOT_ KEY(PERIOD)
#define QUOTE_ KEY(APOSTROPHE)
#define ENTER_ KEY(RETURN)
#define UP_ KEY(ARROW_UP)
#define DOWN_ KEY(ARROW_DOWN)
#define LEFT_ KEY(ARROW_LEFT)
#define RIGHT_ KEY(ARROW_RIGHT)
#define SPACE_ KEY(SPACE)
#define M_MUTE KEY(M_MUTE)
#define M_VOLU KEY(M_VOLUME_UP)
#define M_VOLD KEY(M_VOLUME_DOWN)

// Some of these are available as symbols in the AdaFruit API, but I'm just
// using them as they exist from IOHIDUsageTable.h from the IOHIDFamily source
// code available at www.opensource.apple.com.  I'm pretty sure similar stuff is
// available for Windows, too, somewhere (probably in MSDN docs)
#define M_PLAY CONS(0xcd)
#define M_PREV CONS(0xb6)
#define M_NEXT CONS(0xb5)

const action_t keymap[][numcols * numrows * 2] = {
    {LROW1(KEY(ESCAPE), KEY(1), KEY(2), KEY(3), KEY(4), KEY(5), M_VOLD),
     LROW2(KEY(TAB), KEY(Q), KEY(W), KEY(E), KEY(R), KEY(T), M_PLAY),
     LROW3(LCMD, KEY(A), KEY(S), KEY(D), KEY(F), KEY(G), KEY(GRAVE), LCMD),
     LROW4(LSHFT, KEY(Z), KEY(X), KEY(C), KEY(V), KEY(B), M_PREV, KEY(HOME)),
     LROW5(LCTL, LOPT, LCMD, M_MUTE, KEY(OBRC), KEY(BKSP), KEY(DEL), KEY(END)),

     RROW1(M_VOLU, KEY(6), KEY(7), KEY(8), KEY(9), KEY(0), KEY(MINUS)),
     RROW2(ROPT, KEY(Y), KEY(U), KEY(I), KEY(O), KEY(P), KEY(BACKSLASH)),
     RROW3(RCMD, KEY(EQUAL), KEY(H), KEY(J), KEY(K), KEY(L), SEMI_, QUOTE_),
     RROW4(KEY(PGUP), M_NEXT, KEY(N), KEY(M), COMMA_, DOT_, KEY(SLASH), RSHFT),
     RROW5(KEY(PGDN), ENTER_, SPACE_, KEY(CBRC), LEFT_, UP_, DOWN_, RIGHT_)}};

uint32_t resolveActionForScanCodeOnActiveLayer(uint8_t scanCode) {
  int s = layer_pos;

  while (s >= 0 && keymap[layer_stack[s]][scanCode] == ___) {
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
#endif

void loop() {
  uint32_t now = millis();

  // Get the hardware state for the two sides...
  hwstate downRight{now, rightSide};
  hwstate downLeft{clientUart, leftSide};

  // Update the combined battery level
  if (downRight.battery_level != rightSide.battery_level ||
      downLeft.battery_level != leftSide.battery_level) {
    // We only get the battery level once you hit a key, so only report it if we
    // have something to actually report
    if (downLeft.battery_level) {
      battery.notify((downRight.battery_level + downLeft.battery_level) / 2);
    } else {
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
      // Add 40 to the right scan code...
      sc = getNextScanCode(deltaRight, afterRight, pressed) + 8 * numrows;
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
    state->update(sc, pressed, now);
    // Something to deal with layer changing should get called here.
    // I think state->update() ought to communicate what to do, probably.
    // But I'm not trying anything fancy, yet, so I'm just leaving this alone
  }

#if 0
  // This was Wez's matrix reading & state transition code. I'm starting out a bit
  // simpler, and need to eventually add some more complexity :/

  for (int rowNum = 0; rowNum < numrows; ++rowNum) {
    for (int colNum = 0; colNum < 2 * numcols; ++colNum) {
      auto scanCode = (rowNum * 2 * numcols) + colNum;
      auto isDown = down.rows[rowNum] & (1 << colNum);
      auto wasDown = lastRead.rows[rowNum] & (1 << colNum);

      if (isDown == wasDown) {
        continue;
      }
      keysChanged = true;

      auto state = stateSlot(scanCode, now);
      if (isDown && !state) {
        // Silently drop this key; we're tracking too many
        // other keys right now
        continue;
      }
      DBG(printState(state));

      bool isTransition = false;

      if (state) {
        if (state->scanCode == scanCode) {
          // Update the transition time, if any
          if (state->down != isDown) {
            state->lastChange = now;
            state->down = isDown;
            if (isDown) {
              state->action = resolveActionForScanCodeOnActiveLayer(scanCode);
            }
            isTransition = true;
          }
        } else {
          // We claimed a new slot, so set the transition
          // time to the current time.
          state->down = isDown;
          state->scanCode = scanCode;
          state->lastChange = now;
          if (isDown) {
            state->action = resolveActionForScanCodeOnActiveLayer(scanCode);
          }
          isTransition = true;
        }

        if (isTransition) {
          switch (state->action & kMask) {
            case kLayer:
              if (state->down) {
                // Push the new layer stack position
                layer_stack[++layer_pos] = state->action & 0xff;
              } else {
                // Pop off the layer stack
                --layer_pos;
              }
              break;
          }
        }
      }
    }
  }
#endif

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
    DBG2(Serial.print("Right side "));
    DBG2(downRight.dump());

    // Update the hardware previous state
    rightSide = downRight;
    leftSide = downLeft;
  }
  waitForEvent(); // Request CPU enter low-power mode until an event occurs
}
