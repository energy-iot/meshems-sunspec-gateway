/*
  ModbusTCP SunSpec Server (slave device)

  This implementation provides a SunSpec-compliant Modbus TCP server using the modbus-esp8266 library.
  It exposes Sol-Ark inverter data in a standardized SunSpec format over TCP/IP.
  
  Created: 2025-05-14
  Based on previous RTU implementation by: C. M. Bulliner, doug mendonca
  
  SunSpec compliance added: 2025-04-23
  TCP/IP implementation added: 2025-05-14
*/
#include <modbus.h>
#include <WiFi.h>
#include <pins.h>
#include <data_model.h>
#include <sunspec_models.h>
#include <modbus_server.h>

// Create ModbusIP instance
ModbusIP mb;

// Max WiFi association attempts (1s apart) before giving up and booting without
// a network. 
int CONNECT_ATTEMPTS = 6;

// Flag to track if SunSpec registers have been updated at least once
bool sunspec_initialized = false;

void setup_modbus_server() {
  // Initialize SunSpec models in the Modbus register map
  setup_sunspec_models();
  
  Serial.println("INFO - Modbus Client: SunSpec models initialized");
  Serial.println("INFO - Modbus Client: SunSpec Common (1) and Inverter (701) models available");

  // Start WiFi connection. Bounded by an attempt count rather than blocking the
  // boot forever - the RTU client and display still work without a network, so a
  // failure here must not wedge setup().
  int connect_attempts = CONNECT_ATTEMPTS;
  Serial.printf("wifi connecting: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PW);
  while (WiFi.status() != WL_CONNECTED && (connect_attempts-- > 0)) {
    delay(1000);
    Serial.printf("wifi failed to connect - retrying %s\n", WIFI_SSID);
  }
  Serial.printf("wifi: %s: %s\n", WIFI_SSID,
                WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "FAILED");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WARN - WiFi: continuing without network; Modbus TCP will be unreachable");
  }

  // Configure Modbus registers
  // For ModbusIP, we need to add each register individually
  // Add coils
  for (uint16_t i = 0; i < MODBUS_NUM_COILS; i++) {
    mb.addCoil(i, coils[i]);
  }
  
  // Add discrete inputs
  for (uint16_t i = 0; i < MODBUS_NUM_DISCRETE_INPUTS; i++) {
    mb.addIsts(i, discreteInputs[i]);
  }
  
  // Add holding registers
  for (uint16_t i = 0; i < MODUBS_NUM_HOLDING_REGISTERS; i++) {
    mb.addHreg(i, holdingRegisters[i]);
  }
  
  // Add input registers
  for (uint16_t i = 0; i < MODBUS_NUM_INPUT_REGISTERS; i++) {
    mb.addIreg(i, inputRegisters[i]);
  }
  
  // Start Modbus TCP server on port 8502
  mb.server(8502);
  
  Serial.println("INFO - Modbus Client: Started as SunSpec-compliant TCP server on port 8502");
}

void loop_modbus_server() {
  // Update SunSpec registers with latest Sol-Ark data
  update_sunspec_from_solark();
  
  // Update the Modbus registers with the latest values
  for (uint16_t i = 0; i < MODUBS_NUM_HOLDING_REGISTERS; i++) {
    mb.Hreg(i, holdingRegisters[i]);
  }
  
  // Process Modbus TCP requests
  mb.task();
  
  // Log SunSpec initialization once
  if (!sunspec_initialized) {
    Serial.println("INFO - Modbus Client: SunSpec registers updated with initial Sol-Ark data");
    sunspec_initialized = true;
  }
}
