# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino sketch for an ESP32-S3-based touchscreen Slack status updater. Six tap targets on a Waveshare ESP32-S3 Touch LCD 4.3B display trigger Slack API calls to set status text, emoji, and presence (AWAY/AUTO). On boot, the current Slack status is read from the API and the matching button is highlighted automatically.

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
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** ← required, sketch exceeds the 1.25 MB default |
| Upload Speed | **921600** |
| ESP32 Arduino core | **v3.x latest** |

## Required Libraries

Install via Arduino Library Manager:
- `lvgl` — version **9.5.0** (use exactly this version or later 9.x)
- `Arduino_GFX_Library` — latest (1.6.x, designed for core v3.x)
- `ArduinoSlack` — https://github.com/witnessmenow/arduino-slack-api
- `ArduinoJson` — **v6.x** (the ArduinoSlack library includes v6 headers; do NOT use v7 — `JsonDocument` constructor is protected in v6, use `DynamicJsonDocument(capacity)` instead)

Built-in: `WiFi`, `WiFiClientSecure`, `Wire`

Note: `bb_captouch` is **not used**. Touch is handled via direct Wire I2C to the GT911 controller to avoid the library reinitializing the I2C bus.

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

- **`display.h`** — All display, touch, LVGL, and UI code. Owns the 800×480 RGB panel driver (Arduino_GFX), GT911 capacitive touch via direct Wire, CH422G IO expander init, LVGL v9 DIRECT-mode init, the 3×2 button grid layout, and programmatic button icons. Exposes four public functions and forward-declares `onButtonPressed(int)` which the sketch must define.
- **`Slack_status_updater_Waveshare_4.0in.ino`** — WiFi connect/reconnect, ArduinoSlack + ArduinoJson setup, `onButtonPressed()` implementation (calls Slack API), startup status read, `setup()`, and `loop()`.

No GPIO polling, debounce, or settling delays — LVGL handles touch events and calls `onButtonPressed(int)` directly on tap.

### display.h internal sections (in order)

1. Pin defines, screen dimensions, color palette constants
2. `BTN_LABELS[]`, `BTN_PRESENCE[]` arrays
3. Forward declaration of `onButtonPressed(int)`
4. Public API declarations
5. Private state variables
6. `_display_flush()` — LVGL flush callback (DIRECT mode + bounce buffer)
7. GT911 direct Wire I2C helpers (`_gt911_read`, `_gt911_write_byte`)
8. `_touchpad_read()` — LVGL input device callback
9. `_initCH422G()` — CH422G IO expander init (GT911 reset + backlight enable)
10. `_btn_event_cb()` — LVGL click event → `onButtonPressed()`
11. **Button icon drawing** — `_fill_ri()`, `_draw_icon()`, `_icon_draw_cb()` (programmatic vector icons drawn in `LV_EVENT_DRAW_MAIN`)
12. `_createUI()` — status bar + button grid layout
13. `initDisplay()` — full hardware + LVGL init sequence
14. Public API implementations (`setActiveButton`, `setStatusText`, `setWiFiConnected`)

### display.h Public API

```cpp
void initDisplay();                          // call once from setup()
void setActiveButton(int idx);               // 0–5 highlight; -1 clears all
void setStatusText(const char* text);        // update status bar label
void setWiFiConnected(bool connected);       // update WiFi indicator dot
```

### Button icons

Icons are drawn programmatically via `LV_EVENT_DRAW_MAIN` on each button (no image files). The callback fires after the button's background draws, before child objects (text label) draw — giving the correct z-order: background → icon → label. The icon center is positioned at 45% of the button height from the top. `_draw_icon(layer, cx, cy, idx)` dispatches to per-button drawing code using `lv_draw_rect` with `LV_RADIUS_CIRCLE` for circles/ovals and radius values for rounded rects.

### LVGL DIRECT mode + bounce buffer

LVGL renders directly into the hardware framebuffer (no copy). `_display_flush` defers `Cache_WriteBack_Addr` until `lv_display_flush_is_last()` is true (single writeback per render cycle). The bounce buffer (`SCREEN_W * 10` pixels) is set in `Arduino_ESP32RGBPanel` constructor to decouple DMA reads from PSRAM write timing, eliminating flicker.

### loop() pattern

```cpp
void loop() {
    lv_timer_handler();   // drive LVGL — call every ~5ms
    if (WiFi.status() != WL_CONNECTED) { setWiFiConnected(false); _connectWiFi(); }
    delay(5);
}
```

### Startup status read

`_getSlackStatus()` in the `.ino` calls `users.profile.get` directly via raw HTTPS (ArduinoSlack has no getProfile). It returns the index of the matching `BTN_LABELS[]` entry or -1 if no match. Called once in `setup()` after WiFi connects.

## Button → Status Mapping

| Index | Label | Emoji | Presence |
|-------|-------|-------|----------|
| 0 | Offline | `:no_entry:` | away |
| 1 | Lurking | `:eyes:` | away |
| 2 | Available to chat | `:speech_balloon:` | auto |
| 3 | In a meeting | `:calendar:` | auto |
| 4 | Lunch | `:fork_and_knife:` | away |
| 5 | Walking the dog | `:dog:` | away |

Labels and presence strings are in `display.h` (`BTN_LABELS[]`, `BTN_PRESENCE[]`).
Emoji codes and API presence strings are in the `.ino` (`BTN_EMOJI[]`, `BTN_PRESENCE_API[]`).
To add or modify a status, update all four arrays.

## Known Quirks

- **`_client.stop()` between Slack calls** — required. Without it the SSL handshake for the second call fails with EOF. Called between `setCustomStatus` and `setPresence` in `onButtonPressed()`.
- **`getFramebuffer()` vs `getFrameBuffer()`** — use `_gfx->getFramebuffer()` (lowercase b, plain getter on `Arduino_RGB_Display`). `_rgbpanel->getFrameBuffer()` (uppercase B) calls `esp_lcd_new_rgb_panel()` every invocation and crashes with "no free RGB panel slot".
- **ArduinoJson v6** — use `DynamicJsonDocument doc(4096)` not `JsonDocument doc`. The protected-constructor error at compile time means v6 is active.
- **SD card CS** — the SD CS pin is on CH422G EXIO4 (I2C), incompatible with the Arduino SD library which requires a real GPIO for CS.

## Credentials

`credentials.h` is gitignored. It must define:
- WiFi SSID and password
- Slack OAuth token (`xoxp-...`) with scopes `users.profile:write` and `users:write`

See `credentials_example.h` for the required structure.
