# Wiring

Board: ESP32-S3-CAM, N16R8 (16 MB flash, 8 MB octal PSRAM), OV2640.
Pins used by the board itself: camera 4–13 and 15–18, PSRAM 35–37, SD card 38–40, USB 19/20, UART0 43/44.

| Part | Part pin | ESP32-S3 pin |
|---|---|---|
| SSD1306 OLED | SDA / SCL | GPIO41 / GPIO42, VCC → 3V3 |
| INMP441 mic | SCK / WS / SD | GPIO47 / GPIO21 / GPIO14, VDD → 3V3, L/R → GND |
| MAX98357A amp | BCLK / LRC / DIN | GPIO47 / GPIO21 / GPIO46, VIN → 5 V rail, SD and GAIN unconnected |
| Pan servo | signal (orange) | GPIO2, red → 5 V rail |
| Tilt servo | signal (orange) | GPIO3, red → 5 V rail |
| Battery sense | divider midpoint | GPIO1 (220k to battery +, 100k to GND) |
| BOOT button | | GPIO0 (push-to-talk) |
| On-board RGB LED | | GPIO48 (status) |

The mic and the amp share BCLK and WS (one I2S port in full-duplex mode at 16 kHz).

## Power

- 2 × 18650 in series → 2S charger/BMS → switch → 5 V 3 A+ buck converter → 5 V rail.
- From the 5 V rail: ESP32 `5V` pin, servo red wires, amp VIN.
- 1000 µF across the servo supply near the servos, 100–220 µF + 0.1 µF at the amp, 100 µF at the ESP32 5V pin.
- **All grounds connected together.**
- Don't feed the `5V` pin from the rail while USB is plugged in unless the board has a diode on USB VBUS. Switch the battery off while flashing.
