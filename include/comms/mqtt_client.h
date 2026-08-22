/**
 * @file mqtt_client.h
 * @brief MQTT telemetry publisher for Sol-Ark inverter data.
 *
 * Topic layout mirrors the OpenAMI convention used by the metering firmware:
 *   <MQTT_TOPIC>/<device_id>/<subtopic>     northbound telemetry
 *   <MQTT_TOPIC>/<device_id>/cmd            southbound commands
 *
 * Transport is WiFiClient or EthernetClient depending on which network feature
 * flag is active; exactly one must be set (enforced in main.cpp).
 */

#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

#ifndef MAX_DATA_LEN
#define MAX_DATA_LEN 4096
#endif

#ifndef MQTT_TIMEOUT
#define MQTT_TIMEOUT 3
#endif

void setup_mqtt_client();
void loop_mqtt();
void poll_mqtt();
void maintain_mqtt_connection();
void mqtt_restart();
boolean mqtt_connected();
void subscriber_callback(char* topic, uint8_t* payload, unsigned int length);
