#include "nvs_flash.h"

#include "lcd_1602.h"
#include "esp32_wifi.h"
#include "esp32_mqtt.h"

const char* TAG = "main";

int app_main(void)
{
    // Needed for saving data in non-volatile storage
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // I2C initialization
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    if (i2c_open(&bus_handle, &dev_handle, DEVICE_ADDRESS) != 0) {
        ESP_LOGE(TAG, "Unable to initialize I2C");
        return 1;
    }

    // LCD initialization
    if (lcd_1602_init(dev_handle) != 0) {
        ESP_LOGE(TAG, "Unable to initialize LCD1602");
        return 1;
    }

    // WIFI initialization
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(wait_for_connection(10000));

    // MQTT initialization
    esp_mqtt_client_handle_t mqtt_handle;
    if (mqtt_open(&mqtt_handle) != 0) {
        ESP_LOGE(TAG, "Unable to initialize MQTT");
        return 1;
    }

    xTaskCreate(mqtt_main_loop, "mqtt_main_loop", 4096, NULL, 5, NULL);

    char *string = NULL;

    while(1) {
        if((string = mqtt_get_parsed_string()) != NULL) {
            ESP_LOGE(TAG, "%s", string);
            lcd_1602_send_string(dev_handle, string);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    return 0;
}