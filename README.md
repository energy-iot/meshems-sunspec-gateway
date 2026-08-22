# MeshEMS | Proprietary Application to SunSpec TCP Gateway for Sol-Ark Inverters

[![OpenSSF Scorecard](https://api.scorecard.dev/projects/github.com/energy-iot/meshems-openami-metering/badge)](https://scorecard.dev/viewer/?uri=github.com/energy-iot/meshems-openami-metering)

This document describes the SunSpec compliance implementation for the Sol-Ark inverter with the MeshEMS platform.

## Overview

This implementation allows the MeshEMS platform to act as a SunSpec-compliant Modbus TCP/IP server, exposing Sol-Ark inverter data in a standardized format that can be read by any SunSpec-compatible client.

The implementation follows the SunSpec Alliance specifications and includes:
- SunSpec Common Model (1) - Basic device information
- SunSpec Inverter Model (701) - AC Photovoltaic Inverter data
- SunSpec DER Storage Capacity Model (713) - Battery storage information

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
  - [Source Layout](#source-layout)
- [Build Configuration](#build-configuration)
  - [WiFi Credentials](#wifi-credentials)
  - [Board Revisions](#board-revisions)
- [SunSpec Model Implementation](#sunspec-model-implementation)
- [Register Map](#register-map)
- [Data Mapping](#data-mapping)
- [Usage](#usage)
- [Testing](#testing)
- [Example TCP Client Connection](#example-tcp-client-connection)
- [References](#references)

## Features

- Modbus RTU master on RS485_1 polling a Sol-Ark LV inverter
- SunSpec-compliant Modbus TCP server (Models 1, 701, 713)
- WiFi or W5500 SPI Ethernet uplink (mutually exclusive)
- Optional MQTT telemetry of Sol-Ark data under the OpenAMI topic convention
- Optional SH1106 OLED console, onboard SSR control, and SD card CSV logging
- Feature-flag build system — compile only the subsystems you need

All optional subsystems are guarded by `#ifdef` feature flags set in
`platformio.ini`. See [`docs/FEATURE_FLAGS.md`](docs/FEATURE_FLAGS.md) for the
full flag reference and example configurations.

### Current Feature Status

Active build flags (see `platformio.ini`): `ENABLE_WIFI`, `ENABLE_MODBUS_MASTER`,
`ENABLE_MODBUS_TCP_SERVER`, `ENABLE_DEBUG`, `BOARD_VER_V3`.

| Feature/Component | Status | Notes |
|---|---|---|
| Sol-Ark RTU polling (RS485_1) | Enabled | 9600 baud, node address `0x01` |
| SunSpec Modbus TCP server | Enabled | Port 8502; override with `-DSUNSPEC_TCP_PORT=<n>` |
| WiFi | Enabled | Credentials in gitignored `include/secrets.h` |
| W5500 Ethernet | Available, disabled by default | `BOARD_VER_V3` only; mutually exclusive with WiFi |
| MQTT telemetry | Available, disabled by default | Seven Sol-Ark subtopics + bandwidth stats |
| OLED SH1106 display | Available, disabled by default | Console UI and boot splash |
| SD card + Sol-Ark CSV log | Available, disabled by default | `BOARD_VER_V3` only |
| Onboard SSR (GPIO 38) | Available, disabled by default | `ENABLE_RELAYS` |

## Architecture

The implementation consists of the following components:

1. **SunSpec Models Definition** (`include/metering/sunspec_models.h`): Defines the SunSpec model structure, register maps, and constants.

2. **SunSpec Mapper** (`src/metering/sunspec_mapper.cpp`): Implements the mapping between Sol-Ark data and SunSpec registers.

3. **Modbus RTU Master** (`src/metering/modbus_master.cpp`): Polls the Sol-Ark inverter on RS485_1 and maintains the decoded register cache.

4. **Modbus TCP Server** (`src/metering/modbus_server.cpp`): Initializes the SunSpec models and serves the register map over TCP.

### Source Layout

```
include/
  core/      config.h  console.h  data_model.h  debug.h  pins.h
  hw/        buttons.h  display.h  relay.h  sd_card.h  sd_logger.h
  comms/     wifi.h  ethernet.h  mqtt_client.h
  metering/  modbus.h  modbus_master.h  modbus_server.h  modbus_solark.h  sunspec_models.h
  secrets_example.h        (copy to secrets.h — gitignored)
src/
  core/      main.cpp  config.cpp  console.cpp  data_model.cpp
  hw/        buttons.cpp  display.cpp  relay.cpp  sd_card.cpp  sd_logger.cpp
  comms/     wifi.cpp  ethernet.cpp  mqtt_client.cpp
  metering/  modbus_master.cpp  modbus_server.cpp  modbus_solark.cpp
             modbus_scanner.cpp  sunspec_mapper.cpp
docs/        FEATURE_FLAGS.md  CHANGELOG.md  AGENTS.md
scripts/     pick_serial_port.py
utilities/   sunspec_client_example.py  test_sunspec_registers.py  requirements.txt
```

## Build Configuration

Features are selected with `-D` flags in the `[common] build_flags` section of
`platformio.ini`. Invalid combinations fail at compile time with a descriptive
`#error` rather than misbehaving at runtime — see
[`docs/FEATURE_FLAGS.md`](docs/FEATURE_FLAGS.md).

```bash
pio run              # build
pio run -t upload    # build and flash (port auto-selected)
pio device monitor   # 115200 baud
```

### WiFi Credentials

Credentials are not stored in tracked source. Before building with `-DENABLE_WIFI`:

```bash
cp include/secrets_example.h include/secrets.h
# edit include/secrets.h and set WIFI_SSID / WIFI_PW
```

### Board Revisions

Exactly one board revision macro must be set in `platformio.ini`:

| Flag | Board |
|---|---|
| `BOARD_VER_V3` | NESL EMS Controller PCBA 865B (default) |
| `BOARD_VER_V1` | Legacy hand-soldered 2025 prototype |
| `BOARD_VER_V2` | Legacy 2025 board with swapped RS-485 module pins |

SD card and Ethernet pins exist only on `BOARD_VER_V3`.

## SunSpec Model Implementation

### Common Model (1)

The Common Model provides basic information about the device:
- Manufacturer: "Sol-Ark"
- Model: "Sol-Ark-12K-P" (Dynamically read from inverter if possible, otherwise this placeholder)
- Options: "None" (As per current implementation)
- Version: Dynamically read from Sol-Ark COMM_VERSION
- Serial Number: Dynamically read from Sol-Ark SN_BYTE parts

### Inverter Model (701) - AC Photovoltaic Inverter

The Inverter Model provides real-time data from the Sol-Ark inverter:
- AC measurements (current, voltage, power, frequency, energy, VA, VAR, PF)
- Operating state, status, and alarm information
- Grid connection status and DER operational characteristics
- Temperature (cabinet, transformer, IGBT)
- Per-phase AC measurements (L1, L2)

### DER Storage Capacity Model (713)

The DER Storage Capacity Model provides information about the connected battery storage:
- Energy Rating (Wh)
- Energy Available (Wh)
- State of Charge (SoC %)
- State of Health (SoH %) - Currently defaults to 100%
- Storage Status (OK, Warning, Error) - Based on BMS data

## Register Map

The SunSpec register map is implemented in the Modbus holding registers:

- Base address: 40000
- SunSpec ID marker ("SunS"): Registers 40000-40001
- Common Model (1): Starts at register 40002 (Offset 2 from base)
- Inverter Model (701): Starts at register 40070 (Offset 70 from base)
- DER Storage Capacity Model (713): Starts at register 40225 (Offset 225 from base)
- SunSpec End Block: Starts after the last model.

## Data Mapping

The implementation maps Sol-Ark data to SunSpec registers as follows:

### AC Measurements
- AC current: Average of inverter L1 and L2 currents
- AC voltage: Inverter voltage
- AC power: Inverter output power
- AC frequency: Inverter frequency
- AC energy: Load energy (converted from kWh to Wh)

### DC Measurements
- DC current: Battery current
- DC voltage: Battery voltage
- DC power: Battery power

### Temperature
- Cabinet temperature: Battery temperature

### Status
- Inverter status: Derived from inverter power
- Vendor-specific status: Grid connection, battery charging/discharging status (placed in Alarm Info field of Model 701)
- DER Mode: Grid Following/Forming based on grid relay status.
- AC Wiring Type: Dynamically set based on Sol-Ark Grid Type.

### DER Storage Capacity (Model 713)
- Energy Rating: Calculated from Sol-Ark battery capacity (Ah) and nominal voltage.
- Energy Available: Calculated from Energy Rating, SoC, and SoH.
- State of Charge (SoC): Uses Sol-Ark Battery SOC, overridden by BMS Real Time SOC if available.
- State of Health (SoH): Defaults to 100% as Sol-Ark does not directly provide this.
- Storage Status: Determined from Sol-Ark BMS Warning and Fault registers.

## Usage

The SunSpec-compliant Modbus TCP/IP server runs with the following settings:
- Network: WiFi (`ENABLE_WIFI`) or W5500 Ethernet (`ENABLE_ETHERNET`)
- IP Address: Dynamically assigned by DHCP (displayed on the OLED screen when `ENABLE_OLED_DISPLAY` is set)
- TCP Port: 8502 by default — override with `-DSUNSPEC_TCP_PORT=<n>`
- Protocol: Modbus TCP/IP

Any SunSpec-compatible Modbus TCP client can connect to this server to read the standardized inverter data.

## Testing

You can test the SunSpec implementation using:
1. SunSpec-compatible client software (e.g., SunSpec Dashboard)
2. Modbus TCP polling tools with the appropriate register map (e.g., ModbusPoll, QModMaster)
3. pysunspec2 library for Python-based testing (with TCP transport)
4. Example Python script in utilities/sunspec_client_example.py (modified for TCP)

## Example TCP Client Connection

Using the pysunspec2 library:

```python
import sunspec2.modbus.client as client

# Connect to the SunSpec TCP server
c = client.SunSpecModbusClientTCP(host='192.168.1.x', port=8502)
c.connect()

# Read the models
c.scan()

# Access the inverter model
inv = c.models[701]

# Read inverter data
print(f"AC Power: {inv.points['W'].value} W")
print(f"AC Voltage: {inv.points['PhVphA'].value} V")
print(f"AC Frequency: {inv.points['Hz'].value} Hz")
print(f"DC Voltage: {inv.points['DCV'].value} V")
print(f"DC Current: {inv.points['DCA'].value} A")
print(f"Cabinet Temperature: {inv.points['TmpCab'].value} °C")

# Close the connection
c.close()
```
# Firmware Updates
Follow https://github.com/energy-iot/meshems-openami-metering#dev-environment-installation-guide

## References

- SunSpec Alliance: https://sunspec.org/
- pysunspec2 library: https://github.com/sunspec/pysunspec2
- SunSpec Model 701: https://sunspec.org/wp-content/uploads/SunSpec-Inverter-Models-7xx-20200707.xlsx
- SunSpec Model 713: https://sunspec.org/wp-content/uploads/SunSpec-DER-Storage-Models-7xx-20200707.xlsx
- Modbus-ESP8266 library: https://github.com/emelianov/modbus-esp8266
