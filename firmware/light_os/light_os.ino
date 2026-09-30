// =====================================================
//  LIGHT OS  -  ESP32 + GC9A01 round TFT   (v1.0.0)
//  Project home: https://github.com/mehrdad2200/LightOS-ESP32
//  License: MIT
//
//  Pages : 0 Dashboard | 1 Digital clock | 2 Analog clock
//          3 Temp/Humidity | 4 Weather (internet) | 5 Alarm & Timer
//          6 Light stats + IP | 7 Warp screensaver
//
//  Knob  : rotate = change page      short press = home
//          long press = home / cancel edit / stop alarm
//          on the Alarm page: short press = edit fields step by step
//  Phone : open  http://lightos.local  (or the IP shown on the
//          Stats page / Serial Monitor) while on the same Wi-Fi.
//  Settings are saved in flash and survive power-off.
// =====================================================

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>
#include <DHT.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// ================= YOUR SETTINGS =================
// Wi-Fi credentials live in secrets.h, which is NOT committed to git.
// Copy secrets.h.example to secrets.h and fill in your own Wi-Fi name and password.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h - copy secrets.h.example to secrets.h and put your Wi-Fi name and password in it."
#endif

const bool  ENC_REVERSE = false;          // true if the knob turns the wrong way
const unsigned long IDLE_MS  = 60000;     // 1 minute of nothing -> idle mode
const unsigned long CYCLE_MS = 9000;      // idle: seconds per page
const float WAKE_DELTA = 90;              // light change (ADC units) that wakes the screen
const unsigned long WX_INTERVAL = 15UL * 60UL * 1000UL;   // weather refresh

const int  ADC_MIN = 280;                 // reading that means 0 %
const int  ADC_MAX = 1850;                // reading that means 100 %
const unsigned long HOLD_MS  = 1400;      // delay before LED switches (auto mode)
const unsigned long FRAME_MS = 20;

// ================= PINS =================
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
WebServer server(80);
Preferences prefs;

// ================= Pages =================
const int PG_DASH = 0, PG_CLOCK = 1, PG_ANALOG = 2, PG_CLIMATE = 3;
const int PG_WEATHER = 4, PG_ALARM = 5, PG_STATS = 6, PG_WARP = 7;
const int PAGES = 8;
const int IDLE_COUNT = 6;
const int IDLE_ORDER[IDLE_COUNT] = { PG_CLOCK, PG_CLIMATE, PG_WEATHER, PG_ANALOG, PG_STATS, PG_WARP };

// ================= Custom types =================
// (must stay ABOVE the first function, or Arduino's auto-prototypes fail)
struct Frame {
  float pct;
  int   adc;
  uint8_t level;
  bool  night;
  bool  led;
  float t;
  float ripple;
  bool  boot;
  int   page;
  int   ring;      // 0 none, 1 alarm, 2 timer
};

struct Star { float a, d, v; };

// ================= Saved settings =================
struct Settings {
  int darkTh;
  int brightTh;
  uint8_t ledMode;      // 0 auto, 1 on, 2 off
  bool cycle;           // idle auto-cycle
  bool mute;            // mute LED-change beeps
  uint8_t alarmH, alarmM;
  bool alarmOn;
  uint8_t timerMin;
  float lat, lon;
  char city[16];
};
Settings cfg = { 600, 720, 0, true, false, 7, 0, false, 5, 35.6892f, 51.3890f, "Tehran" };
bool cfgDirty = false;
unsigned long cfgDirtyAt = 0;

void markDirty() {
  cfgDirty = true;
  cfgDirtyAt = millis();
}

void loadSettings() {
  prefs.begin("lightos", false);
  cfg.darkTh   = prefs.getInt("dk", cfg.darkTh);
  cfg.brightTh = prefs.getInt("br", cfg.brightTh);
  cfg.ledMode  = prefs.getUChar("lm", cfg.ledMode);
  cfg.cycle    = prefs.getBool("cy", cfg.cycle);
  cfg.mute     = prefs.getBool("mu", cfg.mute);
  cfg.alarmH   = prefs.getUChar("ah", cfg.alarmH);
  cfg.alarmM   = prefs.getUChar("am", cfg.alarmM);
  cfg.alarmOn  = prefs.getBool("ao", cfg.alarmOn);
  cfg.timerMin = prefs.getUChar("tm", cfg.timerMin);
  cfg.lat      = prefs.getFloat("la", cfg.lat);
  cfg.lon      = prefs.getFloat("lo", cfg.lon);
  String c = prefs.getString("ct", String(cfg.city));
  strncpy(cfg.city, c.c_str(), 15);
  cfg.city[15] = 0;
  prefs.end();
}

void saveSettings() {
  prefs.begin("lightos", false);
  prefs.putInt("dk", cfg.darkTh);
  prefs.putInt("br", cfg.brightTh);
  prefs.putUChar("lm", cfg.ledMode);
  prefs.putBool("cy", cfg.cycle);
  prefs.putBool("mu", cfg.mute);
  prefs.putUChar("ah", cfg.alarmH);
  prefs.putUChar("am", cfg.alarmM);
  prefs.putBool("ao", cfg.alarmOn);
  prefs.putUChar("tm", cfg.timerMin);
  prefs.putFloat("la", cfg.lat);
  prefs.putFloat("lo", cfg.lon);
  prefs.putString("ct", String(cfg.city));
  prefs.end();
}

// ================= Rendering =================
const int CX = 120, CY = 120;
const int BAND_H = 80;
const int BANDS  = 3;
GFXcanvas16 *cv = nullptr;
int by = 0;

const int N_SEG = 54;
const int R_IN = 100, R_OUT = 117;
const float START_DEG = 135.0f, SWEEP_DEG = 270.0f;
int16_t quadPts[N_SEG][8];

const int HIST_N = 58;
uint8_t hist[HIST_N];
int histHead = 0;

// ================= State =================
bool ledState = false;
unsigned long darkStart = 0, lightStart = 0;
int adcAvg = 500;
float refAdc = 500;
float currentPercent = 0;
unsigned long lastSensor = 0, lastFrame = 0, lastHist = 0, lastSerial = 0;
unsigned long rippleStart = 0;
bool rippleOn = false;

int page = 0;
bool idle = false;
int idleIdx = 0;
unsigned long lastActivity = 0, lastCycle = 0;
int editField = -1;     // alarm page editing: -1 none, 0 hour, 1 min, 2 on/off, 3 timer min, 4 start/stop

// network / time
bool timeConfigured = false;
bool serverStarted = false;
unsigned long lastWifiTry = 0;
char ipStr[20] = "";
bool timeValid = false;
struct tm gTm;
float gSecF = 0;

// DHT
float gTemp = 0, gHum = 0;
bool dhtOk = false;
unsigned long lastDht = 0, lastDhtOk = 0;

// weather (filled by background task)
volatile bool wxOk = false;
volatile bool wxRefresh = true;
float wxTemp = 0, wxHum = 0, wxWind = 0;
int wxCode = 0;

// stats
float maxPct = 0, minPct = 100;
double sumPct = 0;
uint32_t cntPct = 0;
unsigned long ledOnMs = 0;

// alarm & timer
bool alarmRing = false, timerRing = false;
unsigned long ringStart = 0, lastRingBeep = 0;
int lastAlarmKey = -1;
bool timerRun = false;
unsigned long timerEndMs = 0, timerTotalMs = 0;

// stars
Star stars[36];

// buzzer
uint8_t beepsLeft = 0;
bool buzzerOn = false;
unsigned long beepToggleAt = 0;

// encoder
volatile int encDelta = 0;
volatile uint8_t encState = 0;
volatile int8_t encAcc = 0;
static const int8_t DRAM_ATTR encTable[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
bool lastSw = HIGH;
unsigned long swChange = 0;
bool swDown = false, longFired = false;
unsigned long swDownAt = 0;

// =====================================================
//  Buzzer (non-blocking)
// =====================================================
void startBeep(uint8_t n) {
  beepsLeft = n;
  buzzerOn = true;
  digitalWrite(BUZZER_PIN, HIGH);
  beepToggleAt = millis() + 70;
}

void updateBeep() {
  if (beepsLeft == 0) return;
  if ((long)(millis() - beepToggleAt) < 0) return;
  if (buzzerOn) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
    beepsLeft--;
    beepToggleAt = millis() + 70;
  } else {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerOn = true;
    beepToggleAt = millis() + 70;
  }
}

// =====================================================
//  Colours
// =====================================================
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

const uint16_t C_WHITE = 0xFFFF;
const uint16_t C_SUN   = rgb(255, 208, 64);
const uint16_t C_MOON  = rgb(222, 230, 255);
const uint16_t C_GREEN = rgb(40, 255, 120);
const uint16_t C_GRAY  = rgb(120, 130, 165);
const uint16_t NIGHT_BG = rgb(4, 6, 20);
const uint16_t DAY_BG   = rgb(24, 15, 6);
const uint16_t STOPS[4] = { rgb(110, 70, 255), rgb(0, 200, 255),
                            rgb(255, 225, 70), rgb(255, 110, 30) };

float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

uint16_t mix(uint16_t a, uint16_t b, uint8_t amt) {
  int r1 = (a >> 11) & 31, g1 = (a >> 5) & 63, b1 = a & 31;
  int r2 = (b >> 11) & 31, g2 = (b >> 5) & 63, b2 = b & 31;
  int r = r1 + ((r2 - r1) * amt) / 255;
  int g = g1 + ((g2 - g1) * amt) / 255;
  int bl = b1 + ((b2 - b1) * amt) / 255;
  return (r << 11) | (g << 5) | bl;
}

uint16_t grad(float t) {
  t = clampf(t, 0, 1);
  float s = t * 3.0f;
  int i = (int)s;
  if (i > 2) i = 2;
  return mix(STOPS[i], STOPS[i + 1], (uint8_t)((s - i) * 255));
}

// =====================================================
//  Band-aware drawing wrappers
// =====================================================
inline void fCircle(int x, int y, int r, uint16_t c) { cv->fillCircle(x, y - by, r, c); }
inline void dCircle(int x, int y, int r, uint16_t c) { cv->drawCircle(x, y - by, r, c); }
inline void fLine(int x0, int y0, int x1, int y1, uint16_t c) { cv->drawLine(x0, y0 - by, x1, y1 - by, c); }
inline void fRect(int x, int y, int w, int h, uint16_t c) { cv->fillRect(x, y - by, w, h, c); }
inline void fRRect(int x, int y, int w, int h, int r, uint16_t c) { cv->fillRoundRect(x, y - by, w, h, r, c); }
inline void dRRect(int x, int y, int w, int h, int r, uint16_t c) { cv->drawRoundRect(x, y - by, w, h, r, c); }
inline void hLine(int x, int y, int w, uint16_t c) { cv->drawFastHLine(x, y - by, w, c); }
inline void vLine(int x, int y, int h, uint16_t c) { cv->drawFastVLine(x, y - by, h, c); }
inline void fTri(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
  cv->fillTriangle(x0, y0 - by, x1, y1 - by, x2, y2 - by, c);
}

void fQuad(int i, uint16_t c) {
  int16_t *q = quadPts[i];
  cv->fillTriangle(q[0], q[1] - by, q[2], q[3] - by, q[4], q[5] - by, c);
  cv->fillTriangle(q[0], q[1] - by, q[4], q[5] - by, q[6], q[7] - by, c);
}

void textCentered(const char *s, int cx, int baseY, const GFXfont *font, uint16_t col) {
  int16_t x1, y1;
  uint16_t w, h;
  cv->setFont(font);
  cv->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  cv->setCursor(cx - (int)w / 2 - x1, baseY - by);
  cv->setTextColor(col);
  cv->print(s);
}

void textLeft(const char *s, int x, int baseY, const GFXfont *font, uint16_t col) {
  cv->setFont(font);
  cv->setCursor(x, baseY - by);
  cv->setTextColor(col);
  cv->print(s);
}

void textRight(const char *s, int xr, int baseY, const GFXfont *font, uint16_t col) {
  int16_t x1, y1;
  uint16_t w, h;
  cv->setFont(font);
  cv->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  cv->setCursor(xr - (int)w - x1, baseY - by);
  cv->setTextColor(col);
  cv->print(s);
}

// =====================================================
//  Jalali (Persian) calendar
// =====================================================
void gregorianToJalali(int gy, int gm, int gd, int &jy, int &jm, int &jd) {
  static const int gdm[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  int gy2 = (gm > 2) ? (gy + 1) : gy;
  long days = 355666L + (365L * gy) + ((gy2 + 3) / 4) - ((gy2 + 99) / 100) + ((gy2 + 399) / 400) + gd + gdm[gm - 1];
  jy = -1595 + (33 * (days / 12053));
  days %= 12053;
  jy += 4 * (days / 1461);
  days %= 1461;
  if (days > 365) {
    jy += (days - 1) / 365;
    days = (days - 1) % 365;
  }
  if (days < 186) {
    jm = 1 + days / 31;
    jd = 1 + (days % 31);
  } else {
    jm = 7 + (days - 186) / 30;
    jd = 1 + ((days - 186) % 30);
  }
}

const char *JALALI_MONTHS[12] = { "Farvardin", "Ordibehesht", "Khordad", "Tir", "Mordad", "Shahrivar",
                                  "Mehr", "Aban", "Azar", "Dey", "Bahman", "Esfand" };
const char *GREG_MONTHS[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
const char *WEEKDAYS[7] = { "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY" };
const char *WEEKDAYS3[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// =====================================================
//  Weather helpers
// =====================================================
const char *wxText(int c) {
  if (c == 0) return "CLEAR";
  if (c == 1) return "MOSTLY CLEAR";
  if (c == 2) return "PART CLOUDY";
  if (c == 3) return "OVERCAST";
  if (c == 45 || c == 48) return "FOG";
  if (c >= 51 && c <= 57) return "DRIZZLE";
  if (c >= 61 && c <= 67) return "RAIN";
  if (c >= 71 && c <= 77) return "SNOW";
  if (c >= 80 && c <= 82) return "SHOWERS";
  if (c == 85 || c == 86) return "SNOW SHOWERS";
  if (c >= 95) return "STORM";
  return "CLOUDY";
}

int wxKind(int c) {
  if (c == 0) return 0;
  if (c <= 2) return 1;
  if (c == 3) return 2;
  if (c == 45 || c == 48) return 3;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return 4;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return 5;
  if (c >= 95) return 6;
  return 2;
}

bool jsonNum(const String &s, int from, const char *key, float &out) {
  String k = String("\"") + key + "\":";
  int i = s.indexOf(k, from);
  if (i < 0) return false;
  i += k.length();
  out = s.substring(i).toFloat();
  return true;
}

void fetchWeather() {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(8);
  HTTPClient http;
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(cfg.lat, 4) +
               "&longitude=" + String(cfg.lon, 4) +
               "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&timezone=auto";
  if (!http.begin(client, url)) return;
  http.setTimeout(8000);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    int c = body.indexOf("\"current\":{");
    float t, h, w, wc;
    if (c >= 0 && jsonNum(body, c, "temperature_2m", t) && jsonNum(body, c, "relative_humidity_2m", h) &&
        jsonNum(body, c, "weather_code", wc) && jsonNum(body, c, "wind_speed_10m", w)) {
      wxTemp = t;
      wxHum = h;
      wxWind = w;
      wxCode = (int)wc;
      wxOk = true;
    }
  }
  http.end();
}

void weatherTask(void *) {
  unsigned long lastFetch = 0;
  bool first = true;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      unsigned long now = millis();
      bool due = wxRefresh || first || (now - lastFetch > WX_INTERVAL) || (!wxOk && now - lastFetch > 30000UL);
      if (due) {
        first = false;
        wxRefresh = false;
        lastFetch = now;
        fetchWeather();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// =====================================================
//  Icons
// =====================================================
void drawSunAt(int cx, int cy, float t, float len) {
  for (int k = 0; k < 8; k++) {
    float a = t * 0.7f + k * (PI / 4.0f);
    int x0 = cx + (int)(cosf(a) * 13), y0 = cy + (int)(sinf(a) * 13);
    int x1 = cx + (int)(cosf(a) * (13 + len)), y1 = cy + (int)(sinf(a) * (13 + len));
    fLine(x0, y0, x1, y1, C_SUN);
    fLine(x0 + 1, y0, x1 + 1, y1, C_SUN);
  }
  fCircle(cx, cy, 8, C_SUN);
  fCircle(cx - 2, cy - 2, 3, mix(C_SUN, C_WHITE, 170));
}

void drawSun(float t, float pct) {
  drawSunAt(CX, 56, t, 4.0f + pct * 0.07f);
}

void drawMoon(float t, uint16_t disc) {
  fCircle(CX, 56, 12, C_MOON);
  fCircle(CX + 6, 52, 11, disc);
  static const int16_t sx[3] = { 88, 154, 164 };
  static const int16_t sy[3] = { 70, 44, 72 };
  for (int i = 0; i < 3; i++) {
    float k = 0.5f + 0.5f * sinf(t * 3.0f + i * 2.1f);
    uint16_t col = mix(disc, C_WHITE, (uint8_t)(70 + 185 * k));
    int s = 1 + (k > 0.7f ? 1 : 0);
    hLine(sx[i] - s, sy[i], 2 * s + 1, col);
    vLine(sx[i], sy[i] - s, 2 * s + 1, col);
  }
}

void drawCloud(int cx, int cy, uint16_t col) {
  fCircle(cx - 11, cy + 3, 7, col);
  fCircle(cx, cy - 3, 10, col);
  fCircle(cx + 11, cy + 3, 8, col);
  fRect(cx - 11, cy + 3, 23, 8, col);
}

void drawWxIcon(int code, float t) {
  int k = wxKind(code);
  int cx = CX, cy = 50;
  uint16_t cloud = rgb(205, 214, 238), dark = rgb(140, 150, 182);
  switch (k) {
    case 0:
      drawSunAt(cx, cy + 2, t, 5);
      break;
    case 1:
      drawSunAt(cx - 9, cy - 4, t, 3);
      drawCloud(cx + 7, cy + 7, cloud);
      break;
    case 2:
      drawCloud(cx, cy, cloud);
      break;
    case 3:
      drawCloud(cx, cy - 4, dark);
      for (int i = 0; i < 3; i++) hLine(cx - 16 + i * 4, cy + 14 + i * 5, 30 - i * 4, C_GRAY);
      break;
    case 4:
      drawCloud(cx, cy - 4, dark);
      for (int i = 0; i < 3; i++) {
        int off = ((int)(t * 24) + i * 5) % 9;
        int x = cx - 10 + i * 10;
        int y0 = cy + 10 + off;
        fLine(x, y0, x - 2, y0 + 5, rgb(80, 170, 255));
      }
      break;
    case 5:
      drawCloud(cx, cy - 4, dark);
      for (int i = 0; i < 3; i++) {
        int off = ((int)(t * 12) + i * 3) % 12;
        fCircle(cx - 10 + i * 10, cy + 11 + off, 1, C_WHITE);
      }
      break;
    default:
      drawCloud(cx, cy - 6, dark);
      fTri(cx + 4, cy + 6, cx - 4, cy + 18, cx + 2, cy + 18, C_SUN);
      fTri(cx + 2, cy + 16, cx + 7, cy + 16, cx - 2, cy + 28, C_SUN);
      break;
  }
}

void drawHand(float ang, int len, int tail, int halfW, uint16_t col) {
  float c = cosf(ang), s = sinf(ang);
  float px = -s, py = c;
  int tx = CX + (int)(c * len), ty = CY + (int)(s * len);
  int b1x = CX + (int)(px * halfW), b1y = CY + (int)(py * halfW);
  int b2x = CX - (int)(px * halfW), b2y = CY - (int)(py * halfW);
  int ex = CX - (int)(c * tail), ey = CY - (int)(s * tail);
  fTri(tx, ty, b1x, b1y, b2x, b2y, col);
  fTri(ex, ey, b1x, b1y, b2x, b2y, col);
}

// number + degree sign + C, centered
void drawTempBig(int value, int baseY, uint16_t accent) {
  char num[8];
  snprintf(num, sizeof(num), "%d", value);
  int16_t x1, y1, cx1, cy1;
  uint16_t w, h, cw, ch;
  cv->setFont(&FreeSansBold24pt7b);
  cv->getTextBounds(num, 0, 0, &x1, &y1, &w, &h);
  cv->setFont(&FreeSansBold12pt7b);
  cv->getTextBounds("C", 0, 0, &cx1, &cy1, &cw, &ch);
  int total = (int)w + 4 + 10 + (int)cw;
  int sx = CX - total / 2;
  cv->setFont(&FreeSansBold24pt7b);
  cv->setTextColor(C_WHITE);
  cv->setCursor(sx - x1, baseY - by);
  cv->print(num);
  int dx = sx + (int)w + 4;
  dCircle(dx + 4, baseY - 26, 3, accent);
  dCircle(dx + 4, baseY - 26, 2, accent);
  cv->setFont(&FreeSansBold12pt7b);
  cv->setTextColor(accent);
  cv->setCursor(dx + 10 - cx1, baseY - by);
  cv->print("C");
}

// =====================================================
//  Timer helpers
// =====================================================
long timerLeftMs() {
  if (!timerRun) return 0;
  long l = (long)(timerEndMs - millis());
  return l < 0 ? 0 : l;
}

// =====================================================
//  Pages
// =====================================================
void drawNoTime(uint16_t accent) {
  textCentered("SYNCING", CX, 116, &FreeSansBold12pt7b, accent);
  textCentered(WiFi.status() == WL_CONNECTED ? "SETTING CLOCK" : "NO WIFI YET", CX, 144, &FreeSans9pt7b, C_GRAY);
}

void drawDash(const Frame &f, uint16_t accent, uint16_t disc) {
  if (f.night) drawMoon(f.t, disc);
  else         drawSun(f.t, f.pct);

  char num[8];
  snprintf(num, sizeof(num), "%d", (int)roundf(f.pct));
  int16_t x1, y1, px1, py1;
  uint16_t w, h, pw, ph;
  cv->setFont(&FreeSansBold24pt7b);
  cv->getTextBounds(num, 0, 0, &x1, &y1, &w, &h);
  cv->setFont(&FreeSansBold12pt7b);
  cv->getTextBounds("%", 0, 0, &px1, &py1, &pw, &ph);
  int total = (int)w + 4 + (int)pw;
  int sx = CX - total / 2;
  cv->setFont(&FreeSansBold24pt7b);
  cv->setTextColor(C_WHITE);
  cv->setCursor(sx - x1, 120 - by);
  cv->print(num);
  cv->setFont(&FreeSansBold12pt7b);
  cv->setTextColor(accent);
  cv->setCursor(sx + (int)w + 4 - px1, 120 - by);
  cv->print("%");

  const char *st = (f.level == 0) ? "DARK" : (f.level == 1) ? "LOW LIGHT" : "BRIGHT";
  textCentered(st, CX, 147, &FreeSansBold12pt7b, accent);

  const int gx = 62, gyBottom = 186, gh = 26;
  hLine(gx, gyBottom, HIST_N * 2, mix(disc, C_WHITE, 60));
  uint16_t barCol = mix(disc, accent, 90);
  for (int k = 0; k < HIST_N; k++) {
    int v = hist[(histHead + k) % HIST_N];
    int bh = (v * gh) / 100;
    if (bh < 1) bh = 1;
    int x = gx + k * 2;
    fRect(x, gyBottom - bh, 2, bh, barCol);
    fRect(x, gyBottom - bh, 2, 2, accent);
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "ADC %d", f.adc);
  cv->setFont();
  cv->setTextSize(1);
  cv->setTextColor(C_GRAY);
  cv->setCursor(CX - (int)strlen(buf) * 3, 190 - by);
  cv->print(buf);

  const int px = 72, py = 203, pwid = 96, phgt = 22;
  float pulse = 0.5f + 0.5f * sinf(f.t * 4.0f);
  if (f.led) {
    fRRect(px, py, pwid, phgt, 11, rgb(8, 60, 30));
    dRRect(px, py, pwid, phgt, 11, C_GREEN);
    dCircle(px + 16, py + 11, 5 + (int)(pulse * 3), rgb(20, 130, 65));
    fCircle(px + 16, py + 11, 5, C_GREEN);
  } else {
    fRRect(px, py, pwid, phgt, 11, rgb(26, 28, 40));
    dRRect(px, py, pwid, phgt, 11, rgb(90, 90, 115));
    fCircle(px + 16, py + 11, 5, rgb(90, 90, 115));
  }
  cv->setFont(&FreeSans9pt7b);
  cv->setTextColor(f.led ? C_GREEN : C_GRAY);
  cv->setCursor(px + 30, py + 16 - by);
  cv->print(f.led ? "LED ON" : "LED OFF");
}

void drawClock(uint16_t accent, uint16_t disc) {
  if (!timeValid) { drawNoTime(accent); return; }

  textCentered(WEEKDAYS[gTm.tm_wday], CX, 78, &FreeSans9pt7b, C_GRAY);

  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", gTm.tm_hour, gTm.tm_min);
  textCentered(b, CX, 122, &FreeSansBold24pt7b, C_WHITE);

  int jy, jm, jd;
  gregorianToJalali(gTm.tm_year + 1900, gTm.tm_mon + 1, gTm.tm_mday, jy, jm, jd);
  char jb[16];
  snprintf(jb, sizeof(jb), "%04d/%02d/%02d", jy, jm, jd);
  textCentered(jb, CX, 152, &FreeSansBold12pt7b, accent);
  textCentered(JALALI_MONTHS[jm - 1], CX, 174, &FreeSans9pt7b, C_GRAY);

  char gb[24];
  snprintf(gb, sizeof(gb), "%d %s %d", gTm.tm_mday, GREG_MONTHS[gTm.tm_mon], gTm.tm_year + 1900);
  textCentered(gb, CX, 196, &FreeSans9pt7b, mix(C_GRAY, disc, 110));
}

void drawAnalog(uint16_t accent, uint16_t bg) {
  for (int k = 0; k < 12; k++) {
    float a = k * PI / 6.0f;
    float c = cosf(a), s = sinf(a);
    bool big = (k % 3 == 0);
    int r0 = big ? 76 : 82;
    uint16_t col = big ? accent : C_GRAY;
    int x0 = CX + (int)(c * r0), y0 = CY + (int)(s * r0);
    int x1 = CX + (int)(c * 90), y1 = CY + (int)(s * 90);
    fLine(x0, y0, x1, y1, col);
    fLine(x0 + 1, y0, x1 + 1, y1, col);
    if (big) fLine(x0, y0 + 1, x1, y1 + 1, col);
  }
  if (!timeValid) { drawNoTime(accent); return; }

  float minutes = gTm.tm_min + (gSecF / 60.0f);
  float hours = (gTm.tm_hour % 12) + minutes / 60.0f;

  float aH = hours / 12.0f * TWO_PI - HALF_PI;
  float aM = minutes / 60.0f * TWO_PI - HALF_PI;
  float aS = gSecF / 60.0f * TWO_PI - HALF_PI;

  drawHand(aH, 46, 8, 4, C_WHITE);
  drawHand(aM, 70, 10, 3, C_WHITE);
  drawHand(aS, 82, 16, 1, accent);
  fCircle(CX, CY, 6, accent);
  fCircle(CX, CY, 3, bg);
}

void drawClimate(uint16_t accent) {
  if (!dhtOk) {
    textCentered("NO DATA", CX, 118, &FreeSansBold12pt7b, C_GRAY);
    textCentered("CHECK SENSOR", CX, 146, &FreeSans9pt7b, C_GRAY);
    return;
  }
  textCentered("INDOOR TEMP", CX, 74, &FreeSans9pt7b, C_GRAY);
  drawTempBig((int)roundf(gTemp), 118, accent);

  textCentered("HUMIDITY", CX, 148, &FreeSans9pt7b, C_GRAY);
  char hb[8];
  snprintf(hb, sizeof(hb), "%d%%", (int)roundf(gHum));
  textCentered(hb, CX, 174, &FreeSansBold12pt7b, C_WHITE);

  const char *comfort;
  if (gTemp < 18) comfort = "TOO COLD";
  else if (gTemp > 28) comfort = "TOO HOT";
  else if (gHum < 30) comfort = "TOO DRY";
  else if (gHum > 65) comfort = "TOO HUMID";
  else comfort = "COMFORTABLE";
  textCentered(comfort, CX, 198, &FreeSans9pt7b, accent);
}

void drawWeather(const Frame &f, uint16_t accent) {
  if (!wxOk) {
    textCentered("WEATHER", CX, 112, &FreeSansBold12pt7b, C_WHITE);
    textCentered(WiFi.status() == WL_CONNECTED ? "LOADING..." : "NO WIFI", CX, 140, &FreeSans9pt7b, C_GRAY);
    return;
  }
  drawWxIcon(wxCode, f.t);
  textCentered(cfg.city, CX, 94, &FreeSans9pt7b, C_GRAY);
  drawTempBig((int)roundf(wxTemp), 134, accent);
  textCentered(wxText(wxCode), CX, 160, &FreeSansBold12pt7b, accent);
  char b[24];
  snprintf(b, sizeof(b), "%d%% | %d km/h", (int)roundf(wxHum), (int)roundf(wxWind));
  textCentered(b, CX, 184, &FreeSans9pt7b, C_GRAY);
}

void drawAlarmPage(uint16_t accent, uint16_t disc) {
  textCentered("ALARM", CX, 68, &FreeSans9pt7b, (editField >= 0 && editField <= 2) ? accent : C_GRAY);

  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", cfg.alarmH, cfg.alarmM);
  textCentered(b, CX, 110, &FreeSansBold24pt7b, cfg.alarmOn ? C_WHITE : C_GRAY);

  int16_t x1, y1;
  uint16_t w, h, w2, h2;
  cv->setFont(&FreeSansBold24pt7b);
  cv->getTextBounds("00:00", 0, 0, &x1, &y1, &w, &h);
  cv->getTextBounds("00", 0, 0, &x1, &y1, &w2, &h2);
  int sx = CX - (int)w / 2;
  if (editField == 0) fRect(sx, 116, w2, 3, accent);
  if (editField == 1) fRect(sx + (int)w - (int)w2, 116, w2, 3, accent);

  const char *onoff = cfg.alarmOn ? "ON" : "OFF";
  textCentered(onoff, CX, 140, &FreeSansBold12pt7b, cfg.alarmOn ? C_GREEN : C_GRAY);
  if (editField == 2) {
    uint16_t w3, h3;
    cv->setFont(&FreeSansBold12pt7b);
    cv->getTextBounds(onoff, 0, 0, &x1, &y1, &w3, &h3);
    fRect(CX - (int)w3 / 2, 145, w3, 2, accent);
  }

  hLine(60, 153, 120, mix(disc, C_WHITE, 45));

  textCentered("TIMER", CX, 172, &FreeSans9pt7b, (editField == 3 || editField == 4) ? accent : C_GRAY);

  char tb[8];
  uint16_t tcol;
  if (timerRun) {
    long s = (timerLeftMs() + 999) / 1000;
    snprintf(tb, sizeof(tb), "%02ld:%02ld", s / 60, s % 60);
    tcol = C_GREEN;
  } else {
    snprintf(tb, sizeof(tb), "%02d:00", cfg.timerMin);
    tcol = C_WHITE;
  }
  textCentered(tb, CX, 194, &FreeSansBold12pt7b, tcol);
  if (editField == 3) {
    uint16_t tw, th, tw2, th2;
    cv->setFont(&FreeSansBold12pt7b);
    cv->getTextBounds("00:00", 0, 0, &x1, &y1, &tw, &th);
    cv->getTextBounds("00", 0, 0, &x1, &y1, &tw2, &th2);
    fRect(CX - (int)tw / 2, 199, tw2, 2, accent);
  }

  const char *hint;
  switch (editField) {
    case 0: hint = "SET HOUR"; break;
    case 1: hint = "SET MINUTE"; break;
    case 2: hint = "ALARM ON / OFF"; break;
    case 3: hint = "TIMER MINUTES"; break;
    case 4: hint = timerRun ? "PRESS TO STOP" : "PRESS TO START"; break;
    default: hint = "PRESS TO EDIT"; break;
  }
  textCentered(hint, CX, 214, &FreeSans9pt7b, editField >= 0 ? accent : C_GRAY);
}

void formatDuration(unsigned long ms, char *out, size_t n) {
  unsigned long totalMin = ms / 60000UL;
  snprintf(out, n, "%luh %02lum", totalMin / 60, totalMin % 60);
}

void drawStats(uint16_t accent, uint16_t disc) {
  textCentered("LIGHT STATS", CX, 58, &FreeSans9pt7b, C_GRAY);
  textCentered(ipStr[0] ? ipStr : "NO WIFI", CX, 77, &FreeSans9pt7b, accent);

  float avg = cntPct ? (float)(sumPct / cntPct) : 0;
  char v0[10], v1[10], v2[10], v3[16], v4[16];
  snprintf(v0, sizeof(v0), "%d%%", (int)maxPct);
  snprintf(v1, sizeof(v1), "%d%%", (int)minPct);
  snprintf(v2, sizeof(v2), "%d%%", (int)avg);
  formatDuration(ledOnMs, v3, sizeof(v3));
  formatDuration(millis(), v4, sizeof(v4));

  const char *labels[5] = { "MAX", "MIN", "AVG", "LED ON", "UPTIME" };
  const char *vals[5] = { v0, v1, v2, v3, v4 };
  const int y0 = 102;
  for (int i = 0; i < 5; i++) {
    int y = y0 + i * 22;
    textLeft(labels[i], 62, y, &FreeSans9pt7b, C_GRAY);
    textRight(vals[i], 178, y, &FreeSansBold12pt7b, i < 3 ? accent : C_WHITE);
    if (i < 4) hLine(62, y + 6, 116, mix(disc, C_WHITE, 45));
  }
}

void initStar(Star &s, bool spread) {
  s.a = random(0, 628) / 100.0f;
  s.d = spread ? random(4, 92) : random(3, 10);
  s.v = 0.6f + random(0, 100) / 60.0f;
}

void updateStars() {
  for (int i = 0; i < 36; i++) {
    stars[i].d += stars[i].v * (0.35f + stars[i].d / 35.0f);
    if (stars[i].d > 92) initStar(stars[i], false);
  }
}

void drawWarp(uint16_t accent, uint16_t disc) {
  for (int i = 0; i < 36; i++) {
    float c = cosf(stars[i].a), s = sinf(stars[i].a);
    float d = stars[i].d;
    float tail = d - stars[i].v * (2.0f + d / 10.0f);
    if (tail < 2) tail = 2;
    int x0 = CX + (int)(c * tail), y0 = CY + (int)(s * tail);
    int x1 = CX + (int)(c * d), y1 = CY + (int)(s * d);
    uint16_t col = mix(disc, mix(C_WHITE, accent, 90), (uint8_t)clampf(d * 3.2f, 20, 255));
    fLine(x0, y0, x1, y1, col);
    if (d > 60) fCircle(x1, y1, 1, col);
  }
  if (timeValid) {
    char b[8];
    snprintf(b, sizeof(b), "%02d:%02d", gTm.tm_hour, gTm.tm_min);
    textCentered(b, CX, CY + 8, &FreeSansBold12pt7b, C_WHITE);
  } else {
    textCentered("LIGHT OS", CX, CY + 8, &FreeSansBold12pt7b, C_WHITE);
  }
}

void drawDots(int pg, uint16_t accent) {
  const int gap = 10;
  int x0 = CX - (PAGES - 1) * gap / 2;
  for (int i = 0; i < PAGES; i++) {
    if (i == pg) fCircle(x0 + i * gap, 231, 3, accent);
    else         fCircle(x0 + i * gap, 231, 2, rgb(70, 75, 100));
  }
}

// =====================================================
//  Scene (drawn once per band)
// =====================================================
void pageParams(const Frame &f, float &ringPct, float &accentT) {
  switch (f.page) {
    case PG_CLOCK:
    case PG_ANALOG:
      ringPct = timeValid ? gSecF / 60.0f * 100.0f : 0;
      accentT = ringPct / 100.0f;
      break;
    case PG_CLIMATE:
      ringPct = dhtOk ? gHum : 0;
      accentT = dhtOk ? clampf((gTemp - 5.0f) / 35.0f, 0, 1) : 0.5f;
      break;
    case PG_WEATHER:
      ringPct = wxOk ? clampf((wxTemp + 10.0f) * 2.0f, 0, 100) : 0;
      accentT = ringPct / 100.0f;
      break;
    case PG_ALARM:
      ringPct = (timerRun && timerTotalMs > 0) ? clampf(100.0f * timerLeftMs() / timerTotalMs, 0, 100) : 0;
      accentT = 0.35f;
      break;
    default:
      ringPct = f.pct;
      accentT = f.pct / 100.0f;
      break;
  }
}

void renderScene(const Frame &f) {
  float ringPct, accentT;
  pageParams(f, ringPct, accentT);

  uint16_t accent = grad(accentT);
  uint16_t bg   = mix(NIGHT_BG, DAY_BG, (uint8_t)(clampf(f.pct, 0, 100) * 2.55f));
  uint16_t disc = mix(bg, accent, 24);
  uint16_t off  = mix(bg, rgb(90, 100, 140), 45);

  cv->fillScreen(bg);

  fCircle(CX, CY, 96, disc);
  dCircle(CX, CY, 96, mix(disc, accent, 70));

  if (f.ripple >= 0.0f) {
    float r1 = 20 + f.ripple * 76;
    uint8_t s1 = (uint8_t)((1.0f - f.ripple) * 220);
    dCircle(CX, CY, (int)r1, mix(disc, accent, s1));
    dCircle(CX, CY, (int)r1 + 1, mix(disc, accent, s1));
    float r2 = r1 - 14;
    if (r2 > 20) dCircle(CX, CY, (int)r2, mix(disc, accent, s1 / 2));
  }

  float fill = ringPct / 100.0f * N_SEG;
  int full = (int)fill;
  float frac = fill - full;
  for (int i = 0; i < N_SEG; i++) {
    uint16_t c;
    if (i < full) c = grad((float)i / (N_SEG - 1));
    else if (i == full && frac > 0.02f) c = mix(off, grad((float)i / (N_SEG - 1)), (uint8_t)(frac * 255));
    else c = off;
    fQuad(i, c);
  }

  float ang = (START_DEG + SWEEP_DEG * ringPct / 100.0f) * DEG_TO_RAD;
  int kx = CX + (int)(cosf(ang) * 108.5f);
  int ky = CY + (int)(sinf(ang) * 108.5f);
  fCircle(kx, ky, 8, C_WHITE);
  fCircle(kx, ky, 4, accent);

  if (f.boot) {
    textCentered("LIGHT OS", CX, 118, &FreeSansBold12pt7b, C_WHITE);
    cv->setFont();
    cv->setTextSize(1);
    cv->setTextColor(C_GRAY);
    cv->setCursor(CX - 24, 132 - by);
    cv->print("STARTING");
    return;
  }

  switch (f.page) {
    case PG_DASH:    drawDash(f, accent, disc); break;
    case PG_CLOCK:   drawClock(accent, disc); break;
    case PG_ANALOG:  drawAnalog(accent, disc); break;
    case PG_CLIMATE: drawClimate(accent); break;
    case PG_WEATHER: drawWeather(f, accent); break;
    case PG_ALARM:   drawAlarmPage(accent, disc); break;
    case PG_STATS:   drawStats(accent, disc); break;
    case PG_WARP:    drawWarp(accent, disc); break;
  }

  if (f.ring) {
    float p = 0.5f + 0.5f * sinf(f.t * 8.0f);
    uint16_t c = mix(rgb(120, 0, 10), rgb(255, 50, 60), (uint8_t)(p * 255));
    fCircle(CX, CY, 80, c);
    dCircle(CX, CY, 84, c);
    dCircle(CX, CY, 87, mix(c, bg, 140));
    textCentered(f.ring == 1 ? "ALARM" : "TIME UP", CX, CY + 2, &FreeSansBold12pt7b, C_WHITE);
    textCentered("PRESS KNOB", CX, CY + 26, &FreeSans9pt7b, C_WHITE);
  }

  drawDots(f.page, accent);
}

void present(const Frame &f) {
  for (int b = 0; b < BANDS; b++) {
    by = b * BAND_H;
    renderScene(f);
    tft.drawRGBBitmap(0, by, cv->getBuffer(), 240, BAND_H);
  }
}

// =====================================================
//  Logic
// =====================================================
int readLdr() {
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(LDR_PIN);
    delayMicroseconds(200);
  }
  return sum / 16;
}

float pctFromAdc(int adc) {
  return clampf((adc - ADC_MIN) * 100.0f / (ADC_MAX - ADC_MIN), 0, 100);
}

void startRipple(unsigned long now) {
  rippleStart = now;
  rippleOn = true;
}

void registerActivity(unsigned long now) {
  lastActivity = now;
  if (idle) {
    idle = false;
    page = PG_DASH;
    editField = -1;
    startRipple(now);
  }
}

void dismissRing() {
  alarmRing = false;
  timerRing = false;
  beepsLeft = 0;
  buzzerOn = false;
  digitalWrite(BUZZER_PIN, LOW);
}

void startTimer(unsigned long now) {
  timerTotalMs = (unsigned long)cfg.timerMin * 60000UL;
  timerEndMs = now + timerTotalMs;
  timerRun = true;
  timerRing = false;
}

void stopTimer() {
  timerRun = false;
}

void triggerAlarm(unsigned long now) {
  alarmRing = true;
  ringStart = now;
  lastRingBeep = 0;
  registerActivity(now);
}

void triggerTimer(unsigned long now) {
  timerRing = true;
  timerRun = false;
  ringStart = now;
  lastRingBeep = 0;
  registerActivity(now);
}

void updateAlarm(unsigned long now) {
  if (timerRun && (long)(now - timerEndMs) >= 0) triggerTimer(now);

  if (timeValid && cfg.alarmOn && !alarmRing && gTm.tm_hour == cfg.alarmH && gTm.tm_min == cfg.alarmM) {
    int key = gTm.tm_yday + (gTm.tm_year << 9);
    if (key != lastAlarmKey) {
      lastAlarmKey = key;
      triggerAlarm(now);
    }
  }

  if (alarmRing || timerRing) {
    if (beepsLeft == 0 && now - lastRingBeep >= 900) {
      startBeep(3);
      lastRingBeep = now;
    }
    if (now - ringStart >= 60000UL) dismissRing();
  }
}

void updateSensor(unsigned long now) {
  if (now - lastSensor < 60) return;
  unsigned long dt = now - lastSensor;
  lastSensor = now;

  adcAvg = (adcAvg * 3 + readLdr()) / 4;
  bool oldLed = ledState;

  if (cfg.ledMode == 1) {
    ledState = true;
    darkStart = lightStart = 0;
  } else if (cfg.ledMode == 2) {
    ledState = false;
    darkStart = lightStart = 0;
  } else if (adcAvg < cfg.darkTh) {
    lightStart = 0;
    if (darkStart == 0) darkStart = now;
    if (now - darkStart >= HOLD_MS) ledState = true;
  } else if (adcAvg > cfg.brightTh) {
    darkStart = 0;
    if (lightStart == 0) lightStart = now;
    if (now - lightStart >= HOLD_MS) ledState = false;
  } else {
    darkStart = 0;
    lightStart = 0;
  }

  digitalWrite(LED_PIN, ledState);

  if (ledState != oldLed) {
    if (!cfg.mute) startBeep(ledState ? 2 : 1);
    startRipple(now);
    registerActivity(now);
  }

  if (fabsf(adcAvg - refAdc) > WAKE_DELTA) {
    registerActivity(now);
    refAdc = adcAvg;
  } else {
    refAdc += (adcAvg - refAdc) / 64.0f;
  }

  float p = pctFromAdc(adcAvg);
  if (p > maxPct) maxPct = p;
  if (p < minPct) minPct = p;
  sumPct += p;
  cntPct++;
  if (ledState) ledOnMs += dt;

  if (now - lastHist >= 400) {
    lastHist = now;
    hist[histHead] = (uint8_t)p;
    histHead = (histHead + 1) % HIST_N;
  }
}

void updateDht(unsigned long now) {
  if (now - lastDht < 3000) return;
  lastDht = now;
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    gHum = h;
    gTemp = t;
    dhtOk = true;
    lastDhtOk = now;
  } else if (now - lastDhtOk > 20000) {
    dhtOk = false;
  }
}

void updateTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  timeValid = tv.tv_sec > 1700000000L;
  if (timeValid) {
    time_t tt = tv.tv_sec;
    localtime_r(&tt, &gTm);
    gSecF = gTm.tm_sec + tv.tv_usec / 1000000.0f;
  }
}

// =====================================================
//  Web server (phone control)
// =====================================================
const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en" dir="ltr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#070914">
<title>Light OS</title>
<style>
:root{--bg:#070914;--card:#10142b;--line:#1e2445;--txt:#eef1ff;--mut:#8a93b8;--a:#00c8ff;--g:#28ff78;--r:#ff4d5e}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0 auto;max-width:560px;background:radial-gradient(1000px 500px at 50% -10%,#171d4a,var(--bg)) fixed;color:var(--txt);font:15px/1.5 system-ui,Segoe UI,Roboto,Tahoma,sans-serif;padding:14px 14px 40px}
.top{display:flex;justify-content:space-between;align-items:center;gap:10px}
h1{font-size:20px;margin:6px 0 12px;display:flex;align-items:center;gap:10px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--r);flex:none}
.dot.on{background:var(--g);box-shadow:0 0 8px var(--g)}
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:14px;margin:12px 0}
.card h2{font-size:13px;letter-spacing:.06em;color:var(--mut);margin:0 0 10px;font-weight:600}
.row{display:flex;gap:10px;align-items:center;justify-content:space-between;flex-wrap:wrap}
.gap{margin-top:10px}
.gauge{--p:0;width:150px;height:150px;border-radius:50%;background:conic-gradient(var(--a) calc(var(--p)*1%),#1b2040 0);display:grid;place-items:center;flex:none;margin:auto}
.gauge>div{width:120px;height:120px;border-radius:50%;background:var(--card);display:grid;place-content:center;text-align:center}
.big{font-size:34px;font-weight:700}
.mut{color:var(--mut);font-size:13px}
.info{flex:1;min-width:160px;display:grid;gap:6px}
.info div{display:flex;justify-content:space-between;gap:8px}
.info b{direction:ltr}
.btn{background:#1a2050;color:var(--txt);border:1px solid var(--line);border-radius:12px;padding:10px 14px;font:inherit;cursor:pointer}
.btn.on{background:linear-gradient(135deg,#0aa8ff,#7a5cff);border-color:transparent;font-weight:700}
.btn.r{background:var(--r);border-color:transparent}
.seg{display:grid;grid-template-columns:repeat(auto-fit,minmax(98px,1fr));gap:8px}
.pill{display:inline-block;padding:6px 12px;border-radius:99px;background:#1a2050;border:1px solid var(--line)}
.pill.g{background:#0a3d22;border-color:var(--g);color:var(--g)}
.banner{background:#4a0d14;border:1px solid var(--r);border-radius:14px;padding:12px;margin:10px 0;display:none;justify-content:space-between;align-items:center;gap:8px}
input,select{background:#0b0e22;color:var(--txt);border:1px solid var(--line);border-radius:10px;padding:9px;font:inherit;max-width:100%}
input[type=range]{width:100%;padding:0;accent-color:#0aa8ff}
input[type=number]{width:90px}
</style>
</head>
<body>
<div class="top">
 <h1><span id="dot" class="dot"></span><span data-i="title"></span></h1>
 <button class="btn" id="langbtn" onclick="toggleLang()">English</button>
</div>

<div id="ring" class="banner"><b id="ringtxt"></b><button class="btn r" data-i="dismiss" onclick="api('dismiss=1')"></button></div>

<div class="card"><h2 data-i="live"></h2>
 <div class="row">
  <div class="gauge" id="gauge"><div><div class="big" id="pct">--</div><div class="mut" id="adc"></div></div></div>
  <div class="info">
   <div><span class="mut" data-i="time"></span><b id="time">--</b></div>
   <div><span class="mut" data-i="date"></span><b id="date">--</b></div>
   <div><span class="mut" data-i="gdate"></span><b id="gdate">--</b></div>
   <div><span class="mut" data-i="temp"></span><b id="temp">--</b></div>
   <div><span class="mut" data-i="hum"></span><b id="hum">--</b></div>
   <div><span class="mut" data-i="rssi"></span><b id="rssi">--</b></div>
  </div>
 </div>
 <div class="gap"><span id="led" class="pill">LED</span></div>
</div>

<div class="card"><h2 data-i="ledc"></h2>
 <div class="seg" id="ledseg">
  <button class="btn" data-v="0" data-i="auto" onclick="api('led=0')"></button>
  <button class="btn" data-v="1" data-i="on" onclick="api('led=1')"></button>
  <button class="btn" data-v="2" data-i="off" onclick="api('led=2')"></button>
 </div>
</div>

<div class="card"><h2 data-i="pagec"></h2>
 <div class="seg" id="pageseg"></div>
</div>

<div class="card"><h2 data-i="alarmc"></h2>
 <div class="row"><label data-i="alarmtime"></label><input type="time" id="at" onchange="setAlarm()"><button class="btn" id="aon" onclick="api('aon='+(S.alarm.on?0:1))">--</button></div>
 <div class="row gap"><label data-i="timermin"></label><input type="number" id="tm" min="1" max="99" onchange="api('tmin='+this.value)"><button class="btn" id="trun" onclick="api('trun='+(S.timer.run?0:1))">--</button></div>
 <div class="mut gap" id="tl"></div>
 <div class="row gap"><button class="btn" data-i="testalarm" onclick="api('testalarm=1')"></button><button class="btn" data-i="testbeep" onclick="api('beep=1')"></button></div>
</div>

<div class="card"><h2 data-i="wxc"></h2>
 <select id="city" onchange="pickCity()"></select>
 <div class="row gap"><input id="lat" placeholder="lat" size="8"><input id="lon" placeholder="lon" size="8"><button class="btn" data-i="apply" onclick="applyLoc()"></button></div>
 <div class="mut gap" id="wx"></div>
</div>

<div class="card"><h2 data-i="setc"></h2>
 <div class="row"><label data-i="dark"></label><b id="dkv"></b></div>
 <input type="range" id="dk" min="100" max="3000" step="10" oninput="$('dkv').textContent=this.value" onchange="api('dark='+this.value)">
 <div class="row gap"><label data-i="bright"></label><b id="brv"></b></div>
 <input type="range" id="br" min="100" max="3200" step="10" oninput="$('brv').textContent=this.value" onchange="api('bright='+this.value)">
 <div class="row gap"><label data-i="cyc"></label><button class="btn" id="cyc" onclick="api('cycle='+(S.cycle?0:1))">--</button></div>
 <div class="row gap"><label data-i="mutel"></label><button class="btn" id="mut" onclick="api('mute='+(S.mute?0:1))">--</button></div>
</div>

<div class="mut" id="foot" style="text-align:center"></div>

<script>
const $=i=>document.getElementById(i);
const TR={
fa:{title:'کنترل Light OS',live:'وضعیت زنده',time:'ساعت',date:'تاریخ شمسی',gdate:'تاریخ میلادی',temp:'دمای اتاق',hum:'رطوبت',rssi:'سیگنال وای‌فای',
ledc:'کنترل LED',auto:'خودکار',on:'همیشه روشن',off:'همیشه خاموش',pagec:'صفحهٔ نمایشگر',alarmc:'زنگ هشدار و تایمر',alarmtime:'ساعت زنگ',timermin:'تایمر (دقیقه)',
testalarm:'تست زنگ',testbeep:'تست بوق',wxc:'آب‌وهوا (اینترنت)',apply:'اعمال',setc:'تنظیمات',dark:'آستانهٔ تاریکی (LED روشن شود)',bright:'آستانهٔ روشنایی (LED خاموش شود)',
cyc:'چرخش خودکار صفحه‌ها (بعد از ۱ دقیقه بی‌حرکتی)',mutel:'بوق تغییر LED',dismiss:'قطع کن',
ledOn:'LED روشن',ledOff:'LED خاموش',alOn:'زنگ فعال',alOff:'زنگ غیرفعال',start:'شروع',stop:'توقف',left:'زمان باقی‌مانده: ',on2:'روشن',off2:'خاموش',muted:'بی‌صدا',active:'فعال',
ringA:'زنگ هشدار!',ringT:'تایمر تمام شد!',wxload:'در حال دریافت آب‌وهوا...',humw:'رطوبت',wind:'باد',up:'روشن‌مانده',lightw:'نور',max:'بیشینه',min:'کمینه',avg:'میانگین',custom:'مختصات دلخواه',
pages:['داشبورد','ساعت','ساعت عقربه‌ای','دما و رطوبت','آب‌وهوا','زنگ و تایمر','آمار','اسکرین‌سیور']},
en:{title:'Light OS control',live:'Live status',time:'Time',date:'Jalali date',gdate:'Gregorian date',temp:'Room temperature',hum:'Humidity',rssi:'Wi-Fi signal',
ledc:'LED control',auto:'Auto',on:'Always on',off:'Always off',pagec:'Display page',alarmc:'Alarm & timer',alarmtime:'Alarm time',timermin:'Timer (minutes)',
testalarm:'Test alarm',testbeep:'Test beep',wxc:'Weather (internet)',apply:'Apply',setc:'Settings',dark:'Dark threshold (LED turns on)',bright:'Bright threshold (LED turns off)',
cyc:'Auto-cycle pages (after 1 min of no activity)',mutel:'LED change beeps',dismiss:'Dismiss',
ledOn:'LED ON',ledOff:'LED OFF',alOn:'Alarm ON',alOff:'Alarm OFF',start:'Start',stop:'Stop',left:'Time left: ',on2:'On',off2:'Off',muted:'Muted',active:'Active',
ringA:'Alarm!',ringT:'Timer finished!',wxload:'Loading weather...',humw:'humidity',wind:'wind',up:'Uptime',lightw:'Light',max:'max',min:'min',avg:'avg',custom:'Custom coordinates',
pages:['Dashboard','Clock','Analog clock','Climate','Weather','Alarm & timer','Stats','Warp screensaver']}
};
const CITIES=[["تهران","Tehran",35.6892,51.3890],["مشهد","Mashhad",36.2605,59.6168],["اصفهان","Isfahan",32.6546,51.6680],["شیراز","Shiraz",29.5918,52.5837],["تبریز","Tabriz",38.0800,46.2919],["کرج","Karaj",35.8400,50.9391],["اهواز","Ahvaz",31.3183,48.6706],["قم","Qom",34.6416,50.8746],["کرمانشاه","Kermanshah",34.3142,47.0650],["رشت","Rasht",37.2808,49.5832],["یزد","Yazd",31.8974,54.3569],["کرمان","Kerman",30.2839,57.0834],["ارومیه","Urmia",37.5527,45.0761],["زاهدان","Zahedan",29.4963,60.8629],["بندرعباس","BandarAbbas",27.1832,56.2666],["همدان","Hamedan",34.7992,48.5146],["اراک","Arak",34.0917,49.6892],["سنندج","Sanandaj",35.3219,46.9862],["اردبیل","Ardabil",38.2498,48.2933],["بوشهر","Bushehr",28.9234,50.8203],["ساری","Sari",36.5633,53.0601],["گرگان","Gorgan",36.8427,54.4353],["زنجان","Zanjan",36.6736,48.4787],["قزوین","Qazvin",36.2797,50.0049]];
let L=(function(){try{const s=localStorage.getItem('lang');if(s==='fa'||s==='en')return s}catch(e){}return (navigator.language||'').toLowerCase().indexOf('fa')===0?'fa':'en'})();
let S={},curName='Custom',locInit=false;
function t(k){return TR[L][k]}
function pad(n){return String(n).padStart(2,'0')}
function fmt(s){return pad(Math.floor(s/60))+':'+pad(s%60)}
function setv(id,v){const e=$(id);if(e&&document.activeElement!==e)e.value=v}
function seg(id,v){document.querySelectorAll('#'+id+' button').forEach(b=>b.classList.toggle('on',b.dataset.v==v))}
async function api(q){try{const r=await fetch('/api/set?'+q);S=await r.json();render()}catch(e){}}
async function poll(){try{const r=await fetch('/api/state');S=await r.json();$('dot').className='dot on';render()}catch(e){$('dot').className='dot'}}
function setAlarm(){const v=$('at').value;if(!v)return;const p=v.split(':');api('ah='+p[0]+'&am='+p[1])}
function pickCity(){const c=CITIES[$('city').value];if(!c)return;$('lat').value=c[2];$('lon').value=c[3];curName=c[1]}
function applyLoc(){api('lat='+$('lat').value+'&lon='+$('lon').value+'&city='+encodeURIComponent(curName))}
function buildLists(){
 const ps=$('pageseg');ps.innerHTML='';
 t('pages').forEach((n,i)=>{const b=document.createElement('button');b.className='btn';b.dataset.v=i;b.textContent=n;b.onclick=()=>api('page='+i);ps.appendChild(b)});
 const cs=$('city');const keep=cs.value;cs.innerHTML='';
 CITIES.forEach((c,i)=>{const o=document.createElement('option');o.value=i;o.textContent=(L==='fa')?c[0]:c[1];cs.appendChild(o)});
 const o=document.createElement('option');o.value=-1;o.textContent=t('custom');cs.appendChild(o);
 if(keep!=='')cs.value=keep;
}
function applyLang(){
 document.documentElement.lang=L;
 document.documentElement.dir=(L==='fa')?'rtl':'ltr';
 document.querySelectorAll('[data-i]').forEach(e=>{e.textContent=t(e.dataset.i)});
 $('langbtn').textContent=(L==='fa')?'English':'فارسی';
 buildLists();
 render();
}
function toggleLang(){L=(L==='fa')?'en':'fa';try{localStorage.setItem('lang',L)}catch(e){}applyLang()}
function render(){
 if(!S.time)return;
 $('gauge').style.setProperty('--p',S.pct);
 $('pct').textContent=Math.round(S.pct)+'%';
 $('adc').textContent='ADC '+S.adc;
 $('time').textContent=S.time;$('date').textContent=S.date;$('gdate').textContent=S.gdate;
 $('temp').textContent=S.dht?S.temp+' °C':'--';
 $('hum').textContent=S.dht?S.hum+' %':'--';
 $('rssi').textContent=S.rssi+' dBm';
 const l=$('led');l.textContent=S.led?t('ledOn'):t('ledOff');l.className='pill'+(S.led?' g':'');
 seg('ledseg',S.ledMode);seg('pageseg',S.page);
 const rg=S.alarm.ring||S.timer.ring;$('ring').style.display=rg?'flex':'none';
 $('ringtxt').textContent=S.alarm.ring?t('ringA'):t('ringT');
 setv('at',pad(S.alarm.h)+':'+pad(S.alarm.m));setv('tm',S.timer.set);
 $('aon').textContent=S.alarm.on?t('alOn'):t('alOff');$('aon').classList.toggle('on',S.alarm.on);
 $('trun').textContent=S.timer.run?t('stop'):t('start');$('trun').classList.toggle('on',S.timer.run);
 $('tl').textContent=S.timer.run?(t('left')+fmt(S.timer.left)):'';
 setv('dk',S.dark);setv('br',S.bright);$('dkv').textContent=S.dark;$('brv').textContent=S.bright;
 $('cyc').textContent=S.cycle?t('on2'):t('off2');$('cyc').classList.toggle('on',S.cycle);
 $('mut').textContent=S.mute?t('muted'):t('active');$('mut').classList.toggle('on',!S.mute);
 if(!locInit){locInit=true;setv('lat',S.lat);setv('lon',S.lon);curName=S.city;const i=CITIES.findIndex(c=>c[1]==S.city);$('city').value=i>=0?i:-1}
 $('wx').textContent=S.wx.ok?(S.wx.city+': '+S.wx.t+'°C | '+S.wx.txt+' | '+t('humw')+' '+S.wx.h+'% | '+t('wind')+' '+S.wx.w+' km/h'):t('wxload');
 $('foot').textContent=t('up')+': '+Math.floor(S.up/3600)+'h '+pad(Math.floor(S.up/60)%60)+'m | '+t('lightw')+': '+t('max')+' '+S.st.max+'% / '+t('min')+' '+S.st.min+'% / '+t('avg')+' '+S.st.avg+'%';
}
applyLang();poll();setInterval(poll,1500);
</script>
</body>
</html>)HTML";

inline const char *jb(bool v) { return v ? "true" : "false"; }

String stateJson() {
  char b[40];
  String s;
  s.reserve(1100);
  float avg = cntPct ? (float)(sumPct / cntPct) : 0;

  s += "{\"pct\":";
  s += String(currentPercent, 0);
  s += ",\"adc\":";
  s += adcAvg;
  s += ",\"led\":";
  s += jb(ledState);
  s += ",\"ledMode\":";
  s += (int)cfg.ledMode;
  s += ",\"temp\":";
  s += String(gTemp, 0);
  s += ",\"hum\":";
  s += String(gHum, 0);
  s += ",\"dht\":";
  s += jb(dhtOk);

  if (timeValid) {
    snprintf(b, sizeof(b), "%02d:%02d:%02d", gTm.tm_hour, gTm.tm_min, gTm.tm_sec);
  } else {
    snprintf(b, sizeof(b), "--:--:--");
  }
  s += ",\"time\":\"";
  s += b;
  s += "\",\"date\":\"";
  if (timeValid) {
    int jy, jm, jd;
    gregorianToJalali(gTm.tm_year + 1900, gTm.tm_mon + 1, gTm.tm_mday, jy, jm, jd);
    snprintf(b, sizeof(b), "%04d/%02d/%02d", jy, jm, jd);
    s += b;
    s += "\",\"gdate\":\"";
    snprintf(b, sizeof(b), "%s %d %s %d", WEEKDAYS3[gTm.tm_wday], gTm.tm_mday, GREG_MONTHS[gTm.tm_mon], gTm.tm_year + 1900);
    s += b;
  } else {
    s += "--\",\"gdate\":\"--";
  }
  s += "\",\"page\":";
  s += page;
  s += ",\"cycle\":";
  s += jb(cfg.cycle);
  s += ",\"mute\":";
  s += jb(cfg.mute);
  s += ",\"dark\":";
  s += cfg.darkTh;
  s += ",\"bright\":";
  s += cfg.brightTh;

  s += ",\"alarm\":{\"h\":";
  s += (int)cfg.alarmH;
  s += ",\"m\":";
  s += (int)cfg.alarmM;
  s += ",\"on\":";
  s += jb(cfg.alarmOn);
  s += ",\"ring\":";
  s += jb(alarmRing);
  s += "},\"timer\":{\"set\":";
  s += (int)cfg.timerMin;
  s += ",\"run\":";
  s += jb(timerRun);
  s += ",\"left\":";
  s += (long)((timerLeftMs() + 999) / 1000);
  s += ",\"ring\":";
  s += jb(timerRing);

  s += "},\"wx\":{\"ok\":";
  s += jb(wxOk);
  s += ",\"t\":";
  s += String(wxTemp, 0);
  s += ",\"h\":";
  s += String(wxHum, 0);
  s += ",\"w\":";
  s += String(wxWind, 0);
  s += ",\"txt\":\"";
  s += wxText(wxCode);
  s += "\",\"city\":\"";
  s += cfg.city;
  s += "\"},\"lat\":";
  s += String(cfg.lat, 4);
  s += ",\"lon\":";
  s += String(cfg.lon, 4);
  s += ",\"city\":\"";
  s += cfg.city;
  s += "\",\"rssi\":";
  s += WiFi.RSSI();
  s += ",\"up\":";
  s += (unsigned long)(millis() / 1000);
  s += ",\"st\":{\"max\":";
  s += (int)maxPct;
  s += ",\"min\":";
  s += (int)minPct;
  s += ",\"avg\":";
  s += (int)avg;
  s += "}}";
  return s;
}

void setCity(const String &n) {
  int j = 0;
  for (unsigned i = 0; i < n.length() && j < 15; i++) {
    char c = n[i];
    if (isalnum((unsigned char)c) || c == ' ' || c == '-') cfg.city[j++] = c;
  }
  cfg.city[j] = 0;
  if (j == 0) strcpy(cfg.city, "Custom");
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleState() {
  server.send(200, "application/json", stateJson());
}

void handleSet() {
  unsigned long now = millis();

  if (server.hasArg("led")) {
    cfg.ledMode = (uint8_t)constrain(server.arg("led").toInt(), 0, 2);
    markDirty();
  }
  if (server.hasArg("page")) {
    int p = server.arg("page").toInt();
    if (p >= 0 && p < PAGES) {
      page = p;
      idle = false;
      editField = -1;
      lastActivity = now;
      startRipple(now);
    }
  }
  if (server.hasArg("cycle")) { cfg.cycle = server.arg("cycle") == "1"; markDirty(); }
  if (server.hasArg("mute"))  { cfg.mute = server.arg("mute") == "1"; markDirty(); }
  if (server.hasArg("ah"))    { cfg.alarmH = (uint8_t)constrain(server.arg("ah").toInt(), 0, 23); lastAlarmKey = -1; markDirty(); }
  if (server.hasArg("am"))    { cfg.alarmM = (uint8_t)constrain(server.arg("am").toInt(), 0, 59); lastAlarmKey = -1; markDirty(); }
  if (server.hasArg("aon"))   { cfg.alarmOn = server.arg("aon") == "1"; lastAlarmKey = -1; markDirty(); }
  if (server.hasArg("tmin"))  { cfg.timerMin = (uint8_t)constrain(server.arg("tmin").toInt(), 1, 99); markDirty(); }
  if (server.hasArg("trun")) {
    if (server.arg("trun") == "1") startTimer(now);
    else stopTimer();
  }
  if (server.hasArg("dark"))   { cfg.darkTh = constrain(server.arg("dark").toInt(), 50, 3500); markDirty(); }
  if (server.hasArg("bright")) { cfg.brightTh = constrain(server.arg("bright").toInt(), 50, 3600); markDirty(); }
  if (cfg.brightTh < cfg.darkTh + 20) cfg.brightTh = cfg.darkTh + 20;

  if (server.hasArg("lat") && server.hasArg("lon")) {
    float la = server.arg("lat").toFloat();
    float lo = server.arg("lon").toFloat();
    if (la >= -90 && la <= 90 && lo >= -180 && lo <= 180 && (la != 0 || lo != 0)) {
      cfg.lat = la;
      cfg.lon = lo;
      if (server.hasArg("city")) setCity(server.arg("city"));
      wxOk = false;
      wxRefresh = true;
      markDirty();
    }
  }
  if (server.hasArg("beep")) startBeep(2);
  if (server.hasArg("dismiss")) dismissRing();
  if (server.hasArg("testalarm")) triggerAlarm(now);

  server.send(200, "application/json", stateJson());
}

void updateNet(unsigned long now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!timeConfigured) {
      // Iran = UTC+3:30 (12600 s), no daylight saving
      configTime(12600, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
      timeConfigured = true;
    }
    if (!serverStarted) {
      server.on("/", handleRoot);
      server.on("/api/state", handleState);
      server.on("/api/set", handleSet);
      server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
      server.begin();
      if (MDNS.begin("lightos")) MDNS.addService("http", "tcp", 80);
      serverStarted = true;
      snprintf(ipStr, sizeof(ipStr), "%s", WiFi.localIP().toString().c_str());
      Serial.print("Web control: http://");
      Serial.print(ipStr);
      Serial.println("  (or http://lightos.local)");
    }
  } else if (now - lastWifiTry > 15000UL || lastWifiTry == 0) {
    lastWifiTry = now ? now : 1;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

// =====================================================
//  Encoder / input
// =====================================================
void IRAM_ATTR encISR() {
  encState = ((encState << 2) | (digitalRead(ENC_CLK) << 1) | digitalRead(ENC_DT)) & 0x0F;
  encAcc += encTable[encState];
  if (encAcc >= 4) {
    encDelta = encDelta + 1;
    encAcc = 0;
  } else if (encAcc <= -4) {
    encDelta = encDelta - 1;
    encAcc = 0;
  }
}

void applyEdit(int d) {
  switch (editField) {
    case 0: cfg.alarmH = (uint8_t)((((int)cfg.alarmH + d) % 24 + 24) % 24); break;
    case 1: cfg.alarmM = (uint8_t)((((int)cfg.alarmM + d) % 60 + 60) % 60); break;
    case 2: cfg.alarmOn = !cfg.alarmOn; break;
    case 3: cfg.timerMin = (uint8_t)clampf((float)((int)cfg.timerMin + d), 1, 99); break;
    default: break;
  }
  lastAlarmKey = -1;
  markDirty();
}

void onShortPress(unsigned long now) {
  if (alarmRing || timerRing) {
    dismissRing();
    lastActivity = now;
    return;
  }
  if (idle) {
    registerActivity(now);
    return;
  }
  lastActivity = now;
  if (page == PG_ALARM) {
    if (editField < 0) editField = 0;
    else if (editField < 4) editField++;
    else {
      if (timerRun) stopTimer();
      else startTimer(now);
      editField = -1;
    }
  } else {
    page = PG_DASH;
    startRipple(now);
  }
}

void onLongPress(unsigned long now) {
  dismissRing();
  editField = -1;
  if (idle) {
    registerActivity(now);
  } else {
    page = PG_DASH;
    lastActivity = now;
    startRipple(now);
  }
}

void handleInput(unsigned long now) {
  int d;
  noInterrupts();
  d = encDelta;
  encDelta = 0;
  interrupts();
  if (ENC_REVERSE) d = -d;

  if (d != 0 && !(alarmRing || timerRing)) {
    if (idle) {
      registerActivity(now);
    } else if (page == PG_ALARM && editField >= 0) {
      applyEdit(d);
      lastActivity = now;
    } else {
      page = ((page + d) % PAGES + PAGES) % PAGES;
      lastActivity = now;
      startRipple(now);
    }
  }

  bool sw = digitalRead(ENC_SW);
  if (sw != lastSw && now - swChange > 30) {
    swChange = now;
    lastSw = sw;
    if (sw == LOW) {
      swDown = true;
      swDownAt = now;
      longFired = false;
    } else {
      if (swDown && !longFired) onShortPress(now);
      swDown = false;
    }
  }
  if (swDown && !longFired && now - swDownAt >= 700) {
    longFired = true;
    onLongPress(now);
  }
}

void updateIdle(unsigned long now) {
  if (!cfg.cycle) {
    if (idle) idle = false;
    return;
  }
  if (alarmRing || timerRing) return;
  if (!idle && now - lastActivity >= IDLE_MS) {
    idle = true;
    idleIdx = 0;
    editField = -1;
    page = IDLE_ORDER[0];
    lastCycle = now;
    startRipple(now);
  } else if (idle && now - lastCycle >= CYCLE_MS) {
    idleIdx = (idleIdx + 1) % IDLE_COUNT;
    page = IDLE_ORDER[idleIdx];
    lastCycle = now;
    startRipple(now);
  }
}

// =====================================================
//  Setup / Loop
// =====================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.begin(115200);
  loadSettings();

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  SPI.begin(TFT_SCL, -1, TFT_SDA, TFT_CS);
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(0);

  cv = new GFXcanvas16(240, BAND_H);
  if (!cv || !cv->getBuffer()) {
    Serial.println("Canvas allocation failed!");
    while (true) delay(1000);
  }

  for (int i = 0; i < N_SEG; i++) {
    float a0 = (START_DEG + i * SWEEP_DEG / N_SEG + 0.7f) * DEG_TO_RAD;
    float a1 = (START_DEG + (i + 1) * SWEEP_DEG / N_SEG - 0.7f) * DEG_TO_RAD;
    quadPts[i][0] = CX + lroundf(R_IN  * cosf(a0));
    quadPts[i][1] = CY + lroundf(R_IN  * sinf(a0));
    quadPts[i][2] = CX + lroundf(R_OUT * cosf(a0));
    quadPts[i][3] = CY + lroundf(R_OUT * sinf(a0));
    quadPts[i][4] = CX + lroundf(R_OUT * cosf(a1));
    quadPts[i][5] = CY + lroundf(R_OUT * sinf(a1));
    quadPts[i][6] = CX + lroundf(R_IN  * cosf(a1));
    quadPts[i][7] = CY + lroundf(R_IN  * sinf(a1));
  }

  for (int p = 0; p <= 100; p += 4) {
    Frame f = {};
    f.pct = p;
    f.ripple = -1;
    f.boot = true;
    present(f);
  }

  dht.begin();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("lightos");

  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT), encISR, CHANGE);

  adcAvg = readLdr();
  refAdc = adcAvg;
  currentPercent = pctFromAdc(adcAvg);
  maxPct = minPct = currentPercent;
  for (int i = 0; i < HIST_N; i++) hist[i] = (uint8_t)currentPercent;
  for (int i = 0; i < 36; i++) initStar(stars[i], true);

  xTaskCreatePinnedToCore(weatherTask, "weather", 12288, NULL, 1, NULL, 0);

  lastSensor = millis();
  lastActivity = millis();
}

void loop() {
  unsigned long now = millis();

  updateNet(now);
  updateSensor(now);
  updateDht(now);
  handleInput(now);
  updateIdle(now);
  updateAlarm(now);
  updateBeep();
  if (serverStarted) server.handleClient();

  if (cfgDirty && now - cfgDirtyAt > 1500) {
    cfgDirty = false;
    saveSettings();
  }

  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;

    float target = pctFromAdc(adcAvg);
    float d = target - currentPercent;
    if (fabsf(d) < 0.15f) currentPercent = target;
    else currentPercent += d * 0.18f;

    if (page == PG_WARP) updateStars();
    updateTime();

    Frame f = {};
    f.pct = currentPercent;
    f.adc = adcAvg;
    f.level = (adcAvg < cfg.darkTh) ? 0 : (adcAvg <= cfg.brightTh ? 1 : 2);
    f.night = ledState;
    f.led = ledState;
    f.t = now / 1000.0f;
    f.page = page;
    f.boot = false;
    f.ring = alarmRing ? 1 : (timerRing ? 2 : 0);

    f.ripple = -1;
    if (rippleOn) {
      float r = (now - rippleStart) / 650.0f;
      if (r >= 1.0f) rippleOn = false;
      else f.ripple = r;
    }

    present(f);
  }

  if (now - lastSerial >= 2000) {
    lastSerial = now;
    Serial.print("ADC=");
    Serial.print(adcAvg);
    Serial.print(" light=");
    Serial.print(currentPercent, 0);
    Serial.print("% LED=");
    Serial.print(ledState ? "ON" : "OFF");
    Serial.print(" page=");
    Serial.print(page);
    Serial.print(idle ? " (idle)" : "");
    Serial.print(" wifi=");
    Serial.print(WiFi.status() == WL_CONNECTED ? ipStr : "no");
    Serial.print(" wx=");
    Serial.println(wxOk ? "ok" : "none");
  }
}
