# Slack Status Updater — Waveshare ESP32-S3 Touch LCD 4.3"

A physical Slack status controller built on a Waveshare ESP32-S3 Touch LCD 4.3". Tap one of six on-screen buttons to instantly update your Slack status and presence without touching your computer — useful for "In a meeting", "Lunch", "Walking the dog", and similar states you switch between throughout the day.

---

## Hardware

| Component | Details |
|-----------|---------|
| [Waveshare ESP32-S3 Touch LCD 4.3"](https://www.waveshare.com/esp32-s3-touch-lcd-4.3.htm) | SKU 25948 — ESP32-S3, 800×480 ST7701 RGB LCD, GT911 capacitive touch |

No physical buttons or external wiring required — the display is self-contained.

---

## Dependencies

Install the following libraries before compiling:

| Library | Version | Install via |
|---------|---------|-------------|
| [LVGL](https://github.com/lvgl/lvgl) | v8.x | Arduino Library Manager |
| [Arduino_GFX_Library](https://github.com/moononournation/Arduino_GFX) | latest | Arduino Library Manager |
| [bb_captouch](https://github.com/bitbank2/bb_captouch) | latest | Arduino Library Manager |
| [ArduinoSlack](https://github.com/witnessmenow/arduino-slack-api) | latest | GitHub (manual install) |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | latest | Arduino Library Manager — search "Arduino Json" |

`WiFi` and `WiFiClientSecure` are included with the ESP32 Arduino core.

### LVGL configuration

After installing LVGL, copy `lv_conf_template.h` from the LVGL library folder to the Arduino libraries folder (one level up from the LVGL folder), rename it to `lv_conf.h`, and set:

```c
#define LV_HOR_RES_MAX  800
#define LV_VER_RES_MAX  480
#define LV_COLOR_DEPTH   16
```

---

## Board Settings (Arduino IDE)

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module (or Waveshare variant) |
| Flash Size | 16MB |
| PSRAM | 8MB OPI (**must be enabled**) |
| esp32 Arduino core | v2.x (required for LVGL v8.x) |

---

## Setup

### 1. Create a Slack App and get an OAuth token

1. Go to [https://api.slack.com/apps](https://api.slack.com/apps) and click **Create New App** → **From scratch**.
2. Give it a name and select your workspace, then click **Create App**.
3. In the left sidebar, go to **OAuth & Permissions**.
4. Scroll to **User Token Scopes** and add:
   - `users.profile:write`
   - `users:write`
5. Scroll back to the top and click **Install to Workspace**, then **Allow**.
6. Copy the **User OAuth Token** — it starts with `xoxp-`.

### 2. Configure credentials

```bash
cp credentials_example.h credentials.h
```

Edit `credentials.h` and fill in:

```cpp
#define WIFI_SSID          "your-network-name"
#define WIFI_PASSWORD      "your-network-password"
#define SLACK_ACCESS_TOKEN "xoxp-your-token-here"
```

`credentials.h` is gitignored and should never be committed.

### 3. Flash the ESP32-S3

1. Open `Slack_status_updater_Waveshare_4.0in.ino` in **Arduino IDE**.
2. Under **Tools → Board**, select **ESP32S3 Dev Module** (or your Waveshare variant).
3. Apply the board settings listed above.
4. Select the correct COM port under **Tools → Port**.
5. Click **Upload**.

To monitor output, open the Serial Monitor at **115200 baud**.

---

## UI Layout

The screen is divided into a status bar and a 3×2 button grid:

**Status bar** (top strip): Shows the currently active status emoji and text on the left, and a WiFi indicator dot (green = connected, red = disconnected) on the right.

**Button grid:**

| Col 1 | Col 2 | Col 3 |
|-------|-------|-------|
| ⛔ Offline | 🔮 Lurking | 💬 Available to chat |
| 📅 In a meeting | 🍔 Lunch | 🐕 Walking the dog |

Tapping a button highlights it (green background and border) and immediately triggers the Slack API call. Tapping again or tapping a different button updates accordingly.

---

## Button → Status Reference

| Index | Status Text | Emoji | Presence |
|-------|-------------|-------|----------|
| 0 | Offline | `:no_entry:` | Away |
| 1 | Lurking | `:crystal_ball:` | Away |
| 2 | Available to chat | `:speech_balloon:` | Auto |
| 3 | In a meeting | `:calendar:` | Auto |
| 4 | Lunch | `:hamburger:` | Away |
| 5 | Walking the dog | `:dog2:` | Away |

---

## Customization

Status mappings live in two places in `display.h`:

- `BTN_LABELS[]` — the status text shown on each button and sent to Slack
- `BTN_PRESENCE[]` — `"AWAY"` or `"AUTO"` for each button

Slack API calls (emoji, presence) are wired up in the LVGL button event callbacks in the main `.ino` file.

---

## File Structure

```
Slack_status_updater_Waveshare_4.0in.ino  — WiFi, Slack API, main loop, LVGL callbacks
display.h                                  — LVGL display driver, touch, UI layout
credentials.h                              — WiFi + Slack token (gitignored)
credentials_example.h                      — Template for credentials.h
```

---

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).
