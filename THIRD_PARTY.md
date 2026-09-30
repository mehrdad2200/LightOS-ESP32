# Third-party software, services and notices

Light OS itself is released under the MIT License (see `LICENSE`). It depends on the following components, which are **not** included in this
repository and are installed separately. Each keeps its own license; the table lists the licenses to the best of our knowledge - please check each
project's repository for the authoritative text.

| Component | Purpose | License (check upstream) |
|---|---|---|
| Arduino-ESP32 core (Espressif) | ESP32 support, Wi-Fi, HTTP client, web server, mDNS, Preferences | LGPL-2.1 (and Apache-2.0 for parts of the ESP-IDF) |
| Adafruit GFX Library | Graphics primitives, canvas, fonts | BSD |
| Adafruit GC9A01A | Round display driver | BSD |
| Adafruit BusIO | Low-level bus helper | MIT |
| DHT sensor library (Adafruit) | DHT11 driver | MIT |
| Adafruit Unified Sensor | Sensor abstraction for the DHT library | Apache-2.0 |

## Services

- **Open-Meteo** (<https://open-meteo.com/>) provides the weather data. The data is licensed CC BY 4.0 and the free API is intended for
  non-commercial use - see <https://open-meteo.com/en/terms>. Attribution: *Weather data by Open-Meteo.com*.
  If you use this project commercially, obtain an appropriate Open-Meteo plan or use another weather source.
- **NTP servers** `pool.ntp.org`, `time.google.com`, `time.cloudflare.com` are used for the clock.

## Algorithms

- The Gregorian to Jalali (Persian) calendar conversion is a compact integer implementation of the widely used public conversion algorithm.

## Trademarks

ESP32 is a trademark of Espressif Systems. Arduino is a trademark of Arduino AG. Adafruit is a trademark of Adafruit Industries.
This project is not affiliated with or endorsed by any of them.
