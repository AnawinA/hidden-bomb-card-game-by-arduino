#include "Variables.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <map>
#include <set>


// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// RFID 
#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN);

// Pins
const uint8_t PIN_BEEP = 8;
const uint8_t PIN_BTN_LEFT = 2;
const uint8_t PIN_BTN_GO = 4;
const uint8_t PIN_BTN_RIGHT = 7;

// RGB pins (PWM)
const uint8_t PIN_R = 3;
const uint8_t PIN_G = 5;
const uint8_t PIN_B = 6;

// Program states
enum AppState { STATE_LOADING, STATE_START, STATE_MENU,
  STATE_BOMB_OR_NOT, STATE_CARD_ID, STATE_SKILLS };


// Menu items
const char* menuItems[] = {
  "Bomb Roulette", "Hidden Defuse", "Hold a Bomb",
  "Bomb Dungeon", "Show Card ID"};
const int menuCount = 5;
int currentSelection = 0;
const int ADDR_SELECTION = 0;

// Debounce helpers
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;
bool lastGoRead = LOW;
bool goPressed = false;

// Slower fake loading
const unsigned long LOADING_DURATION_MS = 1000;

enum GameState {SET_BOMB, SCAN_CARD, RESULT};
AppState state = STATE_LOADING;

unsigned long timeLimit_hiddenDefuse = 10000; 
const int ADDR_TIME_LIMIT_HIDDEN_DEFUSE = 1;


// ------------------------------------------------------------------
//  Simple Bomb Icon 32x32 (monochrome bitmap)
// ------------------------------------------------------------------

const unsigned char bomb32 [] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x0F,0xFF,0x00,0x00,0x00,0x00,0x00,
  0x00,0x7F,0xFF,0xE0,0x00,0x00,0x00,0x00,
  0x00,0xFF,0xFF,0xF0,0x00,0x00,0x00,0x00,
  0x01,0xFF,0xFF,0xF8,0x00,0x00,0x00,0x00,
  0x03,0xFF,0xFF,0xFC,0x00,0x00,0x00,0x00,
  0x03,0xFF,0xFF,0xFC,0x00,0x00,0x00,0x00,
  0x07,0xFF,0xFF,0xFE,0x00,0x00,0x00,0x00,
  0x07,0xFF,0xFF,0xFE,0x00,0x00,0x00,0x00,
  0x07,0xFF,0xFF,0xFE,0x00,0x00,0x00,0x00,
  0x03,0xFF,0xFF,0xFC,0x00,0x00,0x00,0x00,
  0x01,0xFF,0xFF,0xF8,0x00,0x00,0x00,0x00,
  0x00,0xFF,0xFF,0xF0,0x00,0x00,0x00,0x00,
  0x00,0x7F,0xFF,0xE0,0x00,0x00,0x00,0x00,
  0x00,0x0F,0xFF,0x00,0x00,0x00,0x00,0x00
};

const unsigned char shield32 [] PROGMEM = {
  0x00,0x7F,0xF0,0x00,0x00,0x00,0x00,0x00,
  0x03,0xFF,0xFC,0x00,0x00,0x00,0x00,0x00,
  0x07,0xFF,0xFE,0x00,0x00,0x00,0x00,0x00,
  0x0F,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,
  0x1F,0xFF,0xFF,0x80,0x00,0x00,0x00,0x00,
  0x1F,0xFF,0xFF,0x80,0x00,0x00,0x00,0x00,
  0x3F,0xFF,0xFF,0xC0,0x00,0x00,0x00,0x00,
  0x3F,0xFF,0xFF,0xC0,0x00,0x00,0x00,0x00,
  0x3F,0xFF,0xFF,0xC0,0x00,0x00,0x00,0x00,
  0x1F,0xFF,0xFF,0x80,0x00,0x00,0x00,0x00,
  0x1F,0xFF,0xFF,0x80,0x00,0x00,0x00,0x00,
  0x0F,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,
  0x07,0xFF,0xFE,0x00,0x00,0x00,0x00,0x00,
  0x03,0xFF,0xFC,0x00,0x00,0x00,0x00,0x00,
  0x00,0x7F,0xF0,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};


// Bomb card icon (card with ⚠️ inside)
static const unsigned char PROGMEM icon_bomb[] = {
  0x00,0x7E,0x42,0x5A,0x5A,0x42,0x7E,0x00
};
static const unsigned char PROGMEM icon_players[] = {
  0x3C,0x42,0xA5,0x81,0xA5,0x99,0x42,0x3C
};
static const unsigned char PROGMEM icon_question[] = {
  0x00,0x3C,0x42,0x02,0x1C,0x00,0x18,0x18
};
// ------------------------------------------------------------------

void setupPins() {
  pinMode(PIN_BEEP, OUTPUT);
  digitalWrite(PIN_BEEP, LOW);

  pinMode(PIN_BTN_LEFT, INPUT);
  pinMode(PIN_BTN_GO, INPUT);
  pinMode(PIN_BTN_RIGHT, INPUT);

  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);

  Wire.begin();
  Wire.setClock(50000);

  gyr(1, 1, 1);
}

void setup() {
  Serial.begin(115200);
  setupPins();

  SPI.begin();
  rfid.PCD_Init();

  if (!display.begin(0x3C, true)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;) ;
  }
  display.clearDisplay();
  display.display();

  drawLoadingAnimation();
  state = STATE_START;

  currentSelection = EEPROM.read(ADDR_SELECTION);
  if (currentSelection < 0 || currentSelection >= menuCount) {
    currentSelection = 0; // fallback safety
  }

  showStartScreen();
}

void loop() {
  switch (state) {
    case STATE_START:
      pollGoButton();
      if (goPressed) {
        goPressed = false;
        // short beep on press
        tone(PIN_BEEP, 1000, 150);
        state = STATE_MENU;
        
        loadingScreen();
        delay(500);
        showMenuScreen();
      }
      break;

    case STATE_MENU:
        if (digitalRead(PIN_BTN_RIGHT) == HIGH) {
          if (currentSelection < menuCount - 1) {
            currentSelection++;
            showMenuScreen();
            delay(200);
          }
        }
        if (digitalRead(PIN_BTN_LEFT) == HIGH) {
          if (currentSelection > 0) {
            currentSelection--;
            showMenuScreen();
            delay(200);
          }
        }
        pollGoButton();
        if (goPressed) {
          goPressed = false;
          tone(PIN_BEEP, 1000, 100);
          Serial.println(currentSelection);
          EEPROM.update(ADDR_SELECTION, currentSelection);  // Save to EEPROM
          switch (currentSelection) {
            case 0:
              state = STATE_BOMB_OR_NOT;
              break;
            case 1:
              hiddenDefuse();
              break;
            case 2:
              holdABomb();
              break;
            case 3:
              BombDungeon();
              break;
            case 4:
              state = STATE_CARD_ID;
              break;
          }
        }

        switch (currentSelection % 3) {
          case 0:
            gyr(1, 0, 0);
            break;
          case 1:
            gyr(0, 1, 0);
            break;
          case 2:
            gyr(0, 0, 1);
            break;
        }

      break;
    
    case STATE_BOMB_OR_NOT:
      BombRouletteGame();
      break;

    case STATE_CARD_ID:
      cardIDFeature();
      break;

  }
}

// ------------------------------------------------------------------
// UI drawing functions
// ------------------------------------------------------------------
void drawLoadingAnimation() {
  const int steps = 12;
  unsigned long start = millis();
  for (int i = 0; i <= steps; ++i) {
    float frac = (float)i / steps;
    unsigned long elapsed = millis() - start;
    if (elapsed > LOADING_DURATION_MS) break;

    display.clearDisplay();

    // Draw bomb icon centered at top
    int bombX = (SCREEN_WIDTH - 32) / 2;
    display.drawBitmap(bombX, 0, bombBitmap_2, 32, 32, SH110X_WHITE);

    // Title text
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(28, 36);
    display.println(F("Hidden Bomb"));

    // Progress bar
    int barX = 10;
    int barY = 52;
    int barW = SCREEN_WIDTH - 2 * barX;
    int barH = 8;
    display.drawRect(barX, barY, barW, barH, SH110X_WHITE);
    int fillW = (int)(barW * frac + 0.5);
    if (fillW > 0) display.fillRect(barX + 1, barY + 1, max(0, fillW - 2), barH - 2, SH110X_WHITE);

    display.display();

    unsigned long now = millis();
    unsigned long target = start + (unsigned long)((i + 1) * (LOADING_DURATION_MS / (float)steps));
    if (target > now) delay(target - now);
  }

  delay(300);
}

bool firstStartScreen = true;

void showStartScreen() {
  display.clearDisplay();

  // Draw title
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("HIDDEN"));
  display.println(F("BOMB"));

  display.drawBitmap(88, 0, epd_bitmap_game_icon, 32, 32, SH110X_WHITE);
  

  display.setTextSize(1);
  display.setCursor(10, 48);
  display.println(F("Press [GO] to start"));

  display.display();
}

void showMenuScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(F("MENU"));

  display.setTextSize(1);
  int y = 40;
  
  // Show previous item if exists
  if (currentSelection > 0) {
    display.setCursor(20, y - 10);
    display.print(menuItems[currentSelection - 1]);
  }

  // Hovered item with arrow
  display.setCursor(10, y);
  display.print(">");
  display.setCursor(20, y);
  display.print(menuItems[currentSelection]);

  // Show next item if exists
  if (currentSelection < menuCount - 1) {
    display.setCursor(20, y + 10);
    display.print(menuItems[currentSelection + 1]);
  }

  int miniIconX = 100;
  switch (currentSelection) {
    case 0:
      display.drawBitmap(miniIconX, y, icon_bomb, 8, 8, SH110X_WHITE);
      break;
    case 1:
      display.drawBitmap(miniIconX, y, icon_players, 8, 8, SH110X_WHITE);
      break;
    case 2:
      display.drawBitmap(miniIconX, y, icon_question, 8, 8, SH110X_WHITE);
      break;
  }

  display.display();
}

void longBeep() {
  tone(PIN_BEEP, 1000);
  delay(1000);
  noTone(PIN_BEEP);
}

void shortBeep() {
  tone(PIN_BEEP, 1000);
  delay(100);
  noTone(PIN_BEEP);
}

void doubleBeep() {
  tone(PIN_BEEP, 1000, 50);
  delay(100);
  tone(PIN_BEEP, 1000, 50);
}

void resetRFID() {
  rfid.PCD_Reset();
  rfid.PCD_Init();
  Serial.println("RFID reinitialized");
}

String getCardID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 500) { 
      lastCheck = millis();
      if (!rfid.PCD_PerformSelfTest()) {
        resetRFID();
      }
    }
    return "";
  }

  String content = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    content.concat(String(rfid.uid.uidByte[i] < 0x10 ? "0" : ""));
    content.concat(String(rfid.uid.uidByte[i], HEX));
  }
  content.toUpperCase();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return content;
}


void drawCenteredText(String text, int y = 0, int size = 1) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(SH110X_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, ((SCREEN_HEIGHT - h) / 2) + y);
  display.print(text);
  display.display();
}

String bombCardID = "";
String scannedCardID = "";

void loadingScreen() {
  gyr(0, 0, 0);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 28);
  display.print("Loading");
  display.display();
}


void BombRouletteGame() {
  loadingScreen();
  delay(1000);

  display.setTextSize(1);
  drawCenteredText("Scan Bomb Card");
  gyr(0, 1, 0); 
  bombCardID = "";
  while (bombCardID == "") {
    bombCardID = getCardID();

    if (digitalRead(PIN_BTN_GO) == HIGH) {
      shortBeep();
      backToMenu();
      return;
    }
  }
  shortBeep();

  int isSkip = 1;
  drawCenteredText("Shuffle your card");
  gyr(0, 0, 0);
  delay(2000);
  if (digitalRead(PIN_BTN_GO) == HIGH) isSkip = 0;
  drawCenteredText("Shuffle your card.");
  gyr(1, 0, 0);
  delay(2000*isSkip);
  if (digitalRead(PIN_BTN_GO) == HIGH) isSkip = 0;
  drawCenteredText("Shuffle your card..");
  gyr(1, 1, 0);
  delay(2000*isSkip);
  if (digitalRead(PIN_BTN_GO) == HIGH) isSkip = 0;
  drawCenteredText("Shuffle your card...");
  gyr(1, 1, 1);
  delay(2000*isSkip);
  shortBeep();


  // Step 2: Player scan
  while (true) {
    gyr(0, 0, 0);
    drawCenteredText("Scan Your Card");
    scannedCardID = "";
    while (scannedCardID == "") {
      scannedCardID = getCardID();
    }
    // Step 3: Check result
    if (scannedCardID == bombCardID) {
      gyr(0, 0, 1);
      int bombX = (SCREEN_WIDTH - 32) / 2;
      display.clearDisplay();
      display.drawBitmap(bombX, 20, bombBitmap, 32, 32, SH110X_WHITE);
      display.display();
      longBeep();
      gyr(0, 0, 0);
      drawCenteredText("YOU LOSE X", 0, 2);
      delay(2000);
      break;
    } else {
      gyr(1, 0, 0);
      shortBeep();
      drawCenteredText("SAFE /", 0, 2);
      delay(1500);
    }
  }

  backToMenu();
}



void BombIsExploding() {
  int correctWire = random(0, 3);
  const unsigned long TIME_LIMIT = 3000;
  unsigned long start = millis();
  bool success = false;

  // Start screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 10);
  display.println(F("💣 CUT THE WIRE!"));
  display.setCursor(10, 26);
  display.println(F("[LEFT] [GO] [RIGHT]"));
  display.display();

  // Countdown loop
  while (millis() - start < TIME_LIMIT) {
    unsigned long elapsed = millis() - start;
    int remaining = (TIME_LIMIT - elapsed + 999) / 1000; // ceiling rounding

    display.fillRect(50, 40, 30, 20, SH110X_BLACK); // clear area
    display.setTextSize(2);
    display.setCursor(58, 40);
    display.print(remaining);
    display.display();

    // Check input
    if (digitalRead(PIN_BTN_LEFT) == HIGH && correctWire == 0) { success = true; break; } 
    if (digitalRead(PIN_BTN_GO)   == HIGH && correctWire == 1) { success = true; break; }
    if (digitalRead(PIN_BTN_RIGHT)== HIGH && correctWire == 2) { success = true; break; }

    delay(50);
  }

  // Result
  display.clearDisplay();
  display.setTextSize(2);
  if (success) {
    gyr(0,1,0); shortBeep();
    display.setCursor(25, 24);
    display.print(F("SAFE!"));
  } else {
    gyr(1,0,0); longBeep();
    display.setCursor(25, 24);
    display.print(F("BOOM!"));
  }
  display.display();
  delay(2000);
  backToMenu();
}



void hiddenDefuse_setTimeDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  drawCenteredText("< set time (Ready?) >", -10);
  display.setTextSize(2);
  display.setCursor(50, 30);
  display.print(timeLimit_hiddenDefuse/1000);
  display.display();
}

void hiddenDefuse() {
  loadingScreen();
  delay(1000);

  String defuseCard = "";
  bool defused = false;
  bool isNotDefuseCard = false;


  // Step 1: Set bomb card
  display.setTextSize(1);
  drawCenteredText("Scan Defuse Card");
  gyr(0, 1, 0); 
  defuseCard = "";
  while (defuseCard == "") {
    defuseCard = getCardID();

    if (digitalRead(PIN_BTN_GO) == HIGH) {
      shortBeep();
      backToMenu();
      return;
    }
  }
  shortBeep();
  gyr(0, 0, 0);

  hiddenDefuse_setTimeDisplay();

  while (true) {
    if (digitalRead(PIN_BTN_LEFT) == HIGH && timeLimit_hiddenDefuse > 0) {
      timeLimit_hiddenDefuse -= 1000;
      hiddenDefuse_setTimeDisplay();
      delay(300);
    } else if (digitalRead(PIN_BTN_RIGHT) == HIGH && timeLimit_hiddenDefuse < 30000) {
      timeLimit_hiddenDefuse += 1000;
      hiddenDefuse_setTimeDisplay();
      delay(300);
    } else if (digitalRead(PIN_BTN_GO) == HIGH) {
      break;
    }
  }
  shortBeep();

  drawCenteredText("Ready...");
  delay(random(1000, 4000));
  shortBeep();


  unsigned long start = millis();
  int remaining = 0;
  while (millis() - start < timeLimit_hiddenDefuse + 1000) {
    unsigned long elapsed = millis() - start;
    remaining = (timeLimit_hiddenDefuse - elapsed + 999) / 1000;

    // Clear screen before reprinting to avoid overlap
    display.clearDisplay();
    display.setTextSize(1);
    if (isNotDefuseCard) {
      display.setCursor(27, 55);
      display.print("(not defuse!)");
    } 
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(25, 0);
    display.println(F("DEFUSE!"));
    int imgX = (SCREEN_WIDTH - 32) / 2;
    int imgY = ((SCREEN_HEIGHT - 32) / 2) + 0;
    display.drawBitmap(imgX, imgY, epd_bitmap_boom, 32, 32, SH110X_WHITE);
    display.setTextSize(2);
    display.setCursor(57, 50);
    display.print(remaining);
    display.display();

    // Check for card
    String id = getCardID();
    if (id != "") {
      if (id == defuseCard) {
        defused = true;
        break;
      } else {
        gyr(0, 1, 0);
        isNotDefuseCard = true;
        doubleBeep();
      }
    } 
    delay(100);
  }

  display.clearDisplay();
  display.setTextSize(2);
  if (defused) {
    gyr(1,0,0); shortBeep();
    display.setCursor(5, 24);
    display.print(F("SAFE! "));
    display.print(remaining);
    display.print(F("s"));
    if (timeLimit_hiddenDefuse > 3000) timeLimit_hiddenDefuse -= 1000;
  } else {
    display.setCursor(15, 24);
    display.print(F("BOOM!"));
    display.display();
    gyr(0,0,1); longBeep();
  }
  display.display();
  delay(2000);
  backToMenu();
}


int totalcount_bomb = 10;
int count_bomb;
int maxDec_bomb = 2;
int count_putdown;

void holdABomb_setCountDisplay(String name, int num) {
  display.clearDisplay();
  display.setTextSize(1);
  drawCenteredText("< " + name + " >", -10);
  display.setTextSize(2);
  display.setCursor(50, 30);
  display.print(num);
  display.display();
}

String bomb_holder = "";

bool holdABomb_idle() {
  int decTimeLim = ((count_putdown < 8) ? count_putdown * 500 : 4000);
  int timeLimit = 9000 - decTimeLim;
  unsigned long start = millis();
  int remaining = 0;
  while (millis() - start < timeLimit + 500) {
    unsigned long elapsed = millis() - start;
    remaining = (timeLimit - elapsed + 999) / 1000;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(25, 0);
    if (count_bomb == 0) display.println(F("UNLUCK.."));
    else display.println(F("HOLD IT!"));
    int imgX = (SCREEN_WIDTH - 32) / 2;
    int imgY = ((SCREEN_HEIGHT - 32) / 2) + 0;
    display.drawBitmap(imgX, imgY, epd_bitmap_boom, 32, 32, SH110X_WHITE);
    // Countdown number big and centered
    display.setTextSize(2);
    display.setCursor(30, 50);
    display.print(remaining);
    display.print("s : ");
    display.print(count_bomb);
    display.display();

    // Check for card
    bomb_holder = getCardID();
    if (bomb_holder != "" && millis() - start > 300) {
      if (count_bomb <= 0) return false;
      return true;
    } 
    delay(100);
  }
  return false;
}

bool holdABomb_holding(float speed) {
  int countStart = maxDec_bomb;
  float timePerNum = 1000.0 / speed;
  unsigned long lastChange = millis();
  int current = countStart;

  while (current >= 0) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(20, 0);
    if (current == 0) {
      display.println(F("PUT DOWN!"));
    } else {
      display.println(F("HOLDING.."));
    }

    int imgX = (SCREEN_WIDTH - 32) / 2;
    int imgY = ((SCREEN_HEIGHT - 32) / 2) + 0;
    display.drawBitmap(imgX, imgY, epd_bitmap_boom_hide, 32, 32, SH110X_WHITE);

    display.setCursor(30, 50);
    display.print(current);
    display.print(F("n > "));
    display.print(count_bomb);
    display.display();

    // Add extra hold time when showing 0
    unsigned long extra = (current == 0) ? (timePerNum * 0.4) : 0;  // 40% longer for 0

    if (millis() - lastChange >= timePerNum + extra) {
      current--;
      count_bomb--;
      if (count_bomb < 0) return false;
      lastChange = millis();
    }

    // Check for card to stop early
      String id = getCardID();
    if (id != "") {
      if (id == bomb_holder && current < countStart) {
        gyr(1, 0, 0);
        bomb_holder = "";
        count_putdown++;
        return true;
      } else {
        gyr(0, 1, 0);
        doubleBeep();
      }
    }
  }
  return false;
}




void holdABomb() {
  loadingScreen();
  delay(1000);
  bomb_holder = "";
  count_putdown = 0;
  
  holdABomb_setCountDisplay("set Count (Next)", totalcount_bomb);
  while (true) {
    if (digitalRead(PIN_BTN_LEFT) == HIGH && totalcount_bomb > 1) {
      totalcount_bomb--;
      holdABomb_setCountDisplay("set Count (Next)", totalcount_bomb);
      delay(200);
    } else if (digitalRead(PIN_BTN_RIGHT) == HIGH && totalcount_bomb < 30000) {
      totalcount_bomb++;
      holdABomb_setCountDisplay("set Count (Next)", totalcount_bomb);
      delay(200);
    } else if (digitalRead(PIN_BTN_GO) == HIGH) {
      break;
    }
  }
  count_bomb = totalcount_bomb;
  shortBeep();
  loadingScreen();
  delay(500);

  holdABomb_setCountDisplay("max _ times (GO?)", maxDec_bomb);
  while (true) {
    if (digitalRead(PIN_BTN_LEFT) == HIGH && maxDec_bomb > 1) {
      maxDec_bomb--;
      holdABomb_setCountDisplay("max _ times (GO?)", maxDec_bomb);
      delay(300);
    } else if (digitalRead(PIN_BTN_RIGHT) == HIGH && maxDec_bomb < 5) {
      maxDec_bomb++;
      holdABomb_setCountDisplay("max _ times (GO?)", maxDec_bomb);
      delay(300);
    } else if (digitalRead(PIN_BTN_GO) == HIGH) {
      break;
    }
  }
  shortBeep();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(25, 0);
  display.println(F("HOLD=GO"));
  int imgX = (SCREEN_WIDTH - 32) / 2;
  int imgY = ((SCREEN_HEIGHT - 32) / 2) + 0;
  display.drawBitmap(imgX, imgY, epd_bitmap_boom, 32, 32, SH110X_WHITE);
  // Countdown number big and centered
  display.setTextSize(2);
  display.setCursor(30, 50);
  display.print("0s : ");
  display.print(count_bomb);
  display.display();
  while (true) {
    bomb_holder = getCardID();
    if (bomb_holder != "") break;
  }
  shortBeep();

  bool res;
  while (true) {
    res = holdABomb_holding(0.3 + (0.2 * maxDec_bomb));
    shortBeep();
    if (!res) break;
    res = holdABomb_idle();
    shortBeep();
    if (!res) break;
  }

  display.clearDisplay();
  display.setCursor(15, 24);
  display.print(F("BOOM!"));
  display.display();
  gyr(0,0,1); longBeep();
  drawCenteredText("Game Over");
  display.display();
  delay(2000);
  backToMenu();
}

void backToMenu() {
  state = STATE_MENU;
  drawCenteredText("Back to Menu...");
  delay(500);
  showMenuScreen();
}

void cardIDFeature() {
  loadingScreen();
  delay(500);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 10);
  display.print("Scan your card");
  display.display();

  while (true) {
    String newID = getCardID();
    if (newID != "") {
      shortBeep();
      
      display.clearDisplay();
      display.setCursor(0, 10);
      display.print("Card ID:");
      display.setCursor(0, 25);
      display.print(newID);
      display.display();
    }

    // Exit condition: press GO button to go back
    if (digitalRead(PIN_BTN_GO) == HIGH) {
      shortBeep();
      backToMenu();
      break;
    }
  }
}

void gyr(int r, int g, int b) {
  gyrA(r*255, g*255, b*255);
}

void gyrA(int r, int g, int b) {
  analogWrite(PIN_B, r);
  analogWrite(PIN_G, g);
  analogWrite(PIN_R, b);
}

// ------------------------------------------------------------------
// Input handling
// ------------------------------------------------------------------
void pollGoButton() {
  bool raw = digitalRead(PIN_BTN_GO) == HIGH;
  if (raw != lastGoRead) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (raw && !goPressed) {
      goPressed = true;
    }
  }

  lastGoRead = raw;
}

// ---- PAGE STRUCTURE ----
struct TutorialPage {
  const unsigned char* img;
  const char* text;
};

TutorialPage tutorialPages[] = {
  { bomb32,   "Bomb card is dangerous, it will make you lose" },
  { shield32, "Shield card is useful when next card Bomb" },
};

const int TOTAL_PAGES = sizeof(tutorialPages) / sizeof(tutorialPages[0]);
int currentPage = 0;


void tutorialMenu() {
  loadingScreen();
  delay(500);
  bool running = true;
  while (running) {
    display.clearDisplay();
    int imgX = (SCREEN_WIDTH - 32) / 2;
    int imgY = 4;
    display.drawBitmap(imgX, imgY, tutorialPages[currentPage].img, 32, 32, SH110X_WHITE);

    // Draw page counter (top right)
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(SCREEN_WIDTH - 40, 0);
    display.print("<");
    display.print(currentPage + 1);
    display.print("/");
    display.print(TOTAL_PAGES);
    display.print(">");

    // Draw description (bottom)
    display.setCursor(0, 40);
    display.print(tutorialPages[currentPage].text);

    display.display();

    // --- Input handling ---
    if (digitalRead(PIN_BTN_RIGHT) == HIGH) {
      currentPage++;
      if (currentPage >= TOTAL_PAGES) currentPage = 0;
      delay(300);
    }
    if (digitalRead(PIN_BTN_LEFT) == HIGH) {
      currentPage--;
      if (currentPage < 0) currentPage = TOTAL_PAGES - 1;
      delay(300);
    }
    if (digitalRead(PIN_BTN_GO) == HIGH) {
      shortBeep();
      running = false;
      backToMenu();
      delay(200);
    }
  }
}





// ------------------------------
// BOMB DUNGEON is here
// ------------------------------
unsigned int level = 0;
bool gameOver = false;

void dungeon_displayCenter(const char* text, int y = 24, int size = 2) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(SH110X_WHITE);
  int16_t x = (128 - strlen(text) * 6 * size) / 2;
  display.setCursor(x, y);
  display.print(text);
  display.display();
}


// ------------------------------
// Level 0 : Chest of Safety
// ------------------------------
int player_dungeon;
std::map<String, ItemName> cardItems;

void dungeon_level0_display() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 0);
  display.println("Lobby:");
  int imgX = (SCREEN_WIDTH - 32) / 2;
  int imgY = (SCREEN_HEIGHT - 32) / 2;
  display.drawBitmap(imgX, imgY, epd_bitmap_chest, 32, 32, SH110X_WHITE);
  display.setCursor(10, 55);
  display.print(player_dungeon);
  display.print(" Player    ");
  if (player_dungeon <= 0) display.print("[Back?]");
  else display.print("[GO]"); 
  display.display();
}




void getItem_animation(const uint8_t bitmap[]) {
  for (int i=5; i>=0; i--) {
    display.clearDisplay();
    int imgX = (SCREEN_WIDTH - 32) / 2;
    int imgY = ((SCREEN_HEIGHT - 32) / 2 ) - (i*i);
    display.drawBitmap(imgX, imgY, bitmap, 32, 32, SH110X_WHITE);
    display.display();
    delay(50);
  }
}

ItemName prev_item;

void getItemScreen(String card, ItemName item) {
  String nameItem = "???";
  cardItems[card] = item;
  prev_item = item;
  switch (item) {
    case COIN:
      getItem_animation(epd_bitmap_coin);
      nameItem = "Coin";
      break;
    case DEFUSE:
      getItem_animation(epd_bitmap_defuse);
      nameItem = "Defuse";
      break;
    case PREDICT:
      getItem_animation(epd_bitmap_glass);
      nameItem = "Predict";
      break;
    case SHIELD:
      getItem_animation(epd_bitmap_shield);
      nameItem = "Shield";
      break;
    case COPY:
      getItem_animation(epd_bitmap_swap);
      nameItem = "Copy";
      break;

  }
  display.setCursor(10, 55);
  display.print(F("got "));
  display.print(nameItem);
  display.print(F("!   [OK]"));
  display.display();
}


void dungeon_level0_scan() {
  dungeon_level0_display();
  
  String card = "";
  while (card == "") {
    card = getCardID();
    
    if (digitalRead(PIN_BTN_GO) == HIGH) {
      if (player_dungeon <= 0) {
        shortBeep();
        backToMenu();
        gameOver = true;
        return;
      } else {
        shortBeep();
        return;
      }
    }
  }
  
  if (!cardItems.count(card)) {
    shortBeep();
    getItemScreen(card, DEFUSE);
    player_dungeon++;
  } else {
    doubleBeep();
    drawCenteredText("already scan");
  }
  while (digitalRead(PIN_BTN_GO) == LOW) {
    delay(100);
  }

  dungeon_level0_scan();
}

// ------------------------------
// Chest logic (generic)
// ------------------------------
bool defuseBomb_dungeon() {
  bool isNotDefuseCard = false;
  bool defused = false;
  unsigned long start = millis();
  int remaining = 0;
  while (millis() - start < timeLimit_hiddenDefuse + 1000) {
    unsigned long elapsed = millis() - start;
    remaining = (timeLimit_hiddenDefuse - elapsed + 999) / 1000;

    display.clearDisplay();
    display.setTextSize(1);
    if (isNotDefuseCard) {
      display.setCursor(27, 55);
      display.print("(not defuse!)");
    } 
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(25, 0);
    display.println(F("DEFUSE!"));
    int imgX = (SCREEN_WIDTH - 32) / 2;
    int imgY = ((SCREEN_HEIGHT - 32) / 2) + 0;
    display.drawBitmap(imgX, imgY, epd_bitmap_boom, 32, 32, SH110X_WHITE);
    display.setTextSize(2);
    display.setCursor(57, 50);
    display.print(remaining);
    display.display();

    // Check for card
    String card = getCardID();
    if (card != "") {
      if (isEqualItem(card, DEFUSE)
        || isEqualItem(card, SHIELD)) {
        defused = true;
        cardItems.erase(card);
        break;
      } else {
        gyr(0, 1, 0);
        isNotDefuseCard = true;
        doubleBeep();
      }
    } 
    delay(100);
  }

  display.clearDisplay();
  display.setTextSize(2);
  if (defused) {
    gyr(1,0,0); shortBeep();
    display.setCursor(5, 24);
    display.print(F("SAFE! "));
    display.print(remaining);
    display.print(F("s"));
    display.display();
    if (timeLimit_hiddenDefuse > 3000) timeLimit_hiddenDefuse -= 1000;
    delay(2000);
    return true;
  } else {
    display.setCursor(15, 24);
    display.print(F("BOOM!"));
    display.display();
    gyr(0,0,1); longBeep();
    delay(2000);
    return false;
  }
}


bool dungeon_openChest(int bombCount, int scans) {
  std::set<int> bombPositions;
  while (bombPositions.size() < bombCount) {
    bombPositions.insert(random(0, scans));
  }

  for (int i = scans - 1; i >= 0; i--) {
    int remaining = i + 1;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 0);
    display.print("Level ");
    display.print(level);
    display.println(":");
    int imgX = (SCREEN_WIDTH - 32) / 2;
    int imgY = (SCREEN_HEIGHT - 32) / 2;
    display.drawBitmap(imgX, imgY, epd_bitmap_chest, 32, 32, SH110X_WHITE);
    display.setCursor(10, 55);
    display.print(remaining);
    display.print("/");
    display.print(scans);
    display.print("   "); 
    display.print(bombCount);
    display.print(" bomb");
    if (bombCount != 1) display.print("s");
    display.display();

    // --- Wait for card scan ---
    String card = "";
    bool isSkip = false;
    delay(300);
    while (card == "") {
      card = getCardID();
      if (isEqualItem(card, PREDICT)) {
        cardItems.erase(card);
        card = "";
        display.clearDisplay();
        int imgX = (SCREEN_WIDTH - 32) / 2;
        int imgY = (SCREEN_HEIGHT - 32) / 2;
        display.drawBitmap(imgX, imgY, epd_bitmap_chest, 32, 32, SH110X_WHITE);
        display.setCursor(10, 55);
        if (bombPositions.count(i)) display.print("Bomb <!>");
        else display.print("Not Bomb /");
        display.display();
        doubleBeep();
        delay(300);
      }
      if (digitalRead(PIN_BTN_GO) == HIGH) {
        isSkip = true;
        if (bombPositions.count(i)) bombCount--;
        break;
      }
      if (isEqualItem(card, COPY)) {
        drawCenteredText("Copy old card.");
        delay(1000);
        getItemScreen(card, prev_item);
        card = "";
        delay(1000);
        display.clearDisplay();
        int imgX = (SCREEN_WIDTH - 32) / 2;
        int imgY = (SCREEN_HEIGHT - 32) / 2;
        display.drawBitmap(imgX, imgY, epd_bitmap_chest, 32, 32, SH110X_WHITE);
        display.setCursor(10, 55);
        display.print(remaining);
        display.print("/");
        display.print(scans);
        display.print("   "); 
        display.print(bombCount);
        display.print(" bomb");
        display.display();
      }
    }
    if (isSkip) {
      drawCenteredText("Skip...");
      doubleBeep();
      delay(1000);
      continue;
    }

    // --- Check result ---
    if (bombPositions.count(i)) {
      gyr(0, 0, 1);
      doubleBeep();
      bool isPass = defuseBomb_dungeon();
      bombCount--;
      if (!isPass) return false;
    } else {
      gyr(1, 0, 0);
      shortBeep();
      getItemScreen(card, getRandomItem());
      while (digitalRead(PIN_BTN_GO) == LOW) {
        delay(100);
      }
    }
  }

  // --- If all safe ---
  display.clearDisplay();
  display.setTextSize(2);
  drawCenteredText("All Safe!", 20);
  display.display();
  delay(1000);
  return true;
}


// ------------------------------
// Door logic (countdown challenge)
// ------------------------------
bool dungeon_openDoor(int totalCoin) {
  int coin = totalCoin;
  unsigned long time_limit = 20000;
  unsigned long start = millis();

  while (millis() - start < time_limit) {
    unsigned long remain = (time_limit - (millis() - start)) / 1000 + 1;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 10);
    display.print(coin);
    display.print(" coin");
    if (coin > 1) display.print("s");
    display.print(" to Unlock");
    display.setTextSize(3);
    display.setCursor(54, 28);
    display.print(remain);
    display.display();

    String card = getCardID();
    if (card != "") {
      if (isEqualItem(card, COIN)) {
        if (coin <= 1) {
          gyr(1, 0, 0);
          dungeon_displayCenter("Unlocked!", 20);
          delay(1200);
          return true;
        } else {
          gyr(0, 0, 0);
          coin--;
        }
        shortBeep();
        cardItems.erase(card);
      } else {
        gyr(0, 1, 0);
        doubleBeep();
        dungeon_displayCenter("not a coin", 24);
      }
    }
    delay(50);
  }
  dungeon_displayCenter("TIME UP!", 24);
  gyr(1, 0, 0);
  longBeep();
  delay(1500);
  return false;
}

// ------------------------------
// Dungeon flow
// ------------------------------
void BombDungeon() {
  loadingScreen();
  delay(1000);
  gameOver = false;
  level = 0;
  player_dungeon = 0;
  cardItems.clear();

  dungeon_level0_scan();
  if (gameOver) return;

  while (!gameOver) {
    level++;
    if (level > 10) break;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 10);
    display.print("Level ");
    display.print(level);
    display.setCursor(10, 25);
    display.println("Opening Chest...");
    display.display();
    delay(1000);

    int bombs = 1 + (level / 2.5);     // scale difficulty
    int scans = player_dungeon + level;

    bool safe = dungeon_openChest(bombs, scans);
    if (!safe) {
      dungeon_displayCenter("GAME OVER", 20);
      gameOver = true;
      break;
    }

    // Door requirement every few levels
    if (level % 3 == 0) {
      bool door = dungeon_openDoor(level / 3);
      if (!door) {
        gameOver = true;
        break;
      }
    }
  }

  if (!gameOver) {
    dungeon_displayCenter("🏆 YOU WIN! 🏆", 20);
    gyr(0, 1, 0);
    for (int i = 0; i < 3; i++) { shortBeep(); delay(300); }
  }

  delay(3000);
  backToMenu();
}


bool isEqualItem(String keyToCheck, ItemName expectedState) {
  auto it = cardItems.find(keyToCheck); 

  if (it != cardItems.end()) {
    Serial.print("Key '");
    Serial.print(keyToCheck);
    Serial.println("' exists.");
    
    ItemName currentValue = it->second;
    return currentValue == expectedState;
  }
  return false;
}

ItemName getRandomItem() {
  int randomIndex = random(5);
  return static_cast<ItemName>(randomIndex); 
}