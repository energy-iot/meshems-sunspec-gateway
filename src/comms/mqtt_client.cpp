/**
 * @file mqtt_client.cpp
 * @brief MQTT publisher for Sol-Ark inverter telemetry.
 *
 * Derived from the OpenAMI metering firmware's MQTT client (EmonESP lineage,
 * GPL v3) but carries Sol-Ark inverter payloads instead of per-tenant meter
 * readings.  Each subtopic is a self-contained JSON document so a subscriber
 * can consume only the categories it needs:
 *
 *   mfr        SunSpec Model 1 style manufacturer / serial / version block
 *   inverter   operating state and thermal telemetry
 *   battery    power, voltage, SOC, BMS warnings/faults
 *   grid       import/export power, per-leg current, frequency, connection state
 *   pv         per-string and total DC generation
 *   load       per-leg and total load power
 *   energy     lifetime kWh totalisers
 *   stats/bw   outbound/inbound bandwidth counters (periodic)
 */

#ifdef ENABLE_MQTT

#include <comms/mqtt_client.h>
#include <ArduinoJson.h>
#include <TimeLib.h>

#ifdef ENABLE_ETHERNET
  #include <Ethernet.h>
#else
  #include <WiFiMulti.h>
#endif

#include <core/config.h>
#include <metering/modbus_master.h>
#include <metering/modbus_solark.h>

// ---- Transport ------------------------------------------------------------
#ifdef ENABLE_ETHERNET
  static EthernetClient transportClient;
#else
  static WiFiClient transportClient;
#endif
static PubSubClient mqttclient(transportClient);

// ---- Bandwidth accounting -------------------------------------------------
// Published periodically so upstream link budgets can be verified in the field.
static unsigned long mqtt_BWPubOut_payload_bytes = 0;
static unsigned long mqtt_BWPubOut_tcpip_bytes   = 0;
static unsigned long mqtt_BWCmdIn_payload_bytes  = 0;
static unsigned long mqtt_BWCmdIn_tcpip_bytes    = 0;
static unsigned long mqtt_publish_count          = 0;
static unsigned long mqtt_cmd_count              = 0;
static unsigned long last_bandwidth_report_time  = 0;

static const unsigned long BANDWIDTH_REPORT_INTERVAL_MS = 300000; // 5 min

// Estimated TCP/IP + MQTT framing overhead charged to each message.
static const unsigned int TCPIP_OVERHEAD_BYTES = 60;

static String topic_device;   // "<MQTT_TOPIC>/<device_id>/"
static String topic_cmd;      // "<MQTT_TOPIC>/<device_id>/cmd"

static void mqtt_publish_json(const char* subtopic, const JsonDocument* payload);

// Build the device and command topic strings from the runtime device ID.
static void generateTopics() {
  topic_device.reserve(strlen(MQTT_TOPIC) + 1 + strlen(getDeviceID()) + 1 + 16);
  topic_device = MQTT_TOPIC;
  topic_device.concat("/");
  topic_device.concat(getDeviceID());
  topic_device.concat("/");

  topic_cmd = topic_device;
  topic_cmd.concat("cmd");
}

// Open the TCP socket, connect the MQTT session, and subscribe to the command
// topic.  Returns false on any step failing; the caller retries.
static boolean mqtt_connect() {
  Serial.printf("MQTT Connecting...timeout in:%d\r\n", transportClient.getTimeout());
  if (transportClient.connect(MQTT_SERVER, 1883) != 1) { // 8883 for TLS
    Serial.println("MQTT connect timeout.");
    return false;
  }

  mqttclient.setSocketTimeout(6);
  mqttclient.setBufferSize(MAX_DATA_LEN + 200);
  mqttclient.setKeepAlive(180);

  if (strcmp(MQTT_USER, "") == 0) {
    mqttclient.connect(getDeviceID());                       // anonymous broker
  } else {
    mqttclient.connect(getDeviceID(), MQTT_USER, MQTT_PW);
  }

  if (mqttclient.state() != 0) {
    Serial.printf("MQTT failed: %d\r\n", mqttclient.state());
    return false;
  }

  Serial.printf("MQTT connected: %s\r\n", MQTT_SERVER);
  if (!mqttclient.subscribe(topic_cmd.c_str())) {
    Serial.printf("MQTT: FAILED TO SUBSCRIBE TO COMMAND TOPIC: %s\r\n", topic_cmd.c_str());
    return false;
  }
  Serial.printf("MQTT: SUBSCRIBED TO COMMAND TOPIC: %s\r\n", topic_cmd.c_str());
  return true;
}

// Serialise a JSON document onto "<topic_device><subtopic>" using stack buffers
// to avoid heap fragmentation, and charge the result to the bandwidth counters.
static void mqtt_publish_json(const char* subtopic, const JsonDocument* payload) {
  size_t payload_len = measureJson(*payload);
  if (payload_len >= 1024) {
    Serial.println("MQTT publish: payload too large");
    return;
  }

  char data[1024];
  serializeJson(*payload, data, sizeof(data));

  char topicBuf[256];
  int topic_len = snprintf(topicBuf, sizeof(topicBuf), "%s%s",
                           topic_device.c_str(), subtopic);
  if (topic_len < 0 || topic_len >= (int)sizeof(topicBuf)) {
    Serial.println("MQTT publish: topic buffer overflow");
    return;
  }

  if (!mqttclient.publish(topicBuf, data)) {
    Serial.println("MQTT publish: failed");
    return;
  }

  mqtt_BWPubOut_payload_bytes += payload_len;
  mqtt_BWPubOut_tcpip_bytes   += payload_len + TCPIP_OVERHEAD_BYTES;
  mqtt_publish_count++;

#ifdef ENABLE_DEBUG
  Serial.printf("topic: %s, data: %s\n", topicBuf, data);
#endif
}

// Decode the Sol-Ark serial number, which the inverter returns as five 16-bit
// registers each packing two ASCII characters (big-endian within the register).
static void solark_serial_string(char* out, size_t out_len) {
  size_t w = 0;
  for (int i = 0; i < 5 && w + 1 < out_len; ++i) {
    uint16_t part = solark.getSerialNumberPart(i);
    if (part == 0) break;
    char hi = (part >> 8) & 0xFF;
    char lo = part & 0xFF;
    if (hi == 0) break;
    out[w++] = hi;
    if (lo == 0 || w + 1 >= out_len) break;
    out[w++] = lo;
  }
  out[w] = '\0';
}

// SunSpec Model 1 equivalent: static nameplate data. Low change rate, so this
// is published once per publish cycle rather than on the fast poll loop.
static void mqtt_publish_mfr(long timestamp) {
  char serial_str[33];
  solark_serial_string(serial_str, sizeof(serial_str));

  JsonDocument doc;
  doc["timestamp"]    = timestamp;
  doc["device_id"]    = getDeviceID();
  doc["manufacturer"] = "Sol-Ark";
  doc["model"]        = "Sol-Ark LV";
  doc["serial"]       = serial_str;
  doc["comm_version"] = solark.getCommVersion();
  doc["grid_type"]    = solark.getGridType();   // 0:Single 1:Split 2:Three-Phase Wye
  mqtt_publish_json("mfr", &doc);
}

static void mqtt_publish_inverter(long timestamp) {
  JsonDocument doc;
  doc["timestamp"]      = timestamp;
  doc["status"]         = solark.getInverterStatus(); // 1:Self-test 2:Normal 3:Alarm 4:Fault
  doc["dcdc_temp_c"]    = solark.getDCDCTemp();
  doc["igbt_temp_c"]    = solark.getIGBTTemp();
  doc["inverter_power_w"] = solark.getInverterPower();
  doc["inverter_v"]     = solark.getInverterVoltage();
  doc["inverter_hz"]    = solark.getInverterFrequency();
  mqtt_publish_json("inverter", &doc);
}

static void mqtt_publish_battery(long timestamp) {
  JsonDocument doc;
  doc["timestamp"]   = timestamp;
  doc["power_w"]     = solark.getBatteryPower();
  doc["current_a"]   = solark.getBatteryCurrent();
  doc["voltage_v"]   = solark.getBatteryVoltage();
  doc["soc_pct"]     = solark.getBatterySOC();
  doc["temp_c"]      = solark.getBatteryTemperature();
  doc["capacity_ah"] = solark.getBatteryCapacity();
  doc["bms_soc_pct"] = solark.getBMSRealTimeSOC();
  doc["bms_warning"] = solark.getBMSWarning();
  doc["bms_fault"]   = solark.getBMSFault();
  doc["charging"]    = solark.isBatteryCharging();
  doc["discharging"] = solark.isBatteryDischarging();
  mqtt_publish_json("battery", &doc);
}

static void mqtt_publish_grid(long timestamp) {
  JsonDocument doc;
  doc["timestamp"]  = timestamp;
  doc["power_w"]    = solark.getGridPower();
  doc["voltage_v"]  = solark.getGridVoltage();
  doc["current_l1_a"] = solark.getGridCurrentL1();
  doc["current_l2_a"] = solark.getGridCurrentL2();
  doc["frequency_hz"] = solark.getGridFrequency();
  doc["connected"]  = solark.isGridConnected();
  doc["selling"]    = solark.isSellingToGrid();
  doc["buying"]     = solark.isBuyingFromGrid();
  mqtt_publish_json("grid", &doc);
}

static void mqtt_publish_pv(long timestamp) {
  JsonDocument doc;
  doc["timestamp"]   = timestamp;
  doc["pv1_power_w"] = solark.getPV1Power();
  doc["pv2_power_w"] = solark.getPV2Power();
  doc["total_kw"]    = solark.getPVPowerTotal();
  mqtt_publish_json("pv", &doc);
}

static void mqtt_publish_load(long timestamp) {
  JsonDocument doc;
  doc["timestamp"]     = timestamp;
  doc["load_l1_w"]     = solark.getLoadPowerL1();
  doc["load_l2_w"]     = solark.getLoadPowerL2();
  doc["load_total_w"]  = solark.getLoadPowerTotal();
  doc["smart_load_w"]  = solark.getSmartLoadPower();
  doc["frequency_hz"]  = solark.getLoadFrequency();
  mqtt_publish_json("load", &doc);
}

static void mqtt_publish_energy(long timestamp) {
  JsonDocument doc;
  doc["timestamp"]           = timestamp;
  doc["batt_charge_kwh"]     = solark.getBatteryChargeEnergy();
  doc["batt_discharge_kwh"]  = solark.getBatteryDischargeEnergy();
  doc["grid_buy_kwh"]        = solark.getGridBuyEnergy();
  doc["grid_sell_kwh"]       = solark.getGridSellEnergy();
  doc["load_kwh"]            = solark.getLoadEnergy();
  doc["pv_kwh"]              = solark.getPVEnergy();
  mqtt_publish_json("energy", &doc);
}

// Report and reset the bandwidth counters. Both directions are emitted together
// so the two samples share an interval; last_bandwidth_report_time is advanced
// only here.
static void mqtt_publish_bandwidth_stats() {
  unsigned long current_time = millis();
  unsigned long elapsed_ms   = current_time - last_bandwidth_report_time;
  float elapsed_sec = elapsed_ms / 1000.0f;

  JsonDocument out;
  out["interval_ms"]   = elapsed_ms;
  out["pubout_count"]  = mqtt_publish_count;
  out["payload_bytes"] = mqtt_BWPubOut_payload_bytes;
  out["tcpip_bytes"]   = mqtt_BWPubOut_tcpip_bytes;
  out["avg_bps_out"]   = (elapsed_sec > 0) ? (int)((mqtt_BWPubOut_tcpip_bytes * 8) / elapsed_sec) : 0;
  out["timestamp"]     = now();
  mqtt_publish_json("stats/BWPubOut", &out);

  JsonDocument in;
  in["interval_ms"]   = elapsed_ms;
  in["cmdin_count"]   = mqtt_cmd_count;
  in["payload_bytes"] = mqtt_BWCmdIn_payload_bytes;
  in["tcpip_bytes"]   = mqtt_BWCmdIn_tcpip_bytes;
  in["avg_bps_in"]    = (elapsed_sec > 0) ? (int)((mqtt_BWCmdIn_tcpip_bytes * 8) / elapsed_sec) : 0;
  in["timestamp"]     = now();
  mqtt_publish_json("stats/BWCmdIn", &in);

  mqtt_BWPubOut_payload_bytes = 0;
  mqtt_BWPubOut_tcpip_bytes   = 0;
  mqtt_publish_count          = 0;
  mqtt_BWCmdIn_payload_bytes  = 0;
  mqtt_BWCmdIn_tcpip_bytes    = 0;
  mqtt_cmd_count              = 0;
  last_bandwidth_report_time  = current_time;
}

// ---- Southbound command handling ------------------------------------------

static void cmd_report(const JsonDocument&) {
  // Force an immediate publish of every telemetry subtopic.
  long ts = now();
  mqtt_publish_mfr(ts);
  mqtt_publish_inverter(ts);
  mqtt_publish_battery(ts);
  mqtt_publish_grid(ts);
  mqtt_publish_pv(ts);
  mqtt_publish_load(ts);
  mqtt_publish_energy(ts);
}

static void cmd_inverter(const JsonDocument&) { Serial.printf("matched \"inverter\"\n"); }
static void cmd_bms     (const JsonDocument&) { Serial.printf("matched \"bms\"\n");      }

typedef void (*cmd_handler_t)(const JsonDocument&);
struct CmdEntry { const char* keyword; cmd_handler_t handler; };
static const CmdEntry cmd_table[] = {
  { "report",   cmd_report   },
  { "inverter", cmd_inverter },
  { "bms",      cmd_bms      },
};

// PubSubClient callback for "<MQTT_TOPIC>/<device_id>/cmd".
void subscriber_callback(char* topic, uint8_t* payload, unsigned int length) {
  if (length > 254) {
    Serial.printf("MQTT CALLBACK: not handled: payload len overrun:%d\n", length);
    return;
  }
  if (strcmp(topic, topic_cmd.c_str()) != 0) return;

  mqtt_cmd_count++;
  mqtt_BWCmdIn_payload_bytes += length;
  mqtt_BWCmdIn_tcpip_bytes   += length + TCPIP_OVERHEAD_BYTES;

  char payload_buf[256] = {0};
  memcpy(payload_buf, payload, length);
  payload_buf[length] = '\0';
  Serial.printf("\n***MQTT CALLBACK: topic '%s', payload '%s'\n", topic, payload_buf);

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload_buf);
  if (err) {
    Serial.printf("MQTT CALLBACK: JSON parse error: %s\n", err.c_str());
    return;
  }

  const char* cmd = doc["cmd"] | "";
  for (size_t i = 0; i < sizeof(cmd_table) / sizeof(cmd_table[0]); i++) {
    if (strcmp(cmd, cmd_table[i].keyword) == 0) {
      cmd_table[i].handler(doc);
      return;
    }
  }
  Serial.printf("MQTT CALLBACK: no match for cmd '%s'\n", cmd);
}

// ---- Lifecycle ------------------------------------------------------------

void setup_mqtt_client() {
  generateTopics();
  mqttclient.setCallback(subscriber_callback);

  for (int attempt = 0; attempt < 3; attempt++) {
    if (mqtt_connect()) {
      last_bandwidth_report_time = millis();
      return;
    }
    delay(250 * (attempt + 1));
  }
  Serial.println("MQTT: FAILED TO CONNECT");
}

// Publish one full telemetry cycle. Called from the main loop at
// MQTTPublish_rootrate; see the rate TODO in main.cpp for per-topic cadences.
void loop_mqtt() {
  if (!mqttclient.connected()) {
    Serial.println("MQTT not connected!");
    return;
  }

  long ts = now();
  mqtt_publish_mfr(ts);
  mqtt_publish_inverter(ts);
  mqtt_publish_battery(ts);
  mqtt_publish_grid(ts);
  mqtt_publish_pv(ts);
  mqtt_publish_load(ts);
  mqtt_publish_energy(ts);

  if (millis() - last_bandwidth_report_time >= BANDWIDTH_REPORT_INTERVAL_MS) {
    mqtt_publish_bandwidth_stats();
  }
}

void poll_mqtt() {
  mqttclient.loop();
}

void maintain_mqtt_connection() {
  if (!mqttclient.connected()) {
    Serial.println("MQTT: connection lost, attempting reconnect");
    mqtt_connect();
  }
}

void mqtt_restart() {
  if (mqttclient.connected()) {
    mqttclient.disconnect();
  }
}

boolean mqtt_connected() {
  return mqttclient.connected();
}

#endif // ENABLE_MQTT
