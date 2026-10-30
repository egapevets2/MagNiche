# MagNiche

Firmware project for the **Pro Micro (ATmega32U4 5V / 16MHz)** built using **PlatformIO** and the Arduino framework.

## Project Structure
- `platformio.ini`: PlatformIO build, board, and monitor configuration.
- `src/main.cpp`: Main firmware source.
- `include/`: Custom header files.
- `lib/`: Private project libraries.

## Build & Flash Commands
Using the PlatformIO CLI:
```bash
# Build firmware
pio run

# Upload to Pro Micro
pio run --target upload

# Open Serial Monitor at 115200 baud
pio device monitor
```
