using action_t = uint32_t;

constexpr uint32_t kMask = 0xf00;
constexpr uint32_t kKeyPress = 0x100;
constexpr uint32_t kModifier = 0x200;
constexpr uint32_t kTapHold = 0x300;
constexpr uint32_t kToggleMod = 0x400;
constexpr uint32_t kKeyAndMod = 0x500;
// This works like a shift key
constexpr uint32_t kLayerMod = 0x600;
// This turns the layer on or off
constexpr uint32_t kLayerTog = 0x700;
// This is for flagging consumer keycodes, as I have to handle them differently
constexpr uint32_t kConsumer = 0x8000;
action_t resolveActionForScanCodeOnActiveLayer(uint8_t scanCode);

struct keystate {
  // The time this keystate was changed (using millis())
  uint32_t lastChange;
  // The action this key state is referring to.
  // This comes from the keymap when the key is pressed.
  action_t action;
  // The scan code of the key this action is about
  scancode_t scanCode;
  // Is this a press or release?
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
