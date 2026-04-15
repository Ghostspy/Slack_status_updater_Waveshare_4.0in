# Touchscreen Migration Design

**Date:** 2026-04-15  
**Status:** Approved

## Overview

Migrate the Slack status updater from 6 physical GPIO buttons on an ESP32 D1 Mini to a Waveshare ESP32-S3 Touch LCD 4.3" (SKU 25948). The touchscreen replaces the physical buttons with 6 large tap targets. All Slack API logic is preserved unchanged.

---

## Hardware

| Old | New |
|-----|-----|
| ESP32 D1 Mini | Waveshare ESP32-S3 Touch LCD 4.3" |
| 6× GPIO momentary buttons | Capacitive touchscreen (GT911, I2C) |
| No display | ST7701 RGB LCD, 800×480px |

**Board settings in Arduino IDE:**
- Board: ESP32S3 Dev Module (or Waveshare variant)
- Flash: 16MB
- PSRAM: 8MB OPI (must be enabled)
- esp32 Arduino core: **v2.x** (required for LVGL v8.x compatibility)

---

## Libraries

| Library | Version | Source |
|---------|---------|--------|
| LVGL | v8.x | Arduino Library Manager |
| Arduino_GFX_Library | latest | Arduino Library Manager |
| bb_captouch | latest | Arduino Library Manager |
| ArduinoSlack | existing | GitHub |
| ArduinoJson | existing | Arduino Library Manager |

A `lv_conf.h` file must be placed in the Arduino libraries folder alongside the LVGL library. Key settings: `LV_HOR_RES_MAX 800`, `LV_VER_RES_MAX 480`, `LV_COLOR_DEPTH 16`.

---

## File Structure

```
Slack_status_updater_Waveshare_4.0in.ino   — WiFi, Slack API, main loop
display.h                                   — LVGL, display driver, touch, UI
credentials.h                               — WiFi + Slack token (gitignored)
```

---

## display.h Responsibilities

- Initialize Arduino_GFX with ST7701 RGB driver (pins from Waveshare reference)
- Initialize GT911 touch via bb_captouch
- Initialize LVGL v8.x with 800×480 draw buffer
- Register LVGL display flush callback and touch read callback
- Create UI: status bar + 3×2 button grid
- Expose three public functions:
  - `initDisplay()` — call once from `setup()`
  - `setActiveButton(int idx)` — highlight the tapped button, clear others
  - `setStatusText(const char* emoji, const char* text)` — update status bar

No WiFi or Slack knowledge in this file.

---

## Main .ino Responsibilities

- WiFi connect + `checkWiFi()` reconnect logic (unchanged from original)
- ArduinoSlack + ArduinoJson setup (unchanged)
- LVGL button event callbacks: on `LV_EVENT_CLICKED`, call `slack.setPresence()` + `slack.setCustomStatus()`, then `setActiveButton()` + `setStatusText()`
- `setup()`: call `initDisplay()`, connect WiFi, set SSL cert
- `loop()`: call `lv_timer_handler()` every 5ms, call `checkWiFi()`

The 2-second rotary switch settling delay is removed — touch is intentional, API is called immediately on tap.

---

## UI Layout

**Status bar** (top, full width):
- Left: "Current Status" label + active emoji + active status text
- Right: WiFi indicator dot (green = connected, red = disconnected)

**Button grid** (3 columns × 2 rows, fills remaining screen):

| Col 1 | Col 2 | Col 3 |
|-------|-------|-------|
| ⛔ Offline | 🔮 Lurking | 💬 Available to chat |
| 📅 In a meeting | 🍔 Lunch | 🐕 Walking the dog |

Each button shows: emoji (large), status text, presence label (AWAY/AUTO).

**Active state:** Green background + green border. All other buttons use the default dark blue background.

---

## Button → Status Mapping (new index order)

| Index | Status Text | Emoji | Presence |
|-------|-------------|-------|----------|
| 0 | Offline | `:no_entry:` | AWAY |
| 1 | Lurking | `:crystal_ball:` | AWAY |
| 2 | Available to chat | `:speech_balloon:` | AUTO |
| 3 | In a meeting | `:calendar:` | AUTO |
| 4 | Lunch | `:hamburger:` | AWAY |
| 5 | Walking the dog | `:dog2:` | AWAY |

---

## Data Flow

```
User taps button
  → LVGL LV_EVENT_CLICKED fires
    → callback in .ino identifies button index
      → slack.setPresence(AWAY | AUTO)
      → slack.setCustomStatus(text, emoji)
        → on success: setActiveButton(idx), setStatusText(emoji, text)
```

---

## Removed Behaviors

- GPIO pin setup and polling loop
- 20ms debounce delay
- 2-second rotary switch settling delay
- `changeStatus` / `whichStatus` / `lastSwitchState` tracking variables
- 15-second rate-limit cooldown (LVGL event system handles this naturally; one tap = one call)
