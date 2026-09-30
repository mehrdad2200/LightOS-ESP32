# Firmware

| Folder | Sketch | Purpose |
|---|---|---|
| `light_os/` | `light_os.ino` | **The main firmware** - upload this |
| `examples/hardware_selftest/` | `hardware_selftest.ino` | Tests the display, LED, buzzer, LDR, DHT11 and knob one by one |
| `examples/wifi_time_test/` | `wifi_time_test.ino` | Tests only Wi-Fi and internet time (prints the time to the Serial Monitor) |

## Rules for Arduino IDE

- Every sketch must live in **its own folder with the same name as the `.ino` file** (already true here).
- Do **not** copy several sketches into one folder - Arduino would combine them and report `redefinition` errors.
- Open the sketch with *File -> Open...* and choose the `.ino` file.
- Board: **ESP32 Dev Module**. Serial Monitor: **115200 baud**.

## First-time setup of the main firmware

1. In `light_os/`, copy `secrets.h.example` to `secrets.h`.
2. Put your 2.4 GHz Wi-Fi name and password in `secrets.h`.
3. Install the libraries listed in [../docs/libraries.md](../docs/libraries.md).
4. Upload. If "Sketch too big" appears, select *Partition Scheme -> Huge APP*.

`secrets.h` is ignored by git; never commit it.
