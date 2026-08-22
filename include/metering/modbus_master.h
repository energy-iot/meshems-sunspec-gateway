/**
 * @file modbus_master.h
 * @brief Modbus RTU master on RS485_1 — polls the Sol-Ark LV inverter.
 *
 * Named "master" to match the metering firmware convention: this side drives
 * the RS-485 bus. The SunSpec Modbus TCP slave lives in modbus_server.h.
 *
 * Watch out for MAX485 DO VS D1 unloaded SERIAL SIDE AFFECTS - SEE TECH DETAILS AT
 * https://www.analog.com/en/products/max485.html#part-details
 * BIAS RESISTORS DETAILED TECH SPECS HERE
 * https://control.com/forums/threads/modbus-standard-termination.20389/
 * ***** must use 620-150 OHM TERMINATION RESISTOR AT A-B FAR END TERMINATION TO
 * MINIMIZE REFLECTIONS ******
 *
 * TESTING WITH 150 OHM RESISTOR ACROSS LAST FURTHEST A-B MODBUS RTU ENDPOINT THIS
 * CAN BE 620 OHM ALTERNATIVELY - FURTHER SCOPE TESTING REQUIRED ON CAT5E VS STP
 * RS485 CABLE
 */

#pragma once

#include <ModbusMaster.h>
#include <metering/modbus_solark.h>

// The single Sol-Ark inverter instance. Shared with sunspec_mapper.cpp, the
// MQTT publisher, and the SD logger.
extern Modbus_SolArkLV solark;

void setup_modbus_master();
void loop_modbus_master();

// Number of successful poll cycles since boot. Consumers use a non-zero count
// as the signal that the register cache holds real data.
unsigned long get_solark_success_count();
