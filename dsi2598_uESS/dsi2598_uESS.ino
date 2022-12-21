#include <Wire.h>
#include <Adafruit_INA219.h>
#include "bc26.h"

Adafruit_INA219 ina219_in(0x40);
Adafruit_INA219 ina219_out(0x41);

const char *apn   = "internet";         // CHT APN
const char *host  = "io.adafruit.com";  
const char *user  = "YOUR_ADAFRUIT_NAME";
const char *key   = "YOUR_ADAFRUIT_KEY";
const char *topic_load_a = "YORUNAME/feeds/dsi2598-plus.load-a";    // fill up correct topic for load current
const char *topic_load_v = "YORUNAME/feeds/dsi2598-plus.load-v";    // fill up correct topic for load voltage
const char *topic_load_w = "YORUNAME/feeds/dsi2598-plus.load-w";    // fill up correct topic for load energy
const char *topic_pv_a = "YORUNAME/feeds/dsi2598-plus.pv-a";        // fill up correct topic for pv current
const char *topic_pv_v = "YORUNAME/feeds/dsi2598-plus.pv-v";        // fill up correct topic for pv voltage
const char *topic_pv_w = "YORUNAME/feeds/dsi2598-plus.pv-w";        // fill up correct topic for pv energy

#define SENSOR_INTERVAL     1000    //ms
#define UPLOAD_INTERVAL     10000   //ms

void setup(void)
{
  Serial.begin(115200);
  //  while (!Serial) {
  // will pause Zero, Leonardo, etc until serial console opens
  //      delay(1);
  //  }

  Serial.println("Hello uESS!");

  // Initialize the INA219.
  // By default the initialization will use the largest range (32V, 2A).  However
  // you can call a setCalibration function to change this range (see comments).
  if (! ina219_in.begin()) {
    Serial.println("Failed to find INA219 chip A");
    //    delay(2000);
    while (1) {
      delay(100);
    }
  }

  if (! ina219_out.begin()) {
    Serial.println("Failed to find INA219 chip B");
    //    delay(2000);
    while (1) {
      delay(100);
    }
  }
  // To use a slightly lower 32V, 1A range (higher precision on amps):
  //ina219.setCalibration_32V_1A();
  // Or to use a lower 16V, 400mA range (higher precision on volts and amps):
  //ina219.setCalibration_16V_400mA();

  // bc26 init
  BC26Init(BAUDRATE_115200, apn, BAND_8);
  // connect to server
  BC26ConnectMQTTServer(host, user, key, MQTT_PORT_1883);

  Serial.println("Measuring voltage and current with INA219 ...");
}

typedef enum {
  pv_v = 0,
  pv_a,
  pv_w,
  load_v,
  load_a,
  load_w,
  STATE_MAX
} UPLOAD_STATE_t;


float pv_shunt_v = 0;
float pv_bus_v = 0;
float pv_mA = 0;
float pv_load_v = 0;
float pv_mW = 0;

float load_shunt_v = 0;
float load_bus_v = 0;
float load_mA = 0;
float load_load_v = 0;
float load_mW = 0;

void loop(void)
{
  char        buff[128];
  int         tmp_v_decimal;
  static long timer_upload = 0;
  static long timer_sensor = 0;
  static int  upload_state = 0;


  if (millis() - timer_sensor >= SENSOR_INTERVAL) {
    timer_sensor = millis();

    pv_shunt_v = ina219_in.getShuntVoltage_mV();
    pv_bus_v = ina219_in.getBusVoltage_V();
    pv_mA = ina219_in.getCurrent_mA();
    pv_mW = ina219_in.getPower_mW();
    pv_load_v = pv_bus_v + (pv_shunt_v / 1000);

    Serial.print("A: Bus Voltage:   "); Serial.print(pv_bus_v); Serial.println(" V");
    Serial.print("A: Shunt Voltage: "); Serial.print(pv_shunt_v); Serial.println(" mV");
    Serial.print("A: Load Voltage:  "); Serial.print(pv_load_v); Serial.println(" V");
    Serial.print("A: Current:       "); Serial.print(pv_mA); Serial.println(" mA");
    Serial.print("A: Power:         "); Serial.print(pv_mW); Serial.println(" mW");
    Serial.println("");

    load_shunt_v = ina219_out.getShuntVoltage_mV();
    load_bus_v = ina219_out.getBusVoltage_V();
    load_mA = ina219_out.getCurrent_mA();
    load_mW = ina219_out.getPower_mW();
    load_load_v = load_bus_v + (load_shunt_v / 1000);

    Serial.print("B: Bus Voltage:   "); Serial.print(load_bus_v); Serial.println(" V");
    Serial.print("B: Shunt Voltage: "); Serial.print(load_shunt_v); Serial.println(" mV");
    Serial.print("B: Load Voltage:  "); Serial.print(load_load_v); Serial.println(" V");
    Serial.print("B: Current:       "); Serial.print(load_mA); Serial.println(" mA");
    Serial.print("B: Power:         "); Serial.print(load_mW); Serial.println(" mW");
    Serial.println("");
  }

  if (millis() - timer_upload >= UPLOAD_INTERVAL) {
    timer_upload = millis();

    switch (upload_state) {
      case pv_v:
        Serial.println("upload to MQTT Broker - pv v");
        tmp_v_decimal = pv_load_v * 1000;
        tmp_v_decimal = tmp_v_decimal % 1000;
        sprintf(buff, "%d.%03d", (int)pv_load_v, tmp_v_decimal);
        BC26MQTTPublish(topic_pv_v, buff, MQTT_QOS0);
        break;

      case pv_a:
        Serial.println("upload to MQTT Broker - pv a");
        sprintf(buff, "%d", (int)pv_mA);
        BC26MQTTPublish(topic_pv_a, buff, MQTT_QOS0);
        break;

      case pv_w:
        Serial.println("upload to MQTT Broker - pv w");
        sprintf(buff, "%d", (int)pv_mW);
        BC26MQTTPublish(topic_pv_w, buff, MQTT_QOS0);
        break;

      case load_v:

        Serial.println("upload to MQTT Broker - load v");
        tmp_v_decimal = load_load_v * 1000;
        tmp_v_decimal = tmp_v_decimal % 1000;

        sprintf(buff, "%d.%03d", (int)load_load_v, tmp_v_decimal);
        BC26MQTTPublish(topic_load_v, buff, MQTT_QOS0);
        break;

      case load_a:
        Serial.println("upload to MQTT Broker - load a");
        sprintf(buff, "%d", (int)load_mA);
        BC26MQTTPublish(topic_load_a, buff, MQTT_QOS0);
        break;

      case load_w:
        Serial.println("upload to MQTT Broker - load w");
        sprintf(buff, "%d", (int)load_mW);
        BC26MQTTPublish(topic_load_w, buff, MQTT_QOS0);
        break;

      default:
        upload_state = 0;
        break;
    }

    if (++upload_state >= STATE_MAX) {
      upload_state = 0;
    }
  }

  //  delay(20);
}
