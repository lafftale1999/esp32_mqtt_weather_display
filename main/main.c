#include <stdio.h>
#include "nvs_flash.h"

#include "lcd_1602.h"
#include "esp32_wifi.h"

const char* TAG = "main";

int app_main(void)
{

    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    if (i2c_open(&bus_handle, &dev_handle, DEVICE_ADDRESS) != 0) {
        ESP_LOGE(TAG, "Unable to initialize I2C");
        return 1;
    }

    if (lcd_1602_init(dev_handle) != 0) {
        ESP_LOGE(TAG, "Unable to initialize LCD1602");
        return 1;
    }

    ESP_ERROR_CHECK(wifi_init());
    wait_for_connection();

    // mqtt init

    while(1) {
        lcd_1602_send_string(dev_handle, "hello world");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}