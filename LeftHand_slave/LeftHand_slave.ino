#include "shared.h"

BLEDis bledis;
BLEUart bleuart;

keyboard_state lastRead{};

void setup() {
  shared_setup();
  Bluefruit.begin();
  Bluefruit.autoConnLed(false);
  Bluefruit.setTxPower(0);
  Bluefruit.setName(BT_NAME "-LHS");

  bledis.setManufacturer(MANUFACTURER);
  bledis.setModel(MODEL);
  bledis.begin();

  bleuart.begin();

  // Start Advertising the UART service to talk to the other half...
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30); // number of seconds in fast mode
  Bluefruit.Advertising.start(0); // 0 = Don't stop advertising after n seconds
  DBG(Serial.println("test"));
}

void loop() {
  keyboard_state down{lastRead, millis()};
  if (down != lastRead) {
    lastRead = down;
    down.send(bleuart);
  }
  waitForEvent(); // Request CPU enter low-power mode until an event occurs
}
