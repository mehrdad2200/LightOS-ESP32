# How the firmware works

This document is for people who want to understand or modify `light_os.ino`. It is one file, organised in the sections below (in file order).

## 1. Overview

```
                         +-------------------------------+
   LDR (GPIO34) -------> |                               |
   DHT11 (GPIO27) -----> |         main loop()           | -----> LED (GPIO16)
   Knob (GPIO32/33/25) ->|   (core 1, Arduino task)      | -----> Buzzer (GPIO17)
                         |                               | -----> Display (SPI)
   Phone browser <-----> |   WebServer.handleClient()    |
                         +---------------+---------------+
                                         ^
                       shared globals    |
                         +---------------+---------------+
                         |     weatherTask (core 0)      | <----> api.open-meteo.com (HTTPS)
                         +-------------------------------+
   Wi-Fi/NTP (background inside the ESP32 SDK) -> system clock
   Preferences (flash NVS) <-> Settings struct `cfg`
```

`loop()` order each iteration: `updateNet` -> `updateSensor` -> `updateDht` -> `handleInput` -> `updateIdle` -> `updateAlarm` -> `updateBeep`
-> `server.handleClient` -> save settings if dirty -> (every `FRAME_MS`) render a frame.

## 2. File sections

| Section | Content |
|---|---|
| Header / settings | Includes, constants, pin `#define`s, page numbers |
| Custom types | `Frame` (per-frame data for the renderer) and `Star`. **They must be defined above the first function** - see the note below |
| Saved settings | `Settings cfg`, `loadSettings()`, `saveSettings()`, debounced by `markDirty()` |
| Rendering setup | Off-screen canvas, ring geometry, history buffer |
| State | All global runtime variables |
| Buzzer | Non-blocking beeper (`startBeep`, `updateBeep`) |
| Colours | RGB565 helpers, `mix()`, `grad()` |
| Drawing wrappers | `fCircle`, `fLine`, `textCentered`, ... (band-aware) |
| Jalali calendar | `gregorianToJalali()` and name tables |
| Weather | Weather-code helpers, `fetchWeather()`, `weatherTask()` |
| Icons | Sun, moon, cloud/rain/snow/storm, clock hands |
| Pages | One `drawXxx()` function per page |
| Scene | `pageParams()`, `renderScene()`, `present()` |
| Logic | Sensor, LED, alarm/timer, DHT, time |
| Web server | `INDEX_HTML`, `stateJson()`, `handleSet()`, `updateNet()` |
| Input | Encoder ISR, short/long press, idle handling |
| `setup()` / `loop()` | Start-up and main loop |

> **Arduino auto-prototype pitfall:** the Arduino build system generates function prototypes automatically and inserts them before the *first* function in the file.
> If a struct used in a function signature is defined *after* that point, compilation fails with `'Frame' does not name a type`.
> That is why `Frame` and `Star` sit at the top, before `markDirty()`. Keep custom types above all functions when you edit.

## 3. Flicker-free rendering with a band canvas

Drawing directly to the display (clear, then draw) causes visible flicker. The usual fix is an off-screen frame buffer, but a full
240 x 240 x 16-bit buffer is 115,200 bytes - too large to allocate reliably on an ESP32 without external RAM (the heap is fragmented and the largest free block is smaller).

Solution used here: a **`GFXcanvas16` of 240 x 80 pixels (38,400 bytes)** and the screen is drawn in **3 bands**. For each frame:

```
for band in 0..2:
    by = band * 80                 // global y offset of this band
    renderScene(frame)             // draws the WHOLE scene, with y -= by (everything outside the band is clipped)
    tft.drawRGBBitmap(0, by, canvas, 240, 80)   // push the band to the display
```

All drawing helpers (`fCircle`, `fLine`, `textCentered`, ...) subtract the band offset, so page code is written in normal 0-239 screen coordinates.
Each visible pixel is written to the display exactly once per frame, so there is no flicker or tearing inside a band.

## 4. The ring gauge

The 270-degree ring (gap at the bottom) consists of **54 segments**. Their four corner points are pre-computed once in `setup()` (`quadPts`) and each
segment is drawn as two triangles. Lit segments use `grad(i/53)` (violet -> cyan -> yellow -> orange); unlit segments use a dim colour. The partially
filled segment is blended, and a white knob with an accent dot marks the tip.
`pageParams()` decides what the ring shows on each page (light %, seconds, humidity, temperature, timer progress).

## 5. Sensor and LED logic

- `readLdr()` averages 16 ADC samples; `adcAvg = (3*adcAvg + new)/4` smooths further (sampled every 60 ms).
- LED auto logic uses two thresholds and a hold time (`HOLD_MS`): dark for 1.4 s -> on, bright for 1.4 s -> off.
- `refAdc` is a slowly following reference; a difference larger than `WAKE_DELTA` counts as "activity" (wakes idle mode).
- Light level shown = `(adc - ADC_MIN) * 100 / (ADC_MAX - ADC_MIN)` clamped to 0-100, then animated with an exponential smoothing step per frame.

## 6. Rotary encoder

`encISR()` is attached to both CLK and DT (`CHANGE`). It builds a 4-bit history of the two signals and looks it up in a 16-entry
quadrature table; only valid transitions count, which rejects contact bounce. Four consecutive valid steps make one detent.
The ISR only changes a `volatile` counter; `handleInput()` consumes it in the main loop. The button is polled with a 30 ms debounce and
distinguishes short and long (700 ms) presses on release / after the timeout.

## 7. Non-blocking buzzer

`startBeep(n)` starts the first tone and `updateBeep()` (called every loop) toggles the pin after each 70 ms until `n` beeps are done.
No `delay()` is used, so animations never stutter and alarms can repeat while the UI stays responsive.

## 8. Time, Jalali date

Wi-Fi connects in the background (`updateNet()` retries every 15 s). Once connected, `configTime()` starts SNTP. The system clock is read
with `gettimeofday()` (microsecond precision for the smooth second hand/ring) and converted with `localtime_r()` using the time zone set by `configTime`.
`timeValid` becomes true when the epoch time is past a sanity threshold. The Gregorian -> Jalali conversion (`gregorianToJalali`) is an integer algorithm.

## 9. Weather task

`weatherTask()` is a FreeRTOS task pinned to core 0 with a 12 KB stack. HTTPS/TLS handshakes take a while (and would freeze the display if done in `loop()`),
so it runs separately: every 15 minutes (or when the location changes, or every 30 s while no data) it downloads the small JSON reply
and extracts four numbers by string search (`jsonNum`) - no JSON library. Results are stored in plain globals read by the UI.

## 10. Web server

`WebServer` (from the ESP32 core) is polled in `loop()` via `handleClient()`. The page is one `PROGMEM` raw string; the JSON state is assembled as text
in `stateJson()` (the page itself is bilingual and does its translation in the browser); `handleSet()` applies any combination of query parameters and returns the new state. The server is started after the first successful Wi-Fi connection,
together with mDNS (`lightos.local`).

## 11. Persistence

`Preferences` (namespace `lightos`) stores the `Settings` struct field by field. `markDirty()` records the time of the last change and `loop()` writes to flash
1.5 seconds after the last change, so dragging a slider does not cause many flash writes.

## 12. Idle mode

`lastActivity` is refreshed by knob use, LED changes, big light changes and phone page selection. After `IDLE_MS` without activity `updateIdle()` switches to auto-cycle
through `IDLE_ORDER` every `CYCLE_MS`. `registerActivity()` ends idle mode and returns to the dashboard.

## 13. Memory notes

- Display canvas: 38,400 bytes.
- Weather task stack: 12,288 bytes; TLS needs additional heap while the request runs.
- If you add features and see crashes or `Canvas allocation failed!`, reduce memory use (fewer bands is *not* possible without more RAM; more bands use less RAM at the cost of speed).
