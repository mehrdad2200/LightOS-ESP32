# Troubleshooting

Find your symptom, check the likely cause, apply the fix. When asking for help, include the **Serial Monitor output (115200 baud)** and which step failed.

## Installing and compiling

| Symptom | Cause | Fix |
|---|---|---|
| `Failed to install library ... mkdir ...\Documents\Arduino\libraries: The system cannot find the file specified` | The libraries folder does not exist yet (fresh Arduino IDE on Windows) | Create `Documents\Arduino\` and inside it `libraries`, then install again |
| `error: Missing secrets.h - copy secrets.h.example ...` | You did not create your credentials file | In `firmware/light_os/` copy `secrets.h.example` to `secrets.h` and fill in Wi-Fi name/password |
| `fatal error: DHT.h: No such file` / `Adafruit_GC9A01A.h: No such file` | Library missing | Install the libraries listed in [libraries.md](libraries.md) (and their dependencies) |
| `'Frame' does not name a type` (or `'Star'`) | You moved/added code so that a function appears **before** the `struct Frame` / `struct Star` definitions | Keep the custom types above the first function - see the note in [architecture.md](architecture.md) |
| `Sketch too big` / text section exceeds available space | Default partition too small for Wi-Fi + web server + fonts | *Tools -> Partition Scheme -> Huge APP (3MB No OTA/1MB SPIFFS)* |
| Two `.ino` files give `redefinition of ...` | Two sketches are in the same folder | Each sketch needs its **own folder** whose name equals the `.ino` name. Do not put examples next to the main sketch |
| Compile errors after a library update | API change in a newer library | Install the previous library version from Library Manager and open an issue |

## Uploading

| Symptom | Cause | Fix |
|---|---|---|
| Stuck at `Connecting........` | Board not in download mode / bad cable / wrong port | Hold the **BOOT** button while it says "Connecting", release when upload starts. Use a **data** USB cable. Select the right port. Try upload speed 115200 |
| Port does not appear | Missing USB driver | Install the CP210x or CH340 driver that matches your board's USB chip |
| Upload works only with the display unplugged | Display wired to strapping pins GPIO2/GPIO15 interferes at reset | Unplug the DC (GPIO2) wire while uploading, or move DC/RST to other pins ([wiring.md](wiring.md) section 8) |
| Garbage in Serial Monitor | Wrong baud rate | Set 115200 |
| Board keeps rebooting (`Brownout detector was triggered`) | Weak USB power | Different cable/port, powered USB hub, or a proper 5 V adapter |

## Display

| Symptom | Cause | Fix |
|---|---|---|
| Completely black | No power or wrong wiring | Check 3V3/GND, then run the [hardware self-test](../firmware/examples/hardware_selftest/hardware_selftest.ino) |
| White screen | CS/DC/RST or SCL/SDA wrong or swapped | Compare by **pin name** with the table in [wiring.md](wiring.md); on this module SDA = MOSI and SCL = SCK |
| Colours wrong (red and blue swapped) | Unusual module variant | Try another colour byte order or a different module revision; open an issue with a photo |
| Picture rotated/mirrored | Different mounting | Change `tft.setRotation(0)` to 1, 2 or 3 in `setup()` |
| Text cut at the edge | Round screen shows a circle only | Layout is designed for the circle - do not modify margins without testing |
| Slow / stutters | Very long SPI wires, or Wi-Fi busy | Shorten the SPI wires; keep the phone page from polling too fast |
| Display works but freezes at "STARTING" | Canvas could not be allocated (heap too small) | Check Serial for `Canvas allocation failed!`; remove extra features/libraries |

## Light sensor and LED

| Symptom | Cause | Fix |
|---|---|---|
| ADC always 0 or always 4095 | Divider wired wrong / on the wrong pin / floating pin | Midpoint of LDR and 10 k goes to **GPIO34**; check 3V3 and GND |
| Value does not change in light | You are on GPIO4 (ADC2) with Wi-Fi active | Use GPIO34 (see wiring) |
| Brighter = lower value | LDR and resistor swapped | Put the LDR on the 3V3 side and the 10 k resistor on the GND side |
| Percent stuck near 0 or 100 | Range does not match your parts | Calibrate `ADC_MIN`/`ADC_MAX` ([configuration.md](configuration.md)) |
| LED never lights | Reversed LED, no resistor path to GND, LED mode "always off", or auto mode not dark enough | Check polarity; set LED mode to "always on" on the phone page to test |
| LED blinks on and off repeatedly | The LED lights the LDR (feedback), or thresholds are too close | Shield the LDR from the LED; widen the gap between dark and bright thresholds |
| LED flickers with mains lamps | 50/60 Hz lamp flicker | Increase the hold time (`HOLD_MS`) or widen thresholds |

## Buzzer

| Symptom | Cause | Fix |
|---|---|---|
| Only clicks / no tone | Passive buzzer | Use an **active** buzzer |
| Silent | Wrong polarity or pin | `+` to GPIO17, `-` to GND. Note: the "mute" switch silences only the LED-change beeps |
| Weak/hot, resets when beeping | Buzzer draws too much current | Drive it through a transistor ([wiring.md](wiring.md) section 6) |

## Knob (KY-040)

| Symptom | Cause | Fix |
|---|---|---|
| Direction reversed | Depends on the module | `ENC_REVERSE = true` |
| Skips pages / jumps randomly | Loose wires, `+` not on 3V3 | Check the wiring; use short wires |
| No response at all | CLK/DT/SW wires wrong | Run the hardware self-test and watch `KNOB:` and `BUTTON:` |
| Short and long press mixed up | Long press is 0.7 s | Hold about one second for a long press |

## DHT11

| Symptom | Cause | Fix |
|---|---|---|
| `NO DATA` | Pin order wrong, no power, or wrong data pin | Read the labels on the module (S/+/-); data to GPIO27; wait 3-5 s after start |
| Values jump or are off by a few degrees | DHT11 accuracy | Expected: +/-2 C and +/-5 % RH. Keep it away from the ESP32 (which warms up) |

## Wi-Fi, time, weather

| Symptom | Cause | Fix |
|---|---|---|
| Never connects | 5 GHz-only network, wrong name/password, WPA3-only router | Use a 2.4 GHz network (or dual-band with 2.4 GHz enabled), check `secrets.h` (case sensitive), set the router to WPA2 or WPA2/WPA3 mixed |
| Clock shows `SYNCING` forever | No internet, or NTP blocked | Run `wifi_time_test`; check that other devices have internet; try another NTP server in `updateNet()` |
| Time is off by whole hours | Wrong time-zone offset | Adjust `configTime(12600, ...)` ([configuration.md](configuration.md)) |
| Time is right but date/day off | (rare) time-zone/DST | Check the offset and DST setting |
| Weather stuck on `LOADING...` | `api.open-meteo.com` blocked/unreachable, DNS problem, or no internet | On a phone on the same network open `https://api.open-meteo.com/` in a browser to check reachability. Try later. If it is blocked in your country/network you need another weather source (open an issue/PR) |
| Weather for the wrong place | Default city is Tehran | Choose your city on the phone page |

## Phone page

| Symptom | Cause | Fix |
|---|---|---|
| Page does not open | Phone is on another network, guest Wi-Fi with client isolation, VPN, or mobile data taking priority | Same Wi-Fi as the ESP32; disable VPN/mobile data; try the **IP address** instead of `lightos.local` |
| `lightos.local` does not resolve | Some Android versions/browsers lack mDNS | Use the IP address (shown on Serial and on display page 6). Give the ESP32 a fixed IP in your router (DHCP reservation) so it never changes |
| Page shows the wrong language | The first visit follows the phone's language | Press the language button at the top (`English` / `فارسی`); the choice is remembered |
| Page opens but says "disconnected" (red dot) | Device rebooted or Wi-Fi dropped | Wait a few seconds; check power |
| Settings revert after power loss | Power was cut within 1.5 s of the change, or flash was erased | Change again; avoid *Erase All Flash* while uploading |

## Alarm and timer

| Symptom | Cause | Fix |
|---|---|---|
| Alarm does not ring | Alarm OFF, time not synced, or it already rang this minute | Make sure `ALARM ON`, the clock is synced, and set it to a future minute. `Test alarm` on the phone page checks the buzzer/overlay |
| Alarm rings but no sound | Buzzer problem | See the buzzer section |
| Screen keeps waking from idle | Light noise larger than the wake delta | Increase `WAKE_DELTA` or shade the sensor |

## Still stuck?

Open an issue and include: board model, display module, library versions, Serial Monitor log, a photo of your wiring and what you already tried.
