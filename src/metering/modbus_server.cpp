/*
  ModbusTCP SunSpec Server (slave device)

  This implementation provides a SunSpec-compliant Modbus TCP server using the
  modbus-esp8266 library. It exposes Sol-Ark inverter data in a standardized
  SunSpec format over TCP/IP.

  Created: 2025-05-14
  Based on previous RTU implementation by: C. M. Bulliner, doug mendonca

  SunSpec compliance added: 2025-04-23
  TCP/IP implementation added: 2025-05-14
*/

#ifdef ENABLE_MODBUS_TCP_SERVER

#ifdef ENABLE_ETHERNET
  #include <Ethernet.h>
  #include <ModbusEthernet.h>
#else
  #include <ModbusIP_ESP8266.h>
#endif
#include <core/config.h>
#include <core/data_model.h>
#include <core/debug.h>
#include <metering/modbus_server.h>
#include <metering/sunspec_models.h>

// Modbus TCP instance bound to the active network interface.
//
// The library ships two distinct transports and they are NOT interchangeable:
// ModbusIP is templated on WiFiServer/WiFiClient, so using it on an
// Ethernet-only build calls WiFiServer::begin() against a WiFi stack that was
// never started — which panics and reboots. ModbusEthernet is the W5500 variant.
#ifdef ENABLE_ETHERNET

// ModbusEthernet's internal EthernetServerWrapper declares begin(uint16_t port=0)
// as a no-op, which *hides* the virtual EthernetServer::begin(). The result is
// that ModbusTCPTemplate::server() allocates the server but never puts the
// socket into LISTEN, so the port silently accepts nothing. Reach the protected
// tcpserver member and invoke the real base implementation explicitly.
class SunSpecModbusEthernet : public ModbusEthernet {
public:
    void listen(uint16_t port) {
        server(port);
        if (tcpserver != nullptr) {
            tcpserver->EthernetServer::begin();
        }
    }
};
static SunSpecModbusEthernet mb;

#else
static ModbusIP mb;
#endif

// Tracks whether the register map has been populated at least once
static bool sunspec_initialized = false;

// Publish the SunSpec register map and start listening. Assumes the network
// interface is already up.
void setup_modbus_server() {
  setup_sunspec_models();

  Serial.println("INFO - Modbus Server: SunSpec models initialized");
  Serial.println("INFO - Modbus Server: SunSpec Common (1) and Inverter (701) models available");

  // ModbusIP requires each register to be registered individually.
  for (uint16_t i = 0; i < MODBUS_NUM_COILS; i++) {
    mb.addCoil(i, coils[i]);
  }
  for (uint16_t i = 0; i < MODBUS_NUM_DISCRETE_INPUTS; i++) {
    mb.addIsts(i, discreteInputs[i]);
  }
  for (uint16_t i = 0; i < MODUBS_NUM_HOLDING_REGISTERS; i++) {
    mb.addHreg(i, holdingRegisters[i]);
  }
  for (uint16_t i = 0; i < MODBUS_NUM_INPUT_REGISTERS; i++) {
    mb.addIreg(i, inputRegisters[i]);
  }

#ifdef ENABLE_ETHERNET
  mb.listen(SUNSPEC_TCP_PORT);
  Serial.printf("INFO - Modbus Server: SunSpec TCP server on W5500, port %d\n",
                SUNSPEC_TCP_PORT);
#else
  mb.server(SUNSPEC_TCP_PORT);
  Serial.printf("INFO - Modbus Server: SunSpec TCP server on WiFi, port %d\n",
                SUNSPEC_TCP_PORT);
#endif
}

// Refresh the SunSpec map from the Sol-Ark cache, mirror it into the ModbusIP
// register bank, then service pending TCP requests.
void loop_modbus_server() {
  update_sunspec_from_solark();

  for (uint16_t i = 0; i < MODUBS_NUM_HOLDING_REGISTERS; i++) {
    mb.Hreg(i, holdingRegisters[i]);
  }

  mb.task();

  if (!sunspec_initialized) {
    Serial.println("INFO - Modbus Server: SunSpec registers updated with initial Sol-Ark data");
    sunspec_initialized = true;
  }
}

#endif // ENABLE_MODBUS_TCP_SERVER
