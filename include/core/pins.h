/*****************************************************************************
 * @file pins.h
 * @brief Pin definitions for the EMS controller board
 *
 * This header defines all GPIO pin assignments for the system peripherals:
 * - Analog button array (using voltage divider)
 * - SPI OLED display (SH1106)
 * - RS-485 interfaces (dual channels using HW-519 modules)
 * - Relay control
 * - SD card interface (W5500 Ethernet and SD card share SPI bus; separate CS pins)
 * - Ethernet controller (W5500)
 *
 * Exactly one BOARD_VER_* macro must be defined in platformio.ini.  Every board
 * block defines the same canonical macro names so that the rest of the firmware
 * compiles unchanged across revisions.
 *
 * Author(s): Doug Mendonca, Liam O'Brien
 *****************************************************************************/

#pragma once

// Board Version 1 - 2025, hand-soldered prototype
#if defined(BOARD_VER_V1)
    // analog button array (voltage divider)
    #ifdef CONFIG_IDF_TARGET_ESP32S3
        #define ANALOG_BTN_PIN  A0 //GPIO1
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
        #define ANALOG_BTN_PIN  A0
    #else
        #define ANALOG_BTN_PIN  A7
    #endif

    // ==================== SPI DISPLAY ====================
    #define DISPLAY_RST_PIN 46  //Reset
    #define DISPLAY_DC_PIN 3    //Data clock
    #define DISPLAY_CS_PIN 9    //Chip select

    // ==================== RS485 INTERFACE ================
    // RS485_1 drives the Modbus RTU master (Sol-Ark inverter polling).
    // RS485_2 is spare — reserved for a SunSpec Modbus RTU server.
    #define RS485_1_RX  GPIO_NUM_6   // RX maps to RS485 HW-519 module's silk screen "RXD"
    #define RS485_1_TX  GPIO_NUM_7   // TX maps to RS485 HW-519 module's silk screen "TXD"

    #define RS485_2_RX  GPIO_NUM_15
    #define RS485_2_TX  GPIO_NUM_16

    // ==================== RELAY ==========================
    #define RELAY_1_PIN 38  //Pin to toggle the onboard SSR

// Board Version 2 - 2025, RS-485 module pins accidentally swapped in layout
#elif defined(BOARD_VER_V2)
    // analog button array (voltage divider)
    #ifdef CONFIG_IDF_TARGET_ESP32S3
        #define ANALOG_BTN_PIN  A0 //GPIO1
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
        #define ANALOG_BTN_PIN  A0
    #else
        #define ANALOG_BTN_PIN  A7
    #endif

    // ==================== SPI DISPLAY ====================
    #define DISPLAY_RST_PIN 46  //Reset
    #define DISPLAY_DC_PIN 3    //Data clock
    #define DISPLAY_CS_PIN 9    //Chip select

    // ==================== RS485 INTERFACE ================
    // V2 layout swapped the two HW-519 headers relative to V1.
    #define RS485_1_RX  GPIO_NUM_15
    #define RS485_1_TX  GPIO_NUM_16

    #define RS485_2_RX  GPIO_NUM_6
    #define RS485_2_TX  GPIO_NUM_7

    // ==================== RELAY ==========================
    #define RELAY_1_PIN 38  //Pin to toggle the onboard SSR

// Board Version 3 - NESL EMS Controller PCBA 865B
#elif defined(BOARD_VER_V3)
    // esp32s3 devkitc n16r8 onboard rgbw led
    #define RGBLED_DATA_PIN 48

    // analog button array (voltage divider)
    #ifdef CONFIG_IDF_TARGET_ESP32S3
        #define ANALOG_BTN_PIN  A0 //GPIO1
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
        #define ANALOG_BTN_PIN  A0
    #else
        #define ANALOG_BTN_PIN  A7
    #endif

    // ==================== SD CARD ====================
    // Verified by Liam April 23 2026
    #define SD_CS   GPIO_NUM_10
    #define SD_CLK  GPIO_NUM_12
    #define SD_MOSI GPIO_NUM_11
    #define SD_MISO GPIO_NUM_13

    // ==================== Ethernet Interface (W5500) ====================
    // Verified by Liam May 1 2026
    #define ETH_CS   GPIO_NUM_9
    #define ETH_CLK  GPIO_NUM_12
    #define ETH_MOSI GPIO_NUM_11
    #define ETH_MISO GPIO_NUM_13
    #define ETH_RST  GPIO_NUM_3

    // ==================== SPI OLED DISPLAY ====================
    // Verified by Liam April 23 2026
    #define DISPLAY_RST_PIN  GPIO_NUM_8     // RST
    #define DISPLAY_CLK_PIN  GPIO_NUM_12    // CLK
    #define DISPLAY_MOSI_PIN GPIO_NUM_11    // MOSI
    #define DISPLAY_DC_PIN   GPIO_NUM_18    // DC
    #define DISPLAY_CS_PIN   GPIO_NUM_17    // CS

    // ==================== RS485 INTERFACE ================
    // NESL EMS Controller PCB 865B pinout:
    // (first modbus, bridged to "RS485 EVC" connector)
    //   Pin 42 = HW-RXD "Modbus 1 - Master"
    //   Pin 7  = HW-TXD "Modbus 1 - Master"
    // (second modbus, Ethernet Port side)
    //   Pin 6  = HW-RXD "Modbus 2 - Client"
    //   Pin 4  = HW-TXD "Modbus 2 - Client"
    #define RS485_2_RX  GPIO_NUM_42  // Connects to HW-519 RXD
    #define RS485_2_TX  GPIO_NUM_7   // Connects to HW-519 TXD

    #define RS485_1_RX  GPIO_NUM_6   // Connects to HW-519 RXD
    #define RS485_1_TX  GPIO_NUM_4   // Connects to HW-519 TXD

    // ==================== RELAY ==========================
    #define RELAY_1_PIN 38  //Pin to toggle the onboard SSR, 5 VDC TTL

#else
    #error "No board revision defined. Set exactly one of -DBOARD_VER_V1 / -DBOARD_VER_V2 / -DBOARD_VER_V3 in platformio.ini."
#endif
