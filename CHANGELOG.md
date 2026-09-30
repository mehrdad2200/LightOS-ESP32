# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-09-30

First public release.

### Added
- Round 240x240 GC9A01 dashboard with a flicker-free band-canvas renderer and a 54-segment gradient ring gauge.
- Light sensing with LDR (ADC1), automatic LED with hysteresis and hold time, manual LED override, beep patterns.
- 8 pages: dashboard, digital clock (with Jalali date), analog clock, indoor temperature/humidity (DHT11), weather (Open-Meteo), alarm and timer, stats with IP address, warp screensaver.
- Internet time via NTP (default Iran, UTC+3:30).
- Rotary encoder (KY-040) navigation with interrupt-based decoding, short/long press, on-device alarm/timer editor.
- Idle mode: after 1 minute of inactivity the display auto-cycles through the info pages; wakes on light change, LED change, knob, alarm or phone action.
- Built-in web server with a **bilingual (English/Persian) mobile control page** with a language switch, and a JSON API; `lightos.local` mDNS name.
- Settings persisted in flash (thresholds, LED mode, alarm, timer, mute, auto-cycle, weather location).
- Examples: hardware self-test and Wi-Fi/time test.
- Documentation: README (English and Persian), wiring guide, BOM, libraries, usage, configuration, web API, architecture, troubleshooting, schematic and screen illustrations.
