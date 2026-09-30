// =====================================================
//  Light OS - Hardware self-test
//
//  Use this BEFORE the main firmware to check that every
//  part is wired correctly. Open Serial Monitor at 115200.
//
//  What it does:
//   1. Flashes the display red / green / blue / white
//   2. Blinks the LED 3 times and beeps the buzzer twice
//   3. Then shows LIVE readings on the display + Serial:
//        - LDR value (0..4095)   -> cover / uncover the sensor
//        - DHT11 temperature and humidity
//        - Knob rotation counter and button state
// =====================================================

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <DHT.h>

#define LDR_PIN     34
#define LED_PIN     16
#define BUZZER_PIN  17

#define TFT_CS      5
#define TFT_DC      2
#define TFT_RST     15
#define TFT_SDA     23
#define TFT_SCL     18

#define ENC_CLK     32
#define ENC_DT      33
#define ENC_SW      25
#define DHT_PIN     27

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHT_PIN, DHT11);

static const int8_t encTable[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
uint8_t encState = 0;
int8_t encAcc = 0;
int encPos = 0;
bool encChanged = false;

float temp = NAN, hum = NAN;
unsigned long lastDht = 0, lastDraw = 0;
bool lastBtn = false;

void pollEncoder() {
  encState = ((encState << 2) | (digitalRead(ENC_CLK) << 1) | digitalRead(ENC_DT)) & 0x0F;
  encAcc += encTable[encState];
  if (encAcc >= 4) {
    encPos++;
    encAcc = 0;
    encChanged = true;
  } else if (encAcc <= -4) {
    encPos--;
    encAcc = 0;
    encChanged = true;
  }
}

void beep(int ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
  delay(ms);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== Light OS hardware self-test ===");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  SPI.begin(TFT_SCL, -1, TFT_SDA, TFT_CS);
  tft.begin();
  tft.setRotation(0);
  dht.begin();

  Serial.println("1) Display colour test (red, green, blue, white)");
  const uint16_t colours[4] = { 0xF800, 0x07E0, 0x001F, 0xFFFF };
  for (int i = 0; i < 4; i++) {
    tft.fillScreen(colours[i]);
    delay(500);
  }
  tft.fillScreen(0x0000);

  Serial.println("2) LED blink x3 and buzzer beep x2");
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
  }
  beep(120);
  beep(120);

  Serial.println("3) Live readings - cover the LDR, turn and press the knob");
  tft.setTextSize(2);
  tft.setTextColor(0xFFFF);
  tft.setCursor(52, 30);
  tft.print("SELF TEST");
}

void loop() {
  pollEncoder();

  unsigned long now = millis();

  if (now - lastDht >= 2500) {
    lastDht = now;
    hum = dht.readHumidity();
    temp = dht.readTemperature();
  }

  bool btn = (digitalRead(ENC_SW) == LOW);
  bool btnChanged = (btn != lastBtn);
  lastBtn = btn;

  if (encChanged || btnChanged || now - lastDraw >= 500) {
    lastDraw = now;

    long sum = 0;
    for (int i = 0; i < 16; i++) {
      sum += analogRead(LDR_PIN);
      delayMicroseconds(200);
    }
    int ldr = sum / 16;

    tft.setTextSize(2);
    tft.setTextColor(0x07FF, 0x0000);   // cyan on black (overwrites old text)
    tft.setCursor(40, 75);
    tft.print("LDR: ");
    tft.print(ldr);
    tft.print("    ");

    tft.setCursor(40, 105);
    if (isnan(temp) || isnan(hum)) {
      tft.setTextColor(0xF800, 0x0000);
      tft.print("DHT: NO DATA  ");
    } else {
      tft.setTextColor(0xFFE0, 0x0000);
      tft.print(temp, 0);
      tft.print("C  ");
      tft.print(hum, 0);
      tft.print("%   ");
    }

    tft.setTextColor(0x07E0, 0x0000);
    tft.setCursor(40, 135);
    tft.print("KNOB: ");
    tft.print(encPos);
    tft.print("    ");

    tft.setCursor(40, 165);
    tft.setTextColor(btn ? 0xF800 : 0x8410, 0x0000);
    tft.print(btn ? "BUTTON: DOWN" : "BUTTON: up   ");

    Serial.print("LDR=");
    Serial.print(ldr);
    Serial.print("  DHT=");
    if (isnan(temp) || isnan(hum)) Serial.print("no data");
    else {
      Serial.print(temp, 0);
      Serial.print("C/");
      Serial.print(hum, 0);
      Serial.print("%");
    }
    Serial.print("  KNOB=");
    Serial.print(encPos);
    Serial.print("  BUTTON=");
    Serial.println(btn ? "DOWN" : "up");
    encChanged = false;
  }
}
