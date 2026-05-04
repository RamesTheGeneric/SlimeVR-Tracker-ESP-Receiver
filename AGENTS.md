# SlimeVR-Tracker-ESP-Receiver

PlatformIO embedded C++ project for an ESP32-S2 HID dongle that receives tracker data via ESP-Now and forwards it to a PC over USB HID.

## Commands

```
pio run                    # build
pio run -t upload          # build and flash
pio run -t monitor         # serial monitor at 115200
```

## Architecture

- `src/main.cpp` — entrypoint, singleton setup, loop polling `PacketHandling::tick()`
- `src/espnow/` — ESP-Now initialization, pairing mode, received-packet callback
- `src/packetHandling.cpp` — circular buffer of packets, periodic HID report writes
- `src/HID.{cpp,h}` — USB HID device abstraction
- `src/button.cpp` — debounced button with long-press and multi-press callbacks
- `src/led.cpp` — non-blocking LED blink patterns (error codes, pairing indicator, connection status)
- `src/logging/` — simple tag-based logger with levels
- `src/configuration.cpp` — persistent config (paired tracker count, dongle MAC)

## Board targets

- `slime_dongle_s2` — primary supported target (ESP32-S2)
- `slime_dongle_s3` — partial, needs custom variant directory + `pins_arduino.h`

New board support requires: a JSON file in `boards/`, a `variants/<name>/` directory with `pins_arduino.h`, and a matching `[env:...]` section in `platformio.ini`.

## Gotchas

- No test framework is configured. The `test/` directory is a PlatformIO stub with no tests.
- No lint, formatter, or CI. Code style is informal C++2a (gnu++2a).
- Framework is pulled from remote arduino-esp32 3.0.5 / IDF v5.1 via `platformio.ini` — do not assume a local Arduino installation.
- `.pio/` and `.cache/` are build artifacts; ignore them.
- `compile_commands.json` exists for IDE indexing but may be stale — regenerate if needed.
- Pairing: hold the dongle button ~2s to enter pairing mode, then reset trackers 3 times in succession. 5 presses clears saved tracker count.
