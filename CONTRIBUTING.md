# Contributing

Thank you for your interest in improving Light OS! Bug reports, ideas, documentation fixes, new pages and hardware variants are all welcome.

## Contact
Author: **mehrdadFreegan** (GitHub [@mehrdad2200](https://github.com/mehrdad2200)) - email mehrdad2200@gmail.com - Telegram channel tm.favme.
For questions and bugs please prefer a GitHub issue so the answer helps everyone.

## Reporting a bug
Use the **Bug report** issue template and include:
- Board model, display module, Arduino IDE and ESP32 core version
- Library versions
- Serial Monitor output (115200 baud)
- A photo of the wiring if it is a hardware problem
- **Never post your Wi-Fi name/password** (remove them from logs and screenshots)

## Suggesting a feature
Use the **Feature request** template. Ideas that fit the project: new pages, other sensors (BME280, DHT22), OTA updates, a web password,
MQTT/Home Assistant, RTC support, other weather sources, translations of the phone page.

## Pull requests
1. Fork the repository and create a branch (`feature/my-idea`).
2. Keep the code style of `light_os.ino` (clear sections, 2-space indent, short comments explaining *why*).
3. Remember the Arduino rule: custom `struct`s must be defined above the first function.
4. Do not commit `secrets.h` or any credentials.
5. Test on real hardware if you changed behaviour, and describe what you tested (board, display, Arduino core version).
6. Update the documentation (`docs/`, both `README.md` and `README.fa.md` if user-visible) and `CHANGELOG.md`.

## Documentation
Corrections to wording, pin tables and troubleshooting entries are especially appreciated - a wrong pin in the docs costs someone an evening.
