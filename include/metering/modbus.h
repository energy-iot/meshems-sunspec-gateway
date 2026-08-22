/**
 * @file modbus.h
 * @brief Aggregate header for the Modbus subsystem.
 *
 * Prefer including the specific headers directly so that enabling one side does
 * not pull in declarations for the other; this aggregate exists for
 * translation units that genuinely need both.
 */

#pragma once

#ifdef ENABLE_MODBUS_MASTER
  #include <metering/modbus_master.h>  // Modbus RTU master (Sol-Ark polling)
  #include <metering/modbus_solark.h>  // Sol-Ark register map and decoders
#endif

#ifdef ENABLE_MODBUS_TCP_SERVER
  #include <metering/modbus_server.h>  // SunSpec Modbus TCP server (slave)
#endif

#include <metering/sunspec_models.h>   // SunSpec model register layout
