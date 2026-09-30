# Libraries and board package

## Board package

| Item | Details |
|---|---|
| Package | **esp32** by Espressif Systems (Boards Manager) |
| Boards Manager URL | `https://espressif.github.io/arduino-esp32/package_esp32_index.json` |
| Board to select | **ESP32 Dev Module** |
| Tested with | The release installed from Boards Manager in September 2026 (exact version not recorded). If a newer core breaks compilation, please open an issue with the version number |

## Libraries to install (Library Manager)

Open *Tools -> Manage Libraries* (or *Sketch -> Include Library -> Manage Libraries*), search, click **Install**.
When asked "install all dependencies?", choose **Install all**.

| Library (exact name) | Author | Used for | Included headers |
|---|---|---|---|
| **Adafruit GFX Library** | Adafruit | Drawing primitives, text, the off-screen canvas (`GFXcanvas16`), fonts | `Adafruit_GFX.h`, `Fonts/FreeSans*.h` |
| **Adafruit GC9A01A** | Adafruit | Driver for the round display (tested with 1.1.1) | `Adafruit_GC9A01A.h` |
| **DHT sensor library** | Adafruit | DHT11 temperature/humidity | `DHT.h` |
| Adafruit BusIO *(dependency)* | Adafruit | Low-level SPI/I2C helpers used by the display library | - |
| Adafruit Unified Sensor *(dependency)* | Adafruit | Required by the DHT library | - |

## Built into the ESP32 package (nothing to install)

| Header | Used for |
|---|---|
| `WiFi.h`, `WiFiClientSecure.h` | Wi-Fi connection, HTTPS client |
| `HTTPClient.h` | Downloading the weather |
| `WebServer.h` | The phone control web server |
| `ESPmDNS.h` | The `lightos.local` name |
| `Preferences.h` | Saving settings in flash (NVS) |
| `SPI.h` | Display communication |
| `time.h`, `sys/time.h` | Internet time (NTP) and time zones |

No JSON library is needed: the small weather reply is parsed by hand and the JSON for the phone page is built as text.

## Installing a library manually

If the Library Manager cannot download (for example, restricted internet):
1. Download the library as a ZIP from its GitHub page (Adafruit-GFX-Library, Adafruit_GC9A01A, DHT-sensor-library, Adafruit_BusIO, Adafruit_Sensor).
2. In Arduino IDE: *Sketch -> Include Library -> Add .ZIP Library...* and pick the ZIP.

## Windows: "creating temp dir ... Documents\Arduino\libraries" error

A fresh Arduino IDE sometimes cannot install libraries because the folder does not exist yet. Fix:
1. Open `C:\Users\<your name>\Documents\`.
2. Create a folder named `Arduino` (if missing) and inside it a folder named `libraries`.
3. Install the library again.

## Version notes

The firmware was tested with the **latest releases** of these libraries available in the Library Manager in September 2026
(Adafruit GFX Library, Adafruit GC9A01A - the manager offered 1.1.1 - and DHT sensor library). If a future library update breaks
compilation, install the previous version from the Library Manager's version drop-down and please open an issue.
