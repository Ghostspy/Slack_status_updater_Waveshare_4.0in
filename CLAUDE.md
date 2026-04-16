# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino sketch for an ESP32-S3-based touchscreen Slack status updater. Six tap targets on a Waveshare ESP32-S3 Touch LCD 4.3" display trigger Slack API calls to set status text, emoji, and presence (AWAY/AUTO). This is a migration from the original ESP32 D1 Mini + 6 GPIO buttons design.

## Build & Flash

This is an Arduino project — there are no CLI build commands. Compilation and flashing are done through **Arduino IDE**:

1. Copy `credentials_example.h` → `credentials.h` and fill in WiFi credentials and Slack OAuth token
2. Open `Slack_status_updater_Waveshare_4.0in.ino` in Arduino IDE
3. Configure board settings (see below)
4. Click **Upload**

Serial monitor baud rate: **115200**

### Arduino IDE Board Settings

| Setting | Value |
|---------|-------|
| Board | **ESP32S3 Dev Module** |
| Flash Size | **16MB (128Mb)** |
| PSRAM | **OPI PSRAM** ← critical, display won't work without this |
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| Upload Speed | **921600** |
| ESP32 Arduino core | **v3.x latest** |

## Required Libraries

Install via Arduino Library Manager:
- `lvgl` — version **9.5.0** (use exactly this version or later 9.x)
- `Arduino_GFX_Library` — latest (1.6.x, designed for core v3.x)
- `bb_captouch` — latest
- `ArduinoSlack` — https://github.com/witnessmenow/arduino-slack-api
- `ArduinoJson` — latest

Built-in: `WiFi`, `WiFiClientSecure`, `Wire`

### lv_conf.h Setup

LVGL requires a `lv_conf.h` placed one level above the `lvgl/` folder in your Arduino libraries directory:

```bash
cp ~/Documents/Arduino/libraries/lvgl/lv_conf_template.h \
   ~/Documents/Arduino/libraries/lv_conf.h
```

Key settings to enable in `lv_conf.h`:
- `#if 1` (activate the config — change from `#if 0`)
- `LV_COLOR_DEPTH 16`
- `LV_FONT_MONTSERRAT_12 1` and `LV_FONT_MONTSERRAT_14 1`

Note: `LV_TICK_CUSTOM`, `LV_HOR_RES_MAX`, and `LV_VER_RES_MAX` do not exist in LVGL v9.
Tick is configured in code via `lv_tick_set_cb()` in `initDisplay()`.
Resolution is set programmatically via `lv_display_create(SCREEN_W, SCREEN_H)`.

## Architecture

Two-file split:

- **`display.h`** — All display, touch, LVGL, and UI code. Owns the 800×480 RGB panel driver (Arduino_GFX), GT911 capacitive touch (bb_captouch), LVGL v9 init, and the 3×2 button grid layout. Exposes three public functions and forward-declares `onButtonPressed(int)` which the sketch must define.
- **`Slack_status_updater_Waveshare_4.0in.ino`** — WiFi connect/reconnect, ArduinoSlack + ArduinoJson setup, `onButtonPressed()` implementation (calls Slack API), `setup()`, and `loop()`.

No GPIO polling, debounce, or settling delays — LVGL handles touch events and calls `onButtonPressed(int)` directly on tap.

### display.h Public API

```cpp
void initDisplay();                          // call once from setup()
void setActiveButton(int idx);               // 0–5 highlight; -1 clears all
void setStatusText(const char* text);        // update status bar label
void setWiFiConnected(bool connected);       // update WiFi indicator dot
```

### loop() pattern

```cpp
void loop() {
    lv_timer_handler();   // drive LVGL — call every ~5ms
    checkWiFi();          // reconnect if dropped
    delay(5);
}
```

## Button → Status Mapping

| Index | Status Text | Presence |
|-------|-------------|----------|
| 0 | Offline | AWAY |
| 1 | Lurking | AWAY |
| 2 | Available to chat | AUTO |
| 3 | In a meeting | AUTO |
| 4 | Lunch | AWAY |
| 5 | Walking the dog | AWAY |

Button labels and presence values are defined in `display.h` (`BTN_LABELS[]`, `BTN_PRESENCE[]`). To add or modify statuses, update those arrays and the `onButtonPressed()` switch in the `.ino`.

## Credentials

`credentials.h` is gitignored. It must define:
- WiFi SSID and password
- Slack OAuth token (`xoxp-...`) with scopes `users.profile:write` and `users:write`

See `credentials_example.h` for the required structure.
