#ifndef ESP32_WIFI_H_
#define ESP32_WIFI_H_

#include <stdbool.h>
#include "esp_err.h"

esp_err_t wifi_init();
bool wifi_is_connected();
esp_err_t wait_for_connection(void);

#endif