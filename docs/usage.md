# User guide

This guide explains every screen, every knob action, the alarm/timer, idle mode and the phone control page.

Contents: [Knob](#1-the-knob) - [Pages](#2-the-8-pages) - [LED logic](#3-led-logic-and-beeps) - [Alarm and timer](#4-alarm-and-timer)
- [Idle mode](#5-idle-mode-auto-cycle) - [Phone control](#6-phone-control-page) - [What is saved](#7-what-is-saved-across-power-loss)

---

## 1. The knob

| Action | What happens |
|---|---|
| Rotate | Next / previous page (wraps around: page 7 -> 0) |
| Short press | Back to the dashboard. **On the Alarm page** it starts / advances editing (see section 4) |
| Long press (about 0.7 s) | Back to the dashboard, cancels any editing, and stops a ringing alarm/timer |
| Any press while an alarm/timer is ringing | Stops it |
| Any action while in idle mode | Wakes the screen and returns to the dashboard (the action itself is consumed) |

Each page change plays a small ripple animation. The 8 dots at the bottom show which page you are on.

---

## 2. The 8 pages

The ring around the edge is a 270-degree gauge made of 54 segments with a violet -> cyan -> yellow -> orange gradient.
Its meaning depends on the page. The background colour slowly follows the ambient light (dark blue in the dark, warm tone in bright light).

### Page 0 - Dashboard
- **Ring:** light level, 0-100 %.
- **Icon:** a moon with twinkling stars while the LED is ON (night), an animated sun otherwise (its rays get longer in stronger light).
- **Big number:** light level in %.
- **Status text:** `DARK` (below the dark threshold), `LOW LIGHT` (between the thresholds), `BRIGHT` (above the bright threshold).
- **Bar graph:** the last ~23 seconds of light level.
- **`ADC nnnn`:** the smoothed raw sensor value - useful for calibration.
- **Pill:** `LED ON` (pulsing green dot) / `LED OFF`.

### Page 1 - Digital clock
- **Ring:** seconds of the current minute (sweeps smoothly).
- Weekday, time `HH:MM` (24-hour), **Jalali date** as `YYYY/MM/DD`, the Jalali month name, and the Gregorian date.
- Before the time is received it shows `SYNCING` and either `SETTING CLOCK` (Wi-Fi is up) or `NO WIFI YET`.

### Page 2 - Analog clock
- Classic dial with 12 ticks, hour/minute hands and an accent-coloured second hand; the ring again shows the seconds.

### Page 3 - Temperature and humidity (indoor)
- **Ring:** relative humidity.
- Temperature in degrees C (colour goes from violet (cold) to orange (hot)), humidity in %, and a comfort label:
  `TOO COLD` (< 18 C), `TOO HOT` (> 28 C), `TOO DRY` (< 30 %), `TOO HUMID` (> 65 %), otherwise `COMFORTABLE`.
- `NO DATA / CHECK SENSOR` appears when the DHT11 has not answered for 20 seconds.

### Page 4 - Weather (outdoor)
- Needs Wi-Fi and reachability of `api.open-meteo.com`. Shows `LOADING...` until the first download, `NO WIFI` without a connection.
- Animated icon (clear, mostly clear, part cloudy, overcast, fog, drizzle/rain/showers, snow, thunderstorm), city name, temperature, condition text, `humidity % | wind km/h`.
- **Ring:** outdoor temperature on a -10 .. +40 C scale.
- Updates every 15 minutes (and retries every 30 seconds while there is no data). Change the city from the phone page.

### Page 5 - Alarm and timer
- Top: alarm time and `ON`/`OFF`. Bottom: timer value (the set minutes, or the remaining time while running, in green).
- **Ring:** remaining fraction of the timer while it runs.
- The hint line at the bottom tells you what the knob does right now (see section 4).

### Page 6 - Stats and network
- Top: your device **IP address** (type it in your phone browser).
- `MAX`, `MIN`, `AVG` light level since boot, `LED ON` total time, `UPTIME`.

### Page 7 - Warp screensaver
- A star field flying toward you with the current time in the centre.

---

## 3. LED logic and beeps

The LED can be in three modes (choose on the phone page):

| Mode | Behaviour |
|---|---|
| **Auto** (default) | Turns ON after the smoothed reading stays **below the dark threshold** for 1.4 s. Turns OFF after it stays **above the bright threshold** for 1.4 s. Between the two thresholds nothing changes (hysteresis) |
| Always on | LED on |
| Always off | LED off |

Defaults: dark threshold **600**, bright threshold **720** (raw ADC units, 0-4095). The bright threshold is always kept at least 20 above the dark one.

Beeps: **2 beeps** when the LED turns on, **1 beep** when it turns off. These can be muted from the phone page
(the mute switch only affects these LED beeps - never the alarm or timer).

> Tip: keep the LED from shining directly on the LDR. If the LED lights the sensor, the light level rises, the LED turns off, it gets dark again... and the system oscillates. Shade the LDR or place it away from the LED.

---

## 4. Alarm and timer

### Editing on the device
Go to page 5 (rotate the knob). Then:

| Step | Knob | Display hint | Editing |
|---|---|---|---|
| start | short press | `SET HOUR` | rotate: alarm hour 0-23 |
| next | short press | `SET MINUTE` | rotate: alarm minute 0-59 |
| next | short press | `ALARM ON / OFF` | rotate (any direction): toggle the alarm |
| next | short press | `TIMER MINUTES` | rotate: timer length 1-99 min |
| next | short press | `PRESS TO START` / `PRESS TO STOP` | short press: start or stop the timer and leave editing |

A long press at any moment leaves editing and goes to the dashboard. The field being edited is underlined.

### What happens when they trigger
- **Alarm:** rings when the clock reaches the set `HH:MM` and the alarm is ON (once per day). Requires the time to be synced.
- **Timer:** rings when the countdown reaches zero.
- While ringing: a flashing red circle with `ALARM` or `TIME UP`, and three beeps repeated about every second, for up to 60 seconds.
- Stop it with **any knob press** or the **Dismiss** button on the phone page.
- A ringing alarm/timer wakes the display from idle mode.

The timer and alarm can also be set from the phone page.

---

## 5. Idle mode (auto-cycle)

If the light level has not changed noticeably (less than 90 ADC units from a slowly-moving reference), the LED did not change,
and nobody touched the knob or the phone page for **1 minute**, the display starts cycling automatically every 9 seconds through:

`Clock -> Indoor climate -> Weather -> Analog clock -> Stats -> Warp`

Wake-up (return to the dashboard) happens on: a sudden light change, an LED state change, any knob action,
an alarm/timer ring, or choosing a page on the phone. You can disable auto-cycling from the phone page.

---

## 6. Phone control page

Open `http://lightos.local` or `http://<device IP>` while your phone is on the same Wi-Fi. The page refreshes itself about every 1.5 seconds; the dot next to the title is green while it is connected.

| Card | What you can do |
|---|---|
| **Live status** | Light gauge and ADC value, time, Jalali and Gregorian dates, room temperature and humidity, Wi-Fi signal strength, LED state |
| **LED control** | Auto / Always on / Always off |
| **Display page** | Jump to any of the 8 pages |
| **Alarm and timer** | Alarm time and enable switch, timer minutes and start/stop, remaining time, `Test alarm`, `Test beep` |
| **Weather** | Pick a city from a list of Iranian cities or enter custom latitude/longitude; shows the current weather |
| **Settings** | Dark and bright thresholds (sliders), idle auto-cycle on/off, LED beeps on/off |
| **Footer** | Uptime and light statistics |
| **Banner** | Appears while an alarm/timer rings with a **Dismiss** button |

The page is **bilingual**: use the language button at the top (`English` / `فارسی`). The choice is remembered in your browser; the first time, the page picks Persian if your phone's language is Persian and English otherwise. Persian uses a right-to-left layout. See [configuration.md](configuration.md) to add another language.

---

## 7. What is saved across power loss

| Saved in flash | Not saved |
|---|---|
| Dark and bright thresholds | Current page |
| LED mode | Running timer state |
| Auto-cycle on/off | Light statistics (max/min/avg, LED-on time) |
| LED beeps mute | Weather data |
| Alarm time and on/off | The clock (it is re-fetched from the internet at every start) |
| Timer length | |
| Weather city and coordinates | |

Changes are written about 1.5 seconds after the last change (to protect the flash from too many writes).
