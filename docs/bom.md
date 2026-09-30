# Bill of materials

| Qty | Part | Specification / notes | Required |
|---|---|---|---|
| 1 | **ESP32 DevKit** | ESP32-WROOM-32 family, 4 MB flash, USB-serial chip (CP2102 or CH340). **Tested on a 38-pin DevKit with CP2102**; 30-pin boards should work with the same GPIO numbers | yes |
| 1 | **GC9A01 round TFT** | 1.28 inch, 240x240 pixels, SPI, 7-pin module (RST, CS, DC, SDA, SCL, GND, VCC), 3.3 V logic | yes |
| 1 | **LDR** (photoresistor) | e.g. GL5528 or similar | yes |
| 1 | **Resistor 10 kOhm** | 1/4 W, for the LDR divider | yes |
| 1 | **LED** | any colour, 3 mm or 5 mm | yes |
| 1 | **Resistor 220 ohm** | 1/4 W, LED current limit | yes |
| 1 | **Active buzzer** | 3.3-5 V; active type (built-in oscillator) | yes |
| 1 | **KY-040 rotary encoder module** | 5 pins: CLK, DT, SW, +, GND, with push button | yes |
| 1 | **DHT11 module** | 3-pin module with pull-up resistor on board | yes |
| 1 | **Breadboard(s)** | one 830-point board is enough, two make it comfortable | yes |
| ~25 | **Jumper wires** | male-male and male-female (Dupont) | yes |
| 1 | **USB cable** | must carry **data** (not a charge-only cable) | yes |

## Optional / alternative parts

| Part | Why |
|---|---|
| NPN transistor (2N2222 / BC547) + 1 kOhm resistor | Drive a louder buzzer safely (see [wiring.md](wiring.md)) |
| DHT22 (AM2302) or BME280 | More accurate temperature/humidity (needs a small firmware change) |
| Multimeter | Checking for shorts before powering up |
| Perfboard, headers, screw terminals | A permanent build instead of a breadboard |
| 3D-printed / laser-cut enclosure | The round display looks great in a round bezel |
| 5 V USB power adapter | For running without a computer |

## Notes on substitutions

- **Display:** only the GC9A01 controller is supported. Other round displays need a different driver library.
- **Encoder:** any 3.3 V-tolerant rotary encoder with a push button works. Only the pin names differ.
- **ESP32 variants:** the code targets the classic ESP32. S2/S3/C3 boards have different pin maps and ADC rules and need pin changes.
- **Buzzer:** a *passive* buzzer will not work without changing the code (it needs a tone signal).
