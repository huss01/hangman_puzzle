#include "rfid1.h"
#include <EEPROM.h>

// Related to the code to open the box
int misoPin[] = {8, 9, 10, 11, 12, 13};

#define RST_PIN A1
#define IRQ_PIN A2
#define MOSI_PIN A3
#define SCK_PIN A4
#define NSS_PIN A5

RFID1 rfid;
unsigned long masterId = 0x7d3f719aUL;
unsigned long correct[6][2];
unsigned long entered[6];

// Related to hangman
const int outputData = 5;
const int outputLatch = 6;
const int outputClock = 7;

const int letterData = 2;
const int letterLatch = 3;
const int letterClock = 4;

const int debounceTime = 50;

int buttonVoltage[] = {1023, 1001, 969, 930, 836, 696, 512};
int currentButton = 7;

byte hangmanParts = 0;
byte revealedLetters = 0;

bool hangmanDone = false;

// Which mode are we in?
enum mode {ENTERING_CODE, PLAYING_HANGMAN};
enum mode currentMode = ENTERING_CODE;

void setup() {
  Serial.begin(9600);
  setupCode();
  setupHangman();
}

void loop() {
  if (currentMode == ENTERING_CODE)
    codeLoop();
  else
    hangmanLoop();
}

// Code functions
void setupCode(void) {
  int i, j;
  int eepromAddress = 0;

  // Fetch correct RFID ids from EEPROM
  for (i = 0; i < 6; ++i)
    for (j = 0; j < 2; ++j) {
      EEPROM.get(eepromAddress, correct[i][j]);
      eepromAddress += sizeof (unsigned long);
      if (correct[i][j] != (unsigned long) -1) {
        Serial.print("Correct id for ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(correct[i][j], HEX);
        if (j == 1)
          Serial.println(" (alt)");
        else
          Serial.println();
      }
    }
}

void codeLoop(void) {
  for (int i = 0; i < 6; ++i)
    checkRfid(i);
  if (correctEntered()) {
    Serial.println("Correct code entered, starting hangman");
    currentMode = PLAYING_HANGMAN;
    openKeyboard();
  }
}

void checkRfid(int i) {
  unsigned long id;

  id = readRfidId(i);
  if (id == masterId)
    storeCorrect(i);
  else
    entered[i] = id;
}

unsigned long readRfidId(int i) {
  uchar status;
  uchar str[MAX_LEN];
  unsigned long id = 0;

  rfid.begin(IRQ_PIN, SCK_PIN, MOSI_PIN, misoPin[i], NSS_PIN, RST_PIN);
  delay(100);
  rfid.init();

  status = rfid.request(PICC_REQIDL, str);
  if (status != MI_OK)
    id = (unsigned long) -1;
  else {
    status = rfid.anticoll(str);
    if (status == MI_OK)
      for (int j = 0; j < 4; ++j)
        id = (id << 8) | str[j];
  }
  delay(100);
  rfid.halt();
  return id ? id : (unsigned long) -1;
}

void storeCorrect(int i) {
  unsigned long currentId = masterId;
  unsigned long prevId;
  int correctAlt = 0;
  int eepromAddress;

  indicateProgramming(i);
  while (currentId == masterId)
    currentId = readRfidId(i);
  while (currentId != masterId) {
    if (correctAlt < 2 && currentId != (unsigned long) -1) {
      Serial.print("New correct id (");
      Serial.print(i);
      Serial.print(",");
      Serial.print(correctAlt);
      Serial.print("): ");
      Serial.println(currentId, HEX);
      eepromAddress = (2 * i + correctAlt) * sizeof (unsigned long);
      EEPROM.put(eepromAddress, currentId);
      correct[i][correctAlt] = currentId;
      ++correctAlt;
    } else if (currentId != (unsigned long) -1) {
      Serial.print("Ignoring correct id number ");
      Serial.print(correctAlt);
      Serial.print(" (value: ");
      Serial.print(currentId, HEX);
      Serial.println(")");
      ++correctAlt;
    }
    prevId = currentId;
    while (currentId == prevId)
      currentId = readRfidId(i);
  }
  while (correctAlt < 2) {
    eepromAddress = (2 * i + correctAlt) * sizeof (unsigned long);
    EEPROM.put(eepromAddress, (unsigned long) -1);
    ++correctAlt;
  }
  while (currentId == masterId)
    currentId = readRfidId(i);
  indicateProgrammingDone(i);
}

void indicateProgramming(int i) {
  Serial.print("Programming mode entered for ");
  Serial.println(i);
}

void indicateProgrammingDone(int i) {
  Serial.print("Programming mode left for ");
  Serial.println(i);
}

bool correctEntered(void) {
  for (int i = 0; i < 6; ++i)
    if (entered[i] == (unsigned long) -1 ||
        entered[i] != correct[i][0] &&
        entered[i] != correct[i][1])
      return false;
  return true;
}

void waitForClearCode(void) {
  bool allClear = false;
  int i;

  while (! allClear) {
    allClear = true;
    i = 0;
    while (i < 6 && allClear)
      if (readRfidId(i) != (unsigned long) -1)
        allClear = false;
      else
        ++i;
  }
}

// Hangman functions
void setupHangman(void) {
  pinMode(outputData, OUTPUT);
  pinMode(outputClock, OUTPUT);
  pinMode(outputLatch, OUTPUT);
  pinMode(letterData, OUTPUT);
  pinMode(letterClock, OUTPUT);
  pinMode(letterLatch, OUTPUT);
  reset();
}

void hangmanLoop(void) {
  int prevButton = -1;
  int button = readButton(A0);

  if (button != currentButton) {
    // Debounce loop
    while (prevButton != button) {
      delay(50);
      prevButton = button;
      button = readButton(A0);
    }
    if (button != currentButton && button < 7) {
      Serial.print("Button ");
      Serial.print(button);
      Serial.println(" pressed");
      handleButton(button);
    }
  }
  currentButton = button;
  if (hangmanDone) {
    waitForClearCode();
    hangmanDone = false;
    currentMode = ENTERING_CODE;
    reset();
  }
}

int readButton(int pin) {
  int voltage;
  int i = 0;

  voltage = analogRead(A0);
  for (i = 0; i < 7; ++i)
    if (voltage < buttonVoltage[i] + 10 && voltage > buttonVoltage[i] - 10)
      break;
  return i;
}

void handleButton(int button) {
  if (button == 0) {
    if (hangmanParts != 0x3f) {
      hangmanParts = (hangmanParts << 1) | 1;
      writeToRelays(outputLatch, outputData, outputClock, hangmanParts);
    } else {
      openHatch();
      hangmanDone = true;
    }
  } else {
    revealedLetters |= 1 << (button - 1);
    writeToRelays(letterLatch, letterData, letterClock, revealedLetters);
    if (revealedLetters == 0x7f) {
      delay(500);
      reset();
    }
  }
}

void writeToRelays(int latchPin, int dataPin, int clockPin, byte data) {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, data);
  digitalWrite(latchPin, HIGH);
}

void reset(void) {
  hangmanParts = 0;
  if (currentMode == ENTERING_CODE)
    revealedLetters = 0;
  else
    revealedLetters = 1 << 6;
  currentButton = 7;
  writeToRelays(outputLatch, outputData, outputClock, hangmanParts);
  writeToRelays(letterLatch, letterData, letterClock, revealedLetters);
}

void openHatch(void) {
  Serial.println("Hatch opened!");
}

void openKeyboard(void) {
  writeToRelays(letterLatch, letterData, letterClock, 1 << 7);
  delay(500);
  reset();
}
