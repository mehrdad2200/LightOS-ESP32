// =====================================================
//  Wi-Fi + NTP time test (Iran time, UTC+3:30)
//  Only tests the connection and the clock.
//  Open Serial Monitor at 115200 baud.
// =====================================================

#include <WiFi.h>
#include <time.h>
#include <sys/time.h>

const char *WIFI_SSID = "YOUR_WIFI_NAME";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Iran = UTC+3:30 = 12600 seconds, no daylight saving
const long GMT_OFFSET_SEC = 12600;
const int  DST_OFFSET_SEC = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++tries >= 40) {          // 20 seconds
      Serial.println();
      Serial.println("Could NOT connect. Check name/password and that the modem is 2.4 GHz.");
      return;
    }
  }

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.google.com", "time.cloudflare.com");

  Serial.print("Waiting for time");
  struct tm t;
  int n = 0;
  while (!getLocalTime(&t, 500)) {
    Serial.print(".");
    if (++n >= 20) {
      Serial.println();
      Serial.println("Could not get time. Try another NTP server.");
      return;
    }
  }
  Serial.println();
  Serial.println("Time synced!");
}

void loop() {
  struct tm t;
  if (getLocalTime(&t, 0)) {
    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%d  %H:%M:%S  (%A)", &t);
    Serial.println(buf);
  } else if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No Wi-Fi");
  } else {
    Serial.println("Waiting for time...");
  }
  delay(1000);
}
