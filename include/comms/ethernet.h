#pragma once
#include <Arduino.h>

bool ethernet_connected();
String get_eth_ip();
bool setup_ethernet();
void loop_ethernet();
