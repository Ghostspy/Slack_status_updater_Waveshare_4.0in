// credentials_example.h
// Copy this file to credentials.h and fill in your values.
// Do NOT commit credentials.h to version control.

#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// WiFi settings
#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// Slack OAuth token (xoxp-...)
// To get a token:
// 1. Go to https://api.slack.com/apps and create a new app
// 2. Under "OAuth & Permissions", add the following User Token Scopes:
//    - users.profile:write
//    - users:write
// 3. Install the app to your workspace
// 4. Copy the "User OAuth Token" (starts with xoxp-)
#define SLACK_ACCESS_TOKEN "xoxp-your-token-here"

#endif // CREDENTIALS_H
