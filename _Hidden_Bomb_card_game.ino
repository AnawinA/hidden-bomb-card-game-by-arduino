// Hidden Bomb - Start/Menu prototype v2 (with bomb image & beep)
#include "Variables.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <MFRC522.h>
#include <vector>


// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----------------- RFID -----------------
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
const char* menuItems[] = {"Bomb or Not", "Hidden Bomb", "Show Card ID", "Skills"};
const int menuCount = 4;
int currentSelection = 0;

// Debounce helpers
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;
bool lastGoRead = LOW;
bool goPressed = false;

// Slower fake loading
const unsigned long LOADING_DURATION_MS = 1500;

//Game: Bomb or Not
enum GameState {SET_BOMB, SCAN_CARD, RESULT};
AppState state = STATE_LOADING;

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

  rgbLed(0, 0, 0);
}

void setup() {
  Serial.begin(115200);
  setupPins();

  SPI.begin();
  rfid.PCD_Init();

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
        if (digitalRead(PIN_BTN_LEFT) == HIGH) {
          if (currentSelection > 0) currentSelection--;
          showMenuScreen();
          delay(200); // debounce
        }
        
        if (digitalRead(PIN_BTN_RIGHT) == HIGH) {
          if (currentSelection < menuCount - 1) currentSelection++;
          showMenuScreen();
          delay(200);
        }

        pollGoButton();
        if (goPressed) {
          goPressed = false;
          tone(PIN_BEEP, 1000, 100); // short beep
          Serial.println(currentSelection);
          switch (currentSelection) {
            case 0:
              state = STATE_BOMB_OR_NOT;
              break;
            case 2:
              state = STATE_CARD_ID;
              break;
            case 3:
              tutorialMenu();
              break;
          }
        }

        switch (currentSelection) {
          case 0:
            rgbLed(50, 0, 0);
            break;
          case 1:
            rgbLed(0, 20, 0);
            break;
          case 2:
            rgbLed(0, 0, 50);
            break;
          case 3:
            rgbLed(50, 20, 50);
            break;
        }

      break;
    
    case STATE_BOMB_OR_NOT:
      bombOrNotGame();
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

  delay(300); // little pause before start screen
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

  // Draw cards at top right corner
  display.drawBitmap(88, 0, epd_bitmap_game_icon, 32, 32, SH110X_WHITE);
  

  // Press to start text
  display.setTextSize(1);
  display.setCursor(10, 48);
  display.println(F("Press [GO] to start"));

  display.display();
}

void showMenuScreen() {
  display.clearDisplay();
  // Draw title
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(F("MENU"));

  display.setTextSize(1);
  
  int y = 40; // vertical center-ish
  
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

  switch (currentSelection) {
    case 0:
      display.drawBitmap(92, y, icon_bomb, 8, 8, SH110X_WHITE);
      break;
    case 1:
      display.drawBitmap(92, y, icon_players, 8, 8, SH110X_WHITE);
      break;
    case 2:
      display.drawBitmap(92, y, icon_question, 8, 8, SH110X_WHITE);
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

String getCardID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return "";
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

void drawCenteredText(const char* text, int size = 1) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(SH110X_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(text);
  display.display();
}

String bombCardID = "";
String scannedCardID = "";

void loadingScreen() {
  rgbLed(0, 0, 0);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 28);
  display.print("Loading");
  display.display();
}

std::vector<String> shieldCards;

void bombOrNotGame() {
  // Fake loading
  loadingScreen();
  delay(1000);

  // Step 1: Set bomb card
  display.setTextSize(1);
  drawCenteredText("Scan Bomb Card");
  rgbLed(50, 20, 0); 
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

  // while (digitalRead(PIN_BTN_GO) == LOW) {
    
  //   shieldCards.push_back();

  //   if (digitalRead(PIN_BTN_GO) == HIGH) {
  //     break;
  //     shortBeep();
  //   }
  // }


  drawCenteredText("Shuffle your card");
  rgbLed(50, 0, 50);
  delay(2000);
  drawCenteredText("Shuffle your card.");
  rgbLed(0, 0, 0);
  delay(2000);
  drawCenteredText("Shuffle your card..");
  rgbLed(50, 0, 50);
  delay(2000);
  drawCenteredText("Shuffle your card...");
  rgbLed(0, 0, 0);
  delay(2000);
  shortBeep();


  // Step 2: Player scan
  while (true) {
    rgbLed(50, 20, 0);
    drawCenteredText("Scan Your Card");
    scannedCardID = "";
    while (scannedCardID == "") {
      scannedCardID = getCardID();
    }
    // Step 3: Check result
    if (scannedCardID == bombCardID) {
      rgbLed(50, 0, 0);
      int bombX = (SCREEN_WIDTH - 32) / 2;
      display.clearDisplay();
      display.drawBitmap(bombX, 20, bombBitmap, 32, 32, SH110X_WHITE);
      display.display();
      longBeep();
      rgbLed(0, 0, 0);
      drawCenteredText("YOU LOSE X", 2);
      delay(2000);
      break;
    } else {
      rgbLed(0, 20, 0);
      shortBeep();
      drawCenteredText("SAFE /", 2);
      delay(1500);
    }
  }


  // Step 4: Return to menu
  // (You can replace this with a proper state change)
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

void rgbLed(int r, int g, int b) {
  analogWrite(PIN_R, 255-r);
  analogWrite(PIN_G, 255-g);
  analogWrite(PIN_B, 255-b);
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
    // Draw
    display.clearDisplay();

    // Draw image (top center)
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
    if (digitalRead(PIN_BTN_RIGHT) == HIGH) {  // RIGHT button
      currentPage++;
      if (currentPage >= TOTAL_PAGES) currentPage = 0;
      delay(300);
    }
    if (digitalRead(PIN_BTN_LEFT) == HIGH) {  // LEFT button
      currentPage--;
      if (currentPage < 0) currentPage = TOTAL_PAGES - 1;
      delay(300);
    }
    if (digitalRead(PIN_BTN_GO) == HIGH) {  // GO button to exit
      shortBeep();
      running = false;
      backToMenu();
      delay(200);
    }
  }
}



