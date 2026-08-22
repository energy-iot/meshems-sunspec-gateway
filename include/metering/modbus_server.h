/**
 * @file modbus_server.h
 * @brief SunSpec-compliant Modbus TCP server (slave).
 *
 * Network bring-up is NOT done here — setup_wifi() / setup_ethernet() must have
 * run first (see main.cpp). WiFi credentials live in the gitignored
 * include/secrets.h; the listen port is SUNSPEC_TCP_PORT in core/config.h.
 */

#pragma once

#if defined(ENABLE_MODBUS_TCP_SERVER) && !defined(ENABLE_MODBUS_MASTER)
  #error "ENABLE_MODBUS_TCP_SERVER requires ENABLE_MODBUS_MASTER — the SunSpec map is built from the Sol-Ark register cache."
#endif

void setup_modbus_server();
void loop_modbus_server();
