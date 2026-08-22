/**
 * @file sd_logger.cpp
 * @brief Periodic CSV log of Sol-Ark inverter telemetry to the SD card.
 *
 * Rows are only appended after the first successful Sol-Ark poll so the file
 * never opens with a run of spurious all-zero samples.
 */

#ifdef ENABLE_SD_SOLARK_LOG

#include <SD.h>
#include <TimeLib.h>
#include <hw/sd_logger.h>
#include <hw/sd_card.h>
#include <metering/modbus_master.h>
#include <metering/modbus_solark.h>
#include <core/debug.h>

#define LOG_FILE   "/solark_log.csv"
#define CSV_HEADER "timestamp, batt_soc_%, batt_power_w, grid_power_w, pv_total_kw, load_total_w\n"

static unsigned long _lastLogMillis = 0;

// Create the log file with its CSV header if this is the first boot with this card.
void setup_solark_csv_log() {
    if (!sd_card_available()) {
        DBUGLN("WARN - Sol-Ark CSV logger: SD not available");
        return;
    }
    if (!SD.exists(LOG_FILE)) {
        File f = SD.open(LOG_FILE, FILE_WRITE);
        if (f) {
            f.print(CSV_HEADER);
            f.close();
            DBUGLN("INFO - Sol-Ark CSV logger: created " LOG_FILE);
        } else {
            DBUGLN("WARN - Sol-Ark CSV logger: failed to create " LOG_FILE);
        }
    } else {
        DBUGLN("INFO - Sol-Ark CSV logger: appending to existing " LOG_FILE);
    }
}

void loop_solark_csv_log() {
    if (!sd_card_available()) return;
    if (get_solark_success_count() == 0) return;
    if (millis() - _lastLogMillis < SD_SOLARK_LOG_INTERVAL_MS) return;

    _lastLogMillis = millis();

    File f = SD.open(LOG_FILE, FILE_APPEND);
    if (!f) {
        DBUGLN("WARN - Sol-Ark CSV logger: failed to open " LOG_FILE);
        return;
    }

    char row[96];
    snprintf(row, sizeof(row), "%lu, %.0f, %.1f, %.1f, %.3f, %.1f\n",
             (unsigned long)now(),
             solark.getBatterySOC(),
             solark.getBatteryPower(),
             solark.getGridPower(),
             solark.getPVPowerTotal(),
             solark.getLoadPowerTotal());
    f.print(row);
    f.close();

    DBUGF("INFO - Sol-Ark log: SOC=%.0f%% batt=%.1fW grid=%.1fW",
          solark.getBatterySOC(), solark.getBatteryPower(), solark.getGridPower());
}

#endif // ENABLE_SD_SOLARK_LOG
