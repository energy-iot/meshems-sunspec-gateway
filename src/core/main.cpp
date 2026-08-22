/**
 * @file main.cpp
 * @brief Main application entry point for the Energy IoT SunSpec Gateway firmware
 * @author Doug Mendonca, Liam O'Brien
 * @date April 18, 2025
 *
 * Bridges a Sol-Ark LV inverter on RS-485 Modbus RTU to a SunSpec-compliant
 * Modbus TCP server, with optional MQTT telemetry, OLED console, and SD logging.
 *
 *  _____                             _____ _____ _____   _____
 * |  ___|                           |_   _|  _  |_   _| |  _  |                /  ___|
 * | |__ _ __   ___ _ __ __ _ _   _    | | | | | | | |   | | | |_ __   ___ _ __ \ `--.  ___  _   _ _ __ ___ ___
 * |  __| '_ \ / _ \ '__/ _` | | | |   | | | | | | | |   | | | | '_ \ / _ \ '_ \ `--. \/ _ \| | | | '__/ __/ _ \
 * | |__| | | |  __/ | | (_| | |_| |  _| |_\ \_/ / | |   \ \_/ / |_) |  __/ | | /\__/ / (_) | |_| | | | (_|  __/
 * \____/_| |_|\___|_|  \__, |\__, |  \___/ \___/  \_/    \___/| .__/ \___|_| |_\____/ \___/ \__,_|_|  \___\___|
 *                       __/ | __/ |                           | |
 *                      |___/ |___/                            |_|
 *
 * Copyright 2025
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ============================================================
 * Feature flags (set in platformio.ini build_flags):
 * ============================================================
 *   -DENABLE_OLED_DISPLAY      SH1106 OLED display over SPI
 *   -DENABLE_WIFI              WiFi connectivity (mutually exclusive with ENABLE_ETHERNET)
 *   -DENABLE_ETHERNET          W5500 SPI Ethernet via HR961160C (BOARD_VER_V3 only)
 *   -DENABLE_MQTT              MQTT telemetry (requires ENABLE_WIFI or ENABLE_ETHERNET)
 *   -DENABLE_MODBUS_MASTER     Modbus RTU master on RS485_1 — Sol-Ark inverter polling
 *   -DENABLE_MODBUS_TCP_SERVER SunSpec Modbus TCP server (requires a network interface
 *                              and ENABLE_MODBUS_MASTER)
 *   -DENABLE_RELAYS            Onboard SSR control
 *   -DENABLE_SD_CARD           SD card reader via SPI (BOARD_VER_V3 only)
 *   -DENABLE_SD_SOLARK_LOG     Log Sol-Ark telemetry to /solark_log.csv; interval set by
 *                              SD_SOLARK_LOG_INTERVAL_MS (default 60 s)
 *   -DENABLE_DEBUG             Extra debug Serial logging
 * ============================================================
 */

#include <Arduino.h>

// ---- Core (always included) -------------------------------------------------
#include <core/config.h>
#include <core/data_model.h>
#include <core/debug.h>
#include <core/pins.h>
#include <hw/buttons.h>

// ---- SPI (shared between display, SD card, and Ethernet; include once) ------
#if defined(ENABLE_OLED_DISPLAY) || defined(ENABLE_SD_CARD) || defined(ENABLE_ETHERNET)
  #include <SPI.h>
#endif

// ---- OLED Display -----------------------------------------------------------
#ifdef ENABLE_OLED_DISPLAY
  #include <hw/display.h>
  #include <core/console.h>
#endif

// ---- Network interface ------------------------------------------------------
// Exactly one of ENABLE_WIFI or ENABLE_ETHERNET must be defined when networking
// is needed. ENABLE_MQTT and ENABLE_MODBUS_TCP_SERVER both require one.
#if defined(ENABLE_WIFI) && defined(ENABLE_ETHERNET)
  #error "ENABLE_WIFI and ENABLE_ETHERNET are mutually exclusive. Enable only one."
#endif

#ifdef ENABLE_MQTT
  #if !defined(ENABLE_WIFI) && !defined(ENABLE_ETHERNET)
    #error "ENABLE_MQTT requires ENABLE_WIFI or ENABLE_ETHERNET."
  #endif
#endif

#ifdef ENABLE_MODBUS_TCP_SERVER
  #if !defined(ENABLE_WIFI) && !defined(ENABLE_ETHERNET)
    #error "ENABLE_MODBUS_TCP_SERVER requires ENABLE_WIFI or ENABLE_ETHERNET."
  #endif
#endif

#ifdef ENABLE_WIFI
  #include <comms/wifi.h>
#endif

#ifdef ENABLE_ETHERNET
  #include <comms/ethernet.h>
#endif

// ---- MQTT -------------------------------------------------------------------
#ifdef ENABLE_MQTT
  #include <comms/mqtt_client.h>
#endif

// ---- Modbus -----------------------------------------------------------------
// Use the specific headers rather than the aggregate modbus.h so that enabling
// only one side does not drag in declarations for the other.
#ifdef ENABLE_MODBUS_MASTER
  #include <metering/modbus_master.h>
#endif

#ifdef ENABLE_MODBUS_TCP_SERVER
  #include <metering/modbus_server.h>
#endif

// ---- Relays -----------------------------------------------------------------
#ifdef ENABLE_RELAYS
  #include <hw/relay.h>
#endif

// ---- SD Card ----------------------------------------------------------------
#ifdef ENABLE_SD_CARD
  #include <hw/sd_card.h>
#endif

// ---- SD Sol-Ark CSV logger --------------------------------------------------
#ifdef ENABLE_SD_SOLARK_LOG
  #include <hw/sd_logger.h>
#endif


// ============================================================
// setup()
// ============================================================

// One-time hardware and subsystem initialisation.
// Runs in order: Serial → device ID → display splash → WiFi/Ethernet → MQTT →
// Modbus master → SunSpec TCP server → buttons → relays → SD card → SD logger.
// Each subsystem is compiled in only when its feature flag is defined;
// see the feature-flag table in the file header above.
void setup() {
    Serial.begin(115200);
    Serial.println("INFO - Booting");

    generateDeviceID();

#ifdef ENABLE_OLED_DISPLAY
  #ifdef BOARD_VER_V3
    // V3 routes the OLED to explicit SPI pins. SPIClass::begin() is a no-op once
    // the bus is up, so MISO has to be supplied here — a driver calling
    // SPI.begin() later cannot add it. The panel itself is write-only, but the
    // W5500 and SD card share this bus and must be able to read.
    #if defined(ENABLE_ETHERNET) || defined(ENABLE_SD_CARD)
      SPI.begin(DISPLAY_CLK_PIN, ETH_MISO, DISPLAY_MOSI_PIN, DISPLAY_CS_PIN);
    #else
      SPI.begin(DISPLAY_CLK_PIN, -1, DISPLAY_MOSI_PIN, DISPLAY_CS_PIN);
    #endif
  #else
    SPI.begin();
  #endif
    setup_display();

    drawBitmap(40, 5, RICK_WIDTH, RICK_HEIGHT, rick);
    delay(1000);
    drawBitmap(0, 0, LOGO_WIDTH, LOGO_HEIGHT, eIOT_logo);
    delay(1000);
    _console.addLine("Starting Gateway");
#endif

#ifdef ENABLE_WIFI
    setup_wifi();
#endif

#ifdef ENABLE_ETHERNET
    setup_ethernet();
#endif

#ifdef ENABLE_MQTT
    setup_mqtt_client();
#endif

#ifdef ENABLE_MODBUS_MASTER
    // Modbus RTU master on RS485_1 — Sol-Ark inverter polling
    setup_modbus_master();
#endif

#ifdef ENABLE_MODBUS_TCP_SERVER
    // SunSpec-compliant Modbus TCP slave; network must already be up
    setup_modbus_server();
#endif

    setup_buttons();

#ifdef ENABLE_RELAYS
    setup_relays();
#endif

#ifdef ENABLE_SD_CARD
    setup_sd_card();
#endif

#ifdef ENABLE_SD_SOLARK_LOG
    setup_solark_csv_log();
#endif

#ifdef ENABLE_DEBUG
    Serial.println("INFO - Debug logging enabled");
#endif

#ifdef ENABLE_OLED_DISPLAY
    _console.addLine("EMS Gateway Ready!");
    _console.addLine("Sol-Ark -> SunSpec");
    _console.addLine("Model 1, 701, 713 Active");
  #if defined(ENABLE_MODBUS_TCP_SERVER) && defined(ENABLE_WIFI)
    _console.addLine("IP:" + get_wifi_ip() + ":" + String(SUNSPEC_TCP_PORT));
  #elif defined(ENABLE_MODBUS_TCP_SERVER) && defined(ENABLE_ETHERNET)
    _console.addLine("IP:" + get_eth_ip() + ":" + String(SUNSPEC_TCP_PORT));
  #endif
#endif
}


// ============================================================
// loop()
// ============================================================

// Timer variables are declared only when the guarded subsystem is active,
// avoiding unused-variable warnings in minimal builds.
#ifdef ENABLE_MQTT
static unsigned long lastMQTTMillis     = 0;  // last MQTT publish timestamp
static unsigned long lastMQTTPollMillis = 0;  // last mqttclient.loop() call timestamp
#endif

// Main firmware loop. Each subsystem is serviced in priority order:
//   1. Buttons     — always polled for responsive UI
//   2. Modbus      — RTU master on its own rate timer, then TCP server task
//   3. MQTT        — connection maintenance, then poll and publish on separate rates
//   4. Peripherals — Ethernet lease renewal, display, SD logging
void loop() {
    loop_buttons();

    // ==================== Modbus RTU master polling loop ================
#ifdef ENABLE_MODBUS_MASTER
    // Rate limiting lives inside loop_modbus_master(), which honours SolArkPoll_rate.
    loop_modbus_master();
#endif

    // ==================== SunSpec Modbus TCP server =====================
#ifdef ENABLE_MODBUS_TCP_SERVER
    loop_modbus_server();
#endif

    // ==================== MQTT polling loop =============================
#ifdef ENABLE_MQTT
    unsigned long now_ms = millis();
    bool poll_due    = (now_ms - lastMQTTPollMillis) > (unsigned long)MQTTPoll_rate;
    bool publish_due = (now_ms - lastMQTTMillis)     > (unsigned long)MQTTPublish_rootrate;

    if (poll_due || publish_due) {
        maintain_mqtt_connection();

        if (poll_due) {
            lastMQTTPollMillis = now_ms;
            poll_mqtt();
        }
        if (publish_due) {
            lastMQTTMillis = now_ms;
            // TODO: implement adaptive publish scheduling — per-subtopic cadences
            // (fast: battery/grid/load; slow: mfr/energy) instead of one rate for all.
            loop_mqtt();
        }
    }
#endif

    // ==================== Peripheral sub-loops ==========================

#ifdef ENABLE_ETHERNET
    loop_ethernet();
#endif

#ifdef ENABLE_OLED_DISPLAY
    loop_display();
#endif

#ifdef ENABLE_SD_SOLARK_LOG
    loop_solark_csv_log();
#endif
}
