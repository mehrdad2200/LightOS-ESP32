# Web API

The device runs a small HTTP server on port **80**. The same API is used by the built-in phone page and can be used from scripts,
Home Assistant REST sensors, shortcuts, etc. Replace `lightos.local` with the device IP if `.local` names do not resolve on your network.

> There is **no authentication**. Use it only inside a trusted network and never expose it to the internet.

## Endpoints

| Method | Path | Description |
|---|---|---|
| GET | `/` | The Persian control web page (HTML) |
| GET | `/api/state` | Current state as JSON |
| GET | `/api/set?...` | Change one or more settings/actions, returns the new state as JSON |
| any | other | `404 Not found` |

## `/api/set` parameters

Any number of parameters can be combined in one request. Unknown parameters are ignored.

| Parameter | Values | Effect |
|---|---|---|
| `led` | `0` auto, `1` always on, `2` always off | LED mode (saved) |
| `page` | `0`-`7` | Show a page (leaves idle mode). 0 dashboard, 1 clock, 2 analog, 3 climate, 4 weather, 5 alarm/timer, 6 stats, 7 warp |
| `cycle` | `1` / `0` | Idle auto-cycle on/off (saved) |
| `mute` | `1` / `0` | Mute the LED-change beeps (saved) |
| `ah` | `0`-`23` | Alarm hour (saved) |
| `am` | `0`-`59` | Alarm minute (saved) |
| `aon` | `1` / `0` | Alarm enabled (saved) |
| `tmin` | `1`-`99` | Timer length in minutes (saved) |
| `trun` | `1` start / `0` stop | Start or stop the timer |
| `dark` | `50`-`3500` | Dark threshold, raw ADC (saved) |
| `bright` | `50`-`3600` | Bright threshold, raw ADC (saved). Always forced to be at least `dark + 20` |
| `lat`, `lon` | decimal degrees (`lat` -90..90, `lon` -180..180) | Weather location (saved). Send **both**; `0,0` is rejected |
| `city` | text, letters/digits/space/hyphen, max 15 chars | Name shown on the weather page (send together with `lat`/`lon`) |
| `beep` | any value | Two test beeps |
| `dismiss` | any value | Stop a ringing alarm/timer |
| `testalarm` | any value | Trigger the alarm right now |

## Examples

```bash
# read everything
curl http://lightos.local/api/state

# force the LED on
curl "http://lightos.local/api/set?led=1"

# back to automatic
curl "http://lightos.local/api/set?led=0"

# show the weather page
curl "http://lightos.local/api/set?page=4"

# alarm at 06:45, enabled
curl "http://lightos.local/api/set?ah=6&am=45&aon=1"

# 10 minute timer, start it
curl "http://lightos.local/api/set?tmin=10&trun=1"

# weather for another city
curl "http://lightos.local/api/set?lat=29.5918&lon=52.5837&city=Shiraz"

# thresholds
curl "http://lightos.local/api/set?dark=500&bright=800"
```

## State JSON

Example response of `/api/state` (formatted for readability):

```json
{
  "pct": 72,            // light level, 0-100
  "adc": 1480,          // smoothed raw ADC value
  "led": false,         // LED currently on?
  "ledMode": 0,         // 0 auto, 1 on, 2 off
  "temp": 24,           // indoor temperature, C (integer, DHT11)
  "hum": 45,            // indoor humidity, %
  "dht": true,          // DHT11 currently delivering data
  "time": "14:35:12",   // local time, "--:--:--" before sync
  "date": "1405/07/07", // Jalali date
  "gdate": "Tue 29 Sep 2026",
  "page": 0,            // page currently shown
  "cycle": true,        // idle auto-cycle enabled
  "mute": false,        // LED beeps muted
  "dark": 600, "bright": 720,
  "alarm": { "h": 7, "m": 30, "on": true,  "ring": false },
  "timer": { "set": 5, "run": false, "left": 0, "ring": false },   // left = remaining seconds
  "wx": { "ok": true, "t": 21, "h": 38, "w": 9, "txt": "PART CLOUDY", "city": "Tehran" },
  "lat": 35.6892, "lon": 51.3890, "city": "Tehran",
  "rssi": -58,          // Wi-Fi signal strength, dBm
  "up": 43520,          // uptime, seconds
  "st": { "max": 91, "min": 8, "avg": 46 }
}
```
(The real response contains no comments.)

## Notes

- All calls are `GET` and change state, so a browser prefetch could trigger them. That is acceptable for a LAN gadget, but it is another reason to keep it off the internet.
- The server handles one request at a time; do not poll faster than about once per second.
- The response is generated on the ESP32 and is small (about 600 bytes).
