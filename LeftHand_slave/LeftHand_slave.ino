#include "shared.h"

using matrix_t = matrix_type<scancode_t>;

BLEDis bledis;
BLEUart bleuart;
matrix_t lastRead{};

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
}

void loop() {
  matrix_t down = matrix_t::read();
  scancode_t report[numreps];
  uint8_t repsize = 0;

  for (uint8_t rowNum = 0; rowNum < numrows && repsize < numreps; ++rowNum) {
    for (uint8_t colNum = 0; colNum < numcols && repsize < numreps; ++colNum) {
      scancode_t current = lastRead.get_switch(rowNum, colNum);
      scancode_t thisScan = down.get_switch(rowNum, colNum);
      if (current != thisScan) {
        report[repsize++] = make_scan_code(rowNum, colNum, thisScan);
      }
    }
  }

  if (repsize) {
    lastRead = down;
#if DEBUG
    Serial.print("repsize=");
    Serial.print(repsize);
    for (uint8_t i = 0; i < repsize; i++) {
      Serial.print(" ");
      Serial.print(report[i], HEX);
    }
    Serial.println("");
#endif
    bleuart.write(report, repsize);
  }
  waitForEvent(); // Request CPU enter low-power mode until an event occurs
}
