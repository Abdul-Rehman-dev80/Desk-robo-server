# Digilog desk robot (ESP32-S3-CAM + OLED + 2 servos)

Board for the desk robot: ESP32-S3-CAM N16R8, 0.96" SSD1306 OLED (128x64, I2C 0x3C),
INMP441 mic, MAX98357A amp, two SG90 servos (yaw + pitch).

| Part | Pins |
|---|---|
| OLED SDA / SCL | GPIO41 / GPIO42 |
| Mic INMP441 WS / SCK / SD | GPIO1 / GPIO2 / GPIO14 (L/R to GND) |
| Amp MAX98357A LRC / BCLK / DIN | GPIO21 / GPIO40 / GPIO39 |
| Yaw servo | GPIO47 |
| Pitch servo | GPIO45 |
| Camera | on-board, see `config.h` |

Servos need their own 5 V supply (shared ground), not the ESP32's 3.3 V rail.
The servos only move inside the travel limits in `config.h`; narrow them if a bracket binds.

Voice tools for the AI: `self.base.{set_angle,turn_left,turn_right,center}` and
`self.head.{set_angle,look_up,look_down,center}`. The camera photo tool is added by Xiaozhi itself.

Build: `python scripts/build.py digilog-s3cam-oled` (from the `xiaozhi/` folder, with ESP-IDF set up).

## Getting the firmware without installing anything

Every build on GitHub (Actions tab > latest `xiaozhi` run) attaches `merged-binary.bin`.
Flash it at address `0x0`, for example with Espressif's web flasher or:

```sh
python -m esptool --chip esp32s3 -b 460800 write_flash 0x0 merged-binary.bin
```
