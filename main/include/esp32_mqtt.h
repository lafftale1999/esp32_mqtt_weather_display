#ifndef ESP32_MQTT_H_
#define ESP32_MQTT_H_

#define MQTT_SERVER_ADDRESS     "192.168.61.111"
#define MQTT_MTLS_PORT          8883
#define MQTT_TOPIC              "/room_meas"
#define MQTT_STANDARD_QOS       1

#define MQTT_PAYLOAD_MAX_LEN    256
#define MQTT_PARSED_MAX_LEN     256
#define MQTT_TOPIC_MAX_LEN      64
#define MQTT_WAIT_FOR_MUTEX_MS  50
#define MQTT_MAX_TRIES_SAVE     3

#define MQTT_JSON_TEMP_KEY      "temperature"
#define MQTT_JSON_HUM_KEY       "humidity"
#define MQTT_JSON_PRESS_KEY     "pressure"

#define TEMP_DIVISION_VAL       100.0f
#define HUM_DIVISION_VAL        1024.0f
#define PRESS_DIVISION_VAL      (256.0f * 100.0f) 

#include <stdint.h>
#include "mqtt_client.h"

uint8_t mqtt_open(esp_mqtt_client_handle_t *handle);
char* mqtt_get_parsed_string();
void mqtt_main_loop(void *pvParam);

#endif