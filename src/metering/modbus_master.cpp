/**
 * @file modbus_master.cpp
 * @brief Modbus RTU master on RS485_1 — polls the Sol-Ark LV inverter.
 *
 * The decoded register cache lives in the Modbus_SolArkLV instance; the SunSpec
 * mapper, MQTT publisher, and SD logger all read from it.
 */

#ifdef ENABLE_MODBUS_MASTER

#include <SoftwareSerial.h>
#include <core/config.h>
#include <core/console.h>
#include <core/data_model.h>
#include <core/debug.h>
#include <core/pins.h>
#include <metering/modbus_master.h>

// ModbusMaster success/failure constants if not defined
#ifndef ku8MBSuccess
#define ku8MBSuccess 0x00
#define ku8MBIllegalFunction 0x01
#define ku8MBIllegalDataAddress 0x02
#define ku8MBIllegalDataValue 0x03
#define ku8MBSlaveDeviceFailure 0x04
#define ku8MBTimeout 0xE0
#define ku8MBInvalidCRC 0xE1
#define ku8MBInvalidSlaveID 0xE2
#endif

// ==================== Serial Interface Setup ====================
// RS485_1 carries the Sol-Ark link. RS485_2 is unused by the TCP gateway but is
// wired for a future SunSpec Modbus RTU server.
static SoftwareSerial _modbus1(RS485_1_RX, RS485_1_TX); // HW-519 module

// The Sol-Ark Low Voltage inverter
Modbus_SolArkLV solark;

static unsigned long lastSolArkMillis = 0;
static unsigned long solark_success_count = 0;

unsigned long get_solark_success_count() {
    return solark_success_count;
}

// Bind the Sol-Ark instance to its node address on the RS485_1 UART.
static void setup_solark() {
    Serial.printf("SETUP: MODBUS: SolArk #1: address:%d\n", SOLARK_ADDR);
    solark.begin(SOLARK_ADDR, _modbus1);
}

// Bring up the RS485_1 UART and every device attached to it.
void setup_modbus_master() {
    gpio_reset_pin(RS485_1_RX);
    gpio_reset_pin(RS485_1_TX);

    _modbus1.begin(SOLARK_BAUD_RATE);

    setup_solark();
}

#ifdef ENABLE_DEBUG
// Human-readable dumps of the decoded register cache. Debug builds only.
static void printBatteryStatus() {
    Serial.println("BATTERY STATUS:");
    Serial.printf("  Power:       %.1f W\n", solark.getBatteryPower());
    Serial.printf("  Current:     %.2f A\n", solark.getBatteryCurrent());
    Serial.printf("  Voltage:     %.2f V\n", solark.getBatteryVoltage());
    Serial.printf("  SOC:         %.0f%%\n", solark.getBatterySOC());
    Serial.printf("  Temperature: %.1f°C (%.1f°F)\n", 
                  solark.getBatteryTemperature(),
                  solark.getBatteryTemperatureF());
   Serial.printf("  Capacity:    %.1f Ah\n", solark.getBatteryCapacity());
   Serial.printf("  BMS SOC:     %.1f%%\n", solark.getBMSRealTimeSOC());
   Serial.printf("  BMS Warning: 0x%04X\n", solark.getBMSWarning());
   Serial.printf("  BMS Fault:   0x%04X\n", solark.getBMSFault());
   
   // Show charging/discharging status
   Serial.print("  Status:      ");
   if (solark.isBatteryCharging()) {
     Serial.println("CHARGING");
   } else if (solark.isBatteryDischarging()) {
     Serial.println("DISCHARGING");
   } else {
     Serial.println("IDLE");
   }
 }
 
static void printGridStatus() {
    Serial.println("GRID STATUS:");
    Serial.printf("  Power:       %.1f W\n", solark.getGridPower());
    Serial.printf("  Voltage:     %.1f V\n", solark.getGridVoltage());
    Serial.printf("  Current L1:  %.2f A\n", solark.getGridCurrentL1());
    Serial.printf("  Current L2:  %.2f A\n", solark.getGridCurrentL2());
    Serial.printf("  Grid CT Current L1:  %.2f A\n", solark.getGridCurrentL1());
    Serial.printf("  Grid CT Current L2:  %.2f A\n", solark.getGridCurrentL2());
    Serial.printf("  Frequency:   %.2f Hz\n", solark.getGridFrequency());
    
    // Show grid connection status
    Serial.print("  Connection:  ");
    if (solark.isGridConnected()) {
      Serial.println("CONNECTED");
      
      // Show buying/selling status
      Serial.print("  Flow:        ");
      if (solark.isSellingToGrid()) {
        Serial.println("SELLING TO GRID");
      } else if (solark.isBuyingFromGrid()) {
        Serial.println("BUYING FROM GRID");
      } else {
        Serial.println("NO POWER FLOW");
      }
    } else {
      Serial.println("DISCONNECTED");
    }
  }
  
 static void printPVStatus() {
    Serial.println("SOLAR PV STATUS:");
    Serial.printf("  PV1 Power:   %.1f W\n", solark.getPV1Power());
    Serial.printf("  PV2 Power:   %.1f W\n", solark.getPV2Power());
    Serial.printf("  Total Power: %.1f W\n", solark.getPV1Power() + solark.getPV2Power());
    Serial.printf("  Total Power: %.3f kW\n", solark.getPVPowerTotal());
  }
  
 static void printLoadStatus() {
    Serial.println("LOAD STATUS:");
    Serial.printf("  Load L1:     %.1f W\n", solark.getLoadPowerL1());
    Serial.printf("  Load L2:     %.1f W\n", solark.getLoadPowerL2());
    Serial.printf("  Total Load:  %.1f W\n", solark.getLoadPowerTotal());
    Serial.printf("  Smart Load:  %.1f W\n", solark.getSmartLoadPower());
    Serial.printf("  Frequency:   %.2f Hz\n", solark.getLoadFrequency());
  }
  
 static void printEnergyMeters() {
    Serial.println("ENERGY METERS (kWh):");
    Serial.printf("  Battery Charge:    %.1f kWh\n", solark.getBatteryChargeEnergy());
    Serial.printf("  Battery Discharge: %.1f kWh\n", solark.getBatteryDischargeEnergy());
    Serial.printf("  Grid Buy:          %.1f kWh\n", solark.getGridBuyEnergy());
    Serial.printf("  Grid Sell:         %.1f kWh\n", solark.getGridSellEnergy());
    Serial.printf("  Load:              %.1f kWh\n", solark.getLoadEnergy());
    Serial.printf("  PV Generation:     %.1f kWh\n", solark.getPVEnergy());
  }

 static void printInverterDetails() {
    Serial.println("INVERTER DETAILS:");
    Serial.printf("  Comm Version: %u\n", solark.getCommVersion());
    
    char serial_str[33]; // Max 32 chars for serial + null terminator
    char* p_serial = serial_str;
    for (int i = 0; i < 5; ++i) {
        uint16_t sn_part = solark.getSerialNumberPart(i);
        if (sn_part == 0) break;
        char char1 = (sn_part >> 8) & 0xFF;
        char char2 = sn_part & 0xFF;
        if (char1 != 0 && p_serial < serial_str + sizeof(serial_str) -1) *p_serial++ = char1; else if (char1 == 0) break;
        if (char2 != 0 && p_serial < serial_str + sizeof(serial_str) -1) *p_serial++ = char2; else if (char2 == 0) break;
    }
    *p_serial = '\0';
    Serial.printf("  Serial No:   %s\n", serial_str);

    Serial.printf("  Grid Type:   %u (0:Single, 1:Split, 2:Three-Phase Wye)\n", solark.getGridType());
    Serial.printf("  Inv Status:  %u (1:Self-test, 2:Normal, 3:Alarm, 4:Fault)\n", solark.getInverterStatus());
    Serial.printf("  DCDC Temp:   %.1f°C\n", solark.getDCDCTemp());
    Serial.printf("  IGBT Temp:   %.1f°C\n", solark.getIGBTTemp());
  }
#endif // ENABLE_DEBUG

// Poll the inverter on its own rate timer and refresh the register cache.
// Full status is dumped to Serial only in debug builds; the SunSpec register
// map is updated regardless by modbus_server's loop.
static void loop_solark() {
    if (millis() - lastSolArkMillis <= (unsigned long)SolArkPoll_rate) return;
    lastSolArkMillis = millis();

    DBUGLN("Poll SolArk inverter");
    uint8_t result = solark.poll();
    if (result != ku8MBSuccess) {
        Serial.println("Error polling SolArk inverter");
        return;
    }

    solark_success_count++;

#ifdef ENABLE_DEBUG
    printInverterDetails();
    printBatteryStatus();
    printGridStatus();
    printPVStatus();
    printLoadStatus();
    printEnergyMeters();
    Serial.println("-------------------------------------");
#endif
}

// Main polling entry point for the RS-485 master side.
void loop_modbus_master() {
    loop_solark();
}

#endif // ENABLE_MODBUS_MASTER
