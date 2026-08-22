/**
 * @file config.cpp
 * @brief Runtime-tunable rates and device-ID generation.
 */

#include <core/config.h>
#include <Arduino.h>

static char device_id[MAX_DEVICE_ID_CHARS] = {0};

// Sol-Ark register bank is large; 1 s keeps the SunSpec register map fresh
// without saturating the 9600-baud RS-485 link.
int SolArkPoll_rate = 1000;

#ifdef ENABLE_DEBUG
  int MQTTPublish_rootrate = 30000;  // 30 s in debug builds for faster feedback
#else
  int MQTTPublish_rootrate = 300000; // 5 min in production
#endif

int MQTTPoll_rate = 10000;

// Derive a stable device ID from the last three octets of the WiFi station MAC.
// Deliberately avoids the full EFUSE MAC so the vendor OUI is not published.
void generateDeviceID() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(device_id, sizeof(device_id), "%s%02X%02X%02X",
           DEVICE_ID_PREFIX, mac[3], mac[4], mac[5]);
}

const char* getDeviceID() {
  return (const char*)device_id;
}
