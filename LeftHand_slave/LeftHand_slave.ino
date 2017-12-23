#include "shared.h"

BLEDis bledis;
BLEUart bleuart;

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

  startAdv();
}

void startAdv(void) {
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

matrix_t lastRead = {0, 0, 0, 0, 0};

matrix_t read_matrix() {
  matrix_t matrix = {0, 0, 0, 0, 0};

  for (int colNum = 0; colNum < numcols; ++colNum) {
    digitalWrite(colPins[colNum], LOW);

    // TODO: assert that numscols <= 8
    for (int rowNum = 0; rowNum < numrows; ++rowNum) {
      if (!digitalRead(rowPins[rowNum])) {
        matrix.rows[rowNum] |= 1 << colNum;
      }
    }

    digitalWrite(colPins[colNum], HIGH);
  }

  return matrix;
}

void loop() {
  auto down = read_matrix();
  constexpr int numreps = 16;
  uint8_t report[numreps];
  uint8_t repsize = 0;

  for (int rowNum = 0; rowNum < numrows && repsize < numreps; ++rowNum) {
    for (int colNum = 0; colNum < numcols && repsize < numreps; ++colNum) {
      auto mask = 1 << colNum;
      auto current = lastRead.rows[rowNum] & mask;
      auto thisScan = down.rows[rowNum] & mask;
      if (current != thisScan) {
        auto scanCode = (rowNum * numcols) + colNum;
        if (thisScan) {
          // TODO: Again, assert that our scan code all fits in here...
          scanCode |= 0x80; // Set the high bit
        }
        report[repsize++] = scanCode;
      }
    }
  }

  if (repsize) {
    lastRead = down;
#if DEBUG
    Serial.print("repsize=");
    Serial.print(repsize);
    Serial.print(" ");
    for (int i = 0; i < repsize; i++) {
      Serial.print(report[i], HEX);
      Serial.print(" ");
    }
    Serial.print("\r\n");
#endif
    bleuart.write(report, repsize);
  }
  // Request CPU to enter low-power mode until an event/interrupt occurs
  waitForEvent();
}

void rtos_idle_callback(void) {}
