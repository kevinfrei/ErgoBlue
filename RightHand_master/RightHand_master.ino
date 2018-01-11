#include "shared.h"

#include "keystate.h"
#include "keyhelpers.h"
#include "keymap.h"

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

action_t resolveActionForScanCodeOnActiveLayer(uint8_t scanCode) {
  int s = layer_pos;

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
#endif

void loop() {
  uint32_t now = micros();

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
