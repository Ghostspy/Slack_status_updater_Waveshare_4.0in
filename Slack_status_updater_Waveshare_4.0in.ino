#include "display.h"
#include "credentials.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoSlack.h>
#include <ArduinoJson.h>

// ets_printf() outputs directly to UART0 — no Serial.begin() needed.
extern "C" int ets_printf(const char *fmt, ...);

// Slack status emoji per button (matches BTN_LABELS order in display.h)
static const char *BTN_EMOJI[NUM_BUTTONS] = {
    ":no_entry:", ":eyes:", ":speech_balloon:",
    ":calendar:", ":fork_and_knife:", ":dog:"
};

// Slack presence API values (lowercase) per button
static const char *BTN_PRESENCE_API[NUM_BUTTONS] = {
    "away", "away", "auto", "auto", "away", "away"
};

static WiFiClientSecure _client;
static ArduinoSlack     _slack(_client, SLACK_ACCESS_TOKEN);

static void _connectWiFi() {
    ets_printf("WiFi connecting to %s...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    setWiFiConnected(false);
    while (WiFi.status() != WL_CONNECTED) {
        lv_timer_handler();
        delay(250);
    }
    _client.setInsecure();  // skip cert verification — acceptable for local IoT device
    setWiFiConnected(true);
    ets_printf("WiFi connected\n");
}

// Fetch the user's current Slack status and return the matching button index.
// Returns -1 if the current status doesn't match any button label.
// ArduinoSlack has no getProfile(), so we call users.profile.get directly.
static int _getSlackStatus() {
    _client.stop();
    if (!_client.connect(SLACK_HOST, 443)) {
        ets_printf("getStatus: connect failed\n");
        return -1;
    }
    _client.printf(
        "GET /api/users.profile.get HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Authorization: Bearer %s\r\n"
        "Connection: close\r\n\r\n",
        SLACK_HOST, SLACK_ACCESS_TOKEN);

    // Skip HTTP headers
    while (_client.connected()) {
        String line = _client.readStringUntil('\n');
        if (line == "\r") break;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, _client);
    _client.stop();

    if (err || !doc["ok"].as<bool>()) {
        ets_printf("getStatus: parse err or ok=false\n");
        return -1;
    }

    const char *statusText = doc["profile"]["status_text"];
    if (!statusText || statusText[0] == '\0') return -1;

    ets_printf("Current Slack status: \"%s\"\n", statusText);
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (strcmp(statusText, BTN_LABELS[i]) == 0) return i;
    }
    return -1;  // status set outside this device
}

void onButtonPressed(int idx) {
    ets_printf(">>> Button %d: %s [%s]\n", idx, BTN_LABELS[idx], BTN_PRESENCE_API[idx]);

    // Update UI before the blocking HTTPS call
    setActiveButton(idx);
    setStatusText(BTN_LABELS[idx]);
    lv_timer_handler();

    // Set Slack status text + emoji
    SlackProfile profile = _slack.setCustomStatus(
        BTN_LABELS[idx],
        BTN_EMOJI[idx],
        0  // no expiration
    );
    if (!profile.error) {
        ets_printf("Status OK\n");
    } else {
        ets_printf("Status FAILED\n");
    }

    // Close the previous HTTPS connection before opening a new one.
    // Without this, the SSL handshake for the second call fails with EOF.
    _client.stop();

    // Set Slack presence
    if (_slack.setPresence(BTN_PRESENCE_API[idx])) {
        ets_printf("Presence OK: %s\n", BTN_PRESENCE_API[idx]);
    } else {
        ets_printf("Presence FAILED\n");
    }
}

void setup() {
    initDisplay();
    _connectWiFi();

    // Highlight whichever button matches the current Slack status
    int btn = _getSlackStatus();
    if (btn >= 0) {
        setActiveButton(btn);
        setStatusText(BTN_LABELS[btn]);
    }
    lv_timer_handler();
}

void loop() {
    lv_timer_handler();
    if (WiFi.status() != WL_CONNECTED) {
        setWiFiConnected(false);
        _connectWiFi();
    }
    delay(5);
}
