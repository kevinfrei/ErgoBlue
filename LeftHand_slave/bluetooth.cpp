#include <bluefruit.h>

#define DEBUG 0

BLEDis bledis;
BLEUart bleuart;

static const int colPins[] = {16,15,7,11,30,27};
static const int rowPins[] = {2,3,4,5,28,29};

void setup() 
{
  for (auto &pin: rowPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
  for (auto &pin: colPins) {
    pinMode(pin, INPUT_PULLUP);
  }
  
#if DEBUG
Serial.begin(115200);
#endif

  Bluefruit.begin();
  Bluefruit.autoConnLed(false);
  Bluefruit.setTxPower(0);
  Bluefruit.setName("FreikErgoMod LHS");

  bledis.setManufacturer("FreikyStuff");
  bledis.setModel("ErgoMod");
  bledis.begin();

  bleuart.begin();

  startAdv();
}

void startAdv(void)
{  
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

struct matrix_t {
  uint8_t rows[6];
};

struct matrix_t lastRead = {0,0,0,0,0,0};

struct matrix_t read_matrix() {
  matrix_t matrix = {0,0,0,0,0,0};

 for (int rowNum = 0; rowNum < 6; ++rowNum) {
    digitalWrite(rowPins[rowNum], LOW);

    for (int colNum = 0; colNum < 6; ++colNum) {
      if (!digitalRead(colPins[colNum])) {
        matrix.rows[rowNum] |= 1 << colNum;    
      }
    }

    digitalWrite(rowPins[rowNum], HIGH);
  } 

  return matrix;
}

void loop() 
{
  auto down = read_matrix();
  uint8_t report[16];
  uint8_t repsize = 0;

  for (int rowNum = 0; rowNum < 6; ++rowNum) {
    for (int colNum = 0; colNum < 6; ++colNum) {
      auto mask = 1 << colNum;
      auto current = lastRead.rows[rowNum] & mask;
      auto thisScan = down.rows[rowNum] & mask;
      if (current != thisScan) {
        auto scanCode = (rowNum * 6) + colNum;
        if (thisScan) {
          scanCode |= 0b10000000;
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

void rtos_idle_callback(void)
{
}

