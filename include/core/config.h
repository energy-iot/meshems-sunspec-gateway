/**
 * @file config.h
 * @brief Runtime configuration constants and device identity for the SunSpec gateway.
 *
 * Values here are compile-time defaults; the poll/publish rates are mutable
 * globals defined in config.cpp so a future config server can override them.
 */

#pragma once
#include <Arduino.h>

// ==================== Device identity ====================
#define MAX_DEVICE_ID_CHARS   32
#define DEVICE_ID_PREFIX      "SunSpecGW_"

// ==================== Modbus TCP (SunSpec) server ====================
// Port 502 is the Modbus standard; 8502 avoids the privileged-port clash with
// other Modbus TCP services on the same host network.
#ifndef SUNSPEC_TCP_PORT
#define SUNSPEC_TCP_PORT      8502
#endif

// ==================== Sol-Ark Modbus RTU master ====================
#define SOLARK_ADDR           0x01    // Sol-Ark inverter Modbus node address
#define SOLARK_BAUD_RATE      9600    // Sol-Ark only supports 9600 baud

// ==================== Ethernet addressing (ENABLE_ETHERNET) ====================
// Locally-administered MAC. Override per device when several boards share a
// network segment:  -DETH_MAC="0xDE,0xAD,0xBE,0xEF,0xFE,0x01"
#ifndef ETH_MAC
#define ETH_MAC 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
#endif

// Define ENABLE_ETH_STATIC_IP to skip DHCP entirely. Each address below is a
// comma-separated octet list expanded straight into an IPAddress(a,b,c,d)
// constructor, so override them from platformio.ini like this:
//   -DENABLE_ETH_STATIC_IP
//   -DETH_STATIC_IP="192,168,1,50"
//   -DETH_STATIC_GATEWAY="192,168,1,1"
#ifndef ETH_STATIC_IP
#define ETH_STATIC_IP      192,168,1,50
#endif
#ifndef ETH_STATIC_GATEWAY
#define ETH_STATIC_GATEWAY 192,168,1,1
#endif
#ifndef ETH_STATIC_SUBNET
#define ETH_STATIC_SUBNET  255,255,255,0
#endif
// Most small installs run DNS on the gateway; override if yours does not.
#ifndef ETH_STATIC_DNS
#define ETH_STATIC_DNS     ETH_STATIC_GATEWAY
#endif

// DHCP discovery blocks setup() until it succeeds or times out. The library
// default is 60 s, which is a long stall on a headless gateway.
#ifndef ETH_DHCP_TIMEOUT_MS
#define ETH_DHCP_TIMEOUT_MS 15000UL
#endif

// How long to wait for PHY auto-negotiation before reporting link state.
// Static addressing skips the DHCP exchange, so without this wait the first
// linkStatus() query lands before the link is up and falsely reports "down".
#ifndef ETH_LINK_TIMEOUT_MS
#define ETH_LINK_TIMEOUT_MS 4000UL
#endif

// ==================== MQTT ====================
#define MQTT_TOPIC              "openami" // "openami/SunSpecGW_<id>/..."
#define MQTT_SERVER             "public.cloud.shiftr.io"  //"test.mosquitto.org"
#define MQTT_USER               "public"                  // leave empty for test.mosquitto.org
#define MQTT_PW                 "public"                  // leave empty for test.mosquitto.org

// ==================== Poll / publish rates ====================
// Mutable so they can be retuned at runtime; defaults set in config.cpp.
extern int SolArkPoll_rate;       // ms between Sol-Ark RTU poll cycles
extern int MQTTPublish_rootrate;  // ms between MQTT publish cycles
extern int MQTTPoll_rate;         // ms between mqttclient.loop() calls

void generateDeviceID();
const char* getDeviceID();
