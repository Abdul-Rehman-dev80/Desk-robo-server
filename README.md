# Desk Robo

An ESP32-S3-CAM desk robot that talks (English, Urdu/Hindi), recognises and follows your face, and identifies objects you show it.

- `firmware/`: ESP-IDF firmware for the robot
- `server/`: Python backend that relays the robot to Gemini
- `docs/wiring.md`: pin map and power wiring

## Firmware

Built with [ESP-IDF v5.4](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/get-started/index.html), Espressif's own framework (the Arduino core for ESP32 is built on top of it).

### Easiest: VS Code (no command line)

1. Install [VS Code](https://code.visualstudio.com/) and the **ESP-IDF** extension by Espressif.
2. In the extension's setup wizard choose **Express**, ESP-IDF version **v5.4.x**, and let it download everything (a few GB).
3. **File > Open Folder** and open the `firmware` folder of this repo.
4. In the bottom status bar:
   - set the target to **esp32s3** (choose "ESP32-S3 chip (via builtin USB-JTAG)" or "via ESP-PROG", either works for building),
   - pick your COM port (plug the board into the **TTL** USB-C port),
   - click the **gear icon** (SDK Configuration Editor), search "Desk Robo", and fill in your WiFi SSID and password, then Save,
   - click the **flame icon** (Build, Flash and Monitor).
5. The log appears in the terminal. Press `Ctrl+]` to close it.

### Command line

```sh
cd firmware
idf.py set-target esp32s3
idf.py menuconfig        # Desk Robo > WiFi SSID and password (and servo limits)
idf.py build
idf.py -p COM5 flash monitor   # use the TTL USB-C port; your COM port / /dev/ttyUSB0 will differ
```

Your WiFi details are saved in `firmware/sdkconfig`, which is git-ignored, so they never end up on GitHub.

### Phase 1 check

1. **First boot, USB only, servos unplugged.** The monitor should print `flash 16 MB, PSRAM 8 MB`, the camera sensor ID, then `got IP ...`.
2. Open `http://deskrobo.local` (or the IP from the log). You should see the live camera and two sliders.
3. **Power the 5 V rail from the battery/buck, plug the servos in**, and drag the sliders. The servos ramp to each position at the speed set in menuconfig.
4. Move both sliders end to end quickly a few times while the stream runs. The status line under the sliders shows the last reset reason. If it ever says `BROWNOUT`, the 5 V supply is sagging: check the buck rating, wire gauge, the 1000 µF capacitor and the shared ground.
5. If a servo hits the bracket at either end, narrow its limits in menuconfig (Desk Robo > Pan/Tilt limits).

## Server

```sh
cd server
python -m venv .venv && source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements-dev.txt
pytest

cp .env.example .env    # then set DEVICE_TOKEN
export $(cat .env | xargs)   # Windows PowerShell: set the variables manually
uvicorn app.asgi:app --host 0.0.0.0 --port 8000
python tools/fake_robot.py ws://localhost:8000/ws/robot
```

The robot connects to `/ws/robot` with `Authorization: Bearer <DEVICE_TOKEN>`. The protocol is described in `server/app/protocol.py`. The Gemini key is only ever stored on the server.
