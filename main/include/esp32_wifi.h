#ifndef ESP32_WIFI_H_
#define ESP32_WIFI_H_

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initializes the wifi connection in station mode and registers
 * event handler to listen for wifi events.
 * 
 * @return ESP_OK for success. Will restart on fail.
 */
esp_err_t wifi_init();

/**
 * @brief Checks if the unit is connected to WiFi.
 * 
 * @return true if connected. false if not.
 */
bool wifi_is_connected();

/**
 * @brief waits until wifi is connected. Waits undefinetely.
 * 
 * @param wait_ms amount of time in ms to wait for connection.
 * 
 * @return ESP_OK for connected. ESP_FAIL for not connected.
 */
esp_err_t wait_for_connection(const int wait_ms);

#endif