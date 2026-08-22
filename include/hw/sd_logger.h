#pragma once
#ifdef ENABLE_SD_SOLARK_LOG

#ifndef ENABLE_SD_CARD
  #error "ENABLE_SD_SOLARK_LOG requires ENABLE_SD_CARD."
#endif
#ifndef ENABLE_MODBUS_MASTER
  #error "ENABLE_SD_SOLARK_LOG requires ENABLE_MODBUS_MASTER."
#endif

#ifndef SD_SOLARK_LOG_INTERVAL_MS
  #define SD_SOLARK_LOG_INTERVAL_MS 60000UL
#endif

void setup_solark_csv_log();
void loop_solark_csv_log();

#endif // ENABLE_SD_SOLARK_LOG
