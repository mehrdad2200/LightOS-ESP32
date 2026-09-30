# Configuration

There are three kinds of settings:

1. **Compile-time constants** at the top of `firmware/light_os/light_os.ino` (this page).
2. **Wi-Fi credentials** in `firmware/light_os/secrets.h`.
3. **Runtime settings** changed from the phone page or the knob and saved automatically (see [usage.md](usage.md)).

---

## 1. Constants in `light_os.ino`

| Constant | Default | Meaning |
|---|---|---|
| `ENC_REVERSE` | `false` | Set `true` if turning the knob clockwise goes to the previous page |
| `IDLE_MS` | `60000` | Milliseconds without activity before idle mode starts |
| `CYCLE_MS` | `9000` | Milliseconds each page is shown in idle mode |
| `WAKE_DELTA` | `90` | How big a sudden light change (ADC units) must be to wake the screen. Raise it if the screen keeps waking by itself |
| `WX_INTERVAL` | 15 minutes | How often the weather is refreshed |
| `ADC_MIN` | `280` | Sensor reading that is shown as 0 % |
| `ADC_MAX` | `1850` | Sensor reading that is shown as 100 % |
| `HOLD_MS` | `1400` | How long the light must stay dark/bright before the LED switches (auto mode) |
| `FRAME_MS` | `20` | Minimum time between two display frames |
| pin `#define`s | see wiring | `LDR_PIN`, `LED_PIN`, `BUZZER_PIN`, `TFT_*`, `ENC_*`, `DHT_PIN` |

### Default runtime settings
The line `Settings cfg = { 600, 720, 0, true, false, 7, 0, false, 5, 35.6892f, 51.3890f, "Tehran" };` holds the values used
when nothing has been saved yet, in this order: dark threshold, bright threshold, LED mode (0 auto), auto-cycle, mute,
alarm hour, alarm minute, alarm on, timer minutes, latitude, longitude, city name (max 15 characters).

To wipe saved settings back to these defaults: in Arduino IDE enable *Tools -> Erase All Flash Before Sketch Upload*, upload once, then disable it again.

### Other values inside the code
| Where | What |
|---|---|
| `drawClimate()` | Comfort limits: 18 C, 28 C, 30 %, 65 % |
| `updateSensor()` | LED logic; smoothing `adcAvg = (adcAvg*3 + reading)/4`, sampling every 60 ms |
| `handleInput()` | Long-press time (700 ms), button debounce (30 ms) |
| `updateAlarm()` | Alarm/timer ring length (60 s) and beep rhythm (3 beeps every 900 ms) |
| `startBeep()`/`updateBeep()` | Beep length (70 ms on / 70 ms off) |
| `initStar()`/`updateStars()` | Warp star count (36) and speed |

---

## 2. Time zone and NTP

The clock uses `configTime(12600, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");` inside `updateNet()`.

- `12600` is the offset from UTC in **seconds** - Iran is UTC+3:30 = 3.5 x 3600. Examples: UTC+1 -> `3600`, UTC+0 -> `0`, UTC-5 -> `-18000`.
- The second number is a fixed daylight-saving offset (seconds). `configTime` does **not** switch DST automatically.
- For automatic DST use a POSIX time-zone string instead (available in ESP32 core 2.x and later), for example:
  ```cpp
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
  ```
- The Jalali date is always computed from the local time; if you do not need it, simply do not show that line (`drawClock()`).

---

## 3. Weather location

The easiest way: choose a city in the phone page (Weather card). It is saved.

To change the default city in code, edit the initial `cfg` values (latitude, longitude, name), or add cities to the `CITIES` array in the web page inside `INDEX_HTML`
(format: `["Persian name","English name",latitude,longitude]`). The English name is what appears on the device display
(the display font has no Persian glyphs).

Weather is requested from
`https://api.open-meteo.com/v1/forecast?latitude=..&longitude=..&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&timezone=auto`.

---

## 4. Names

| Item | Where | Default |
|---|---|---|
| Network hostname / mDNS name | `WiFi.setHostname("lightos")` and `MDNS.begin("lightos")` | `lightos` (-> `lightos.local`) |
| Web server port | `WebServer server(80);` | 80 |

---

## 5. Calibrating the light sensor for your LDR

Every LDR/resistor pair is a little different, so calibrate once:

1. Open the dashboard (or the phone page) - it shows `ADC nnnn`.
2. **Darkest situation** (cover the sensor with your hand): note the value, e.g. 250. Set `ADC_MIN` slightly below it.
3. **Brightest situation** (a lamp close to the sensor or daylight): note the value, e.g. 2200. Set `ADC_MAX` a bit below it.
4. Decide where the LED should switch: read the ADC value at the dimness where you want the LED to turn on (-> dark threshold)
   and where it should turn off again (-> bright threshold, higher). Set them with the sliders on the phone page - no re-upload needed.
5. Keep a gap of at least ~80-100 units between the thresholds so the LED does not flicker around the switching point.

The ESP32's ADC is not perfectly linear; that is fine for a light gauge.

---

## 6. Languages, translating or restyling the phone page

The whole web page is the text block `INDEX_HTML` (a raw string) in `light_os.ino`. It is plain HTML/CSS/JavaScript with no external files or libraries.

**It is bilingual (English and Persian).** All texts live in the `TR` object in the page's script: `TR.en` and `TR.fa`, with identical keys.
Static labels in the HTML carry a `data-i="key"` attribute and are filled by `applyLang()`; dynamic texts (LED state, buttons, weather line, footer) are looked up with `t('key')` in `render()`.
The language button (`toggleLang()`) switches and remembers the choice in the browser (`localStorage`); on the first visit the page picks Persian when the browser language starts with `fa`, otherwise English. Persian switches the page to `dir="rtl"`.

**To add another language** (for example Arabic or German):
1. Copy the `en:{...}` block inside `TR`, rename it (e.g. `de`) and translate every value (keep the keys; `pages` must have 8 items).
2. Extend the language selection: in `toggleLang()` cycle through your list of languages instead of switching between two, and in `applyLang()` set `dir="rtl"` only for right-to-left languages.
3. Optionally add localized city names to the `CITIES` array (`[fa name, en name, lat, lon]`).

The texts shown **on the round display** are English only (the built-in font has no Persian glyphs).
The JSON API is language independent ([web-api.md](web-api.md)).
