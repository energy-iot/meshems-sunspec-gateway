/**
 * @file wifi.cpp
 * @brief WiFi station bring-up for the SunSpec Modbus TCP gateway.
 *
 * Credentials live in the gitignored include/secrets.h — see secrets_example.h.
 */

#ifdef ENABLE_WIFI
#include <Arduino.h>
#include <WiFiMulti.h>
#include <comms/wifi.h>
#include <secrets.h>

// Number of 1 s retries before giving up; the gateway still boots without a
// link so the RS-485 side and OLED remain usable for diagnostics.
static int CONNECT_ATTEMPTS = 6;
static WiFiMulti wifiMulti;

bool wifi_client_connected() {
  return WiFi.isConnected() && (WIFI_STA == (WiFi.getMode() & WIFI_STA));
}

String get_wifi_ip() {
  if (wifi_client_connected()) {
    return WiFi.localIP().toString();
  }
  return "Not Connected";
}

bool setup_wifi() {
  Serial.printf("wifi connecting: %s\n", WIFI_SSID);
  wifiMulti.addAP(WIFI_SSID, WIFI_PW);
  while (wifiMulti.run() != WL_CONNECTED && (CONNECT_ATTEMPTS-- > 0)) {
    delay(1000);
    Serial.printf("wifi failed to connect - retrying %s\n", WIFI_SSID);
  }
  Serial.printf("wifi: %s: %s\n", WIFI_SSID,
                wifi_client_connected() ? WiFi.localIP().toString().c_str() : "FAILED");
  return wifi_client_connected();
}

#endif // ENABLE_WIFI
