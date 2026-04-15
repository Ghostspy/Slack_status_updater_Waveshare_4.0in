# Slack Status Updater — Waveshare 4.0in

A physical Slack status controller built on an ESP32 D1 Mini. Press one of six buttons to instantly update your Slack status and presence without touching your computer — useful for "In a meeting", "Lunch", "Walking the dog", and similar states you switch between throughout the day.

---

## Hardware

| Component | Link |
|-----------|------|
| ESP32 D1 Mini style dev board | [AliExpress](http://s.click.aliexpress.com/e/C6ds4my)* |
| 6× momentary pushbuttons | — |
| Waveshare 4.0in enclosure (optional) | — |

\* Affiliate link

**GPIO pin assignments:**

| Button | GPIO |
|--------|------|
| 1 | 13 |
| 2 | 25 |
| 3 | 33 |
| 4 | 18 |
| 5 | 17 |
| 6 | 16 |

All button pins are configured as `INPUT_PULLUP` — wire each button between its GPIO pin and GND.

---

## Dependencies

Install the following libraries before compiling:

| Library | Install via |
|---------|-------------|
| [ArduinoSlack](https://github.com/witnessmenow/arduino-slack-api) | GitHub (manual install) |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | Arduino Library Manager — search "Arduino Json" |

`WiFi` and `WiFiClientSecure` are included with the ESP32 Arduino core.

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
#define WIFI_SSID     "your-network-name"
#define WIFI_PASSWORD "your-network-password"
#define SLACK_ACCESS_TOKEN "xoxp-your-token-here"
```

`credentials.h` is gitignored and should never be committed.

### 3. Flash the ESP32

1. Open `Slack_status_updater_Waveshare_4.0in.ino` in **Arduino IDE**.
2. Under **Tools → Board**, select **ESP32 D1 Mini** (or your specific ESP32 variant).
3. Select the correct COM port under **Tools → Port**.
4. Click **Upload**.

To monitor output, open the Serial Monitor at **115200 baud**.

---

## Button → Status Reference

| Button | GPIO | Status Text | Emoji | Presence |
|--------|------|-------------|-------|----------|
| 1 | 13 | Walking the dog | `:dog2:` | Away |
| 2 | 25 | Lunch | `:hamburger:` | Away |
| 3 | 33 | In a meeting | `:calendar:` | Auto |
| 4 | 18 | Available to chat | `:speech_balloon:` | Auto |
| 5 | 17 | Lurking | `:crystal_ball:` | Away |
| 6 | 16 | Offline | `:no_entry:` | Away |

---

## Customization

Status mappings are in the `switch` block inside `loop()` in the `.ino` file. Each `case` corresponds to a button (0-indexed). To change a status, update the text, emoji, and presence for that case:

```cpp
case 0:
    slack.setPresence(SLACK_PRESENCE_AWAY);
    profile = slack.setCustomStatus("On a walk", ":walking:");
    break;
```

To add an expiration time, pass a Unix timestamp as a third argument to `setCustomStatus`:

```cpp
profile = slack.setCustomStatus("Lunch", ":hamburger:", 1716300000);
```

---

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).

---

*Written by [Brian Lough](https://github.com/witnessmenow) — [YouTube](https://www.youtube.com/brianlough) · [Tindie](https://www.tindie.com/stores/brianlough/)*
