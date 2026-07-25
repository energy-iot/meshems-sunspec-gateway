#pragma once
#include <ModbusIP_ESP8266.h>

// WiFi credentials come from include/secrets.h, which is gitignored.
// Copy secrets_example.h to secrets.h and fill in WIFI_SSID / WIFI_PW.
#include <secrets.h>

void setup_modbus_server();
void loop_modbus_server();
