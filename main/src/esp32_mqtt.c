#include "esp32_mqtt.h"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <cJSON.h>

#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"

static const char *TAG = "esp32_mqtt";

extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_crt_end");

typedef struct{
    char topic[MQTT_TOPIC_MAX_LEN];
    char payload[MQTT_PAYLOAD_MAX_LEN];
    char parsed_string[MQTT_PARSED_MAX_LEN];
    SemaphoreHandle_t mutex;
}mqtt_message_t;

static QueueHandle_t message_queue_handle = NULL;
static mqtt_message_t mqtt_message;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void parse_json_string() {
    cJSON *root = cJSON_Parse(mqtt_message.payload);
    ESP_LOGI(TAG, "Payload parsing: %s", mqtt_message.payload);

    if (!root) {
        printf("JSON parse error\n");
        return;
    }

    const cJSON *temp = cJSON_GetObjectItem(root, MQTT_JSON_TEMP_KEY);
    const cJSON *hum  = cJSON_GetObjectItem(root, MQTT_JSON_HUM_KEY);
    const cJSON *press = cJSON_GetObjectItem(root, MQTT_JSON_PRESS_KEY);

    if (cJSON_IsNumber(temp) && cJSON_IsNumber(hum) && cJSON_IsNumber(press)) {
        float temperature = temp->valuedouble / TEMP_DIVISION_VAL;
        float humidity = hum->valuedouble / HUM_DIVISION_VAL;
        float pressure = press->valuedouble / PRESS_DIVISION_VAL;

        snprintf(mqtt_message.parsed_string, sizeof(mqtt_message.parsed_string), "T:%.1fC H:%.1f%% P:%.fhPa", temperature, humidity, pressure);
    }
}

static uint8_t mqtt_save_data(const char* topic, const char* payload) {
    for(size_t i = 0; i < MQTT_MAX_TRIES_SAVE; i++) {
        if (xSemaphoreTake(mqtt_message.mutex, pdMS_TO_TICKS(MQTT_WAIT_FOR_MUTEX_MS)) == pdTRUE) {
            snprintf(mqtt_message.topic, sizeof(mqtt_message.topic), "%s", topic);
            snprintf(mqtt_message.payload, sizeof(mqtt_message.payload), "%s", payload);
            parse_json_string();

            xSemaphoreGive(mqtt_message.mutex);
            return 0;
        }
    }   

    return 1;
}

/**
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(client, MQTT_TOPIC, MQTT_STANDARD_QOS);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        mqtt_message_t msg = {0};
        snprintf(msg.topic, sizeof(msg.topic), "%.*s", event->topic_len, event->topic);
        snprintf(msg.payload, sizeof(msg.payload), "%.*s", event->data_len, event->data);

        ESP_LOGI(TAG, "Topic received: %s", msg.topic);
        ESP_LOGI(TAG, "Payload received: %s", msg.payload);

        if(xQueueSend(message_queue_handle, &msg, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Queue is full");
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

uint8_t mqtt_open(esp_mqtt_client_handle_t *handle)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = MQTT_SERVER_ADDRESS,
        .broker.address.port = MQTT_MTLS_PORT,
        .broker.address.transport = MQTT_TRANSPORT_OVER_SSL,
        .broker.verification.certificate = (const char *)server_cert_pem_start,
        .credentials = {
            .authentication = {
            .certificate = (const char *)client_cert_pem_start,
            .key = (const char *)client_key_pem_start,
            },
        }
    };

    mqtt_message.mutex = xSemaphoreCreateMutex();

    if (message_queue_handle == NULL) {
        message_queue_handle = xQueueCreate(10, sizeof(mqtt_message));
        if(message_queue_handle == NULL) {
            ESP_LOGE(TAG, "Failed to create queue handle");
            return 1;
        }
    }

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    *handle = esp_mqtt_client_init(&mqtt_cfg);
    
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    if (esp_mqtt_client_register_event(*handle, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register mqtt_event_handler");
        return 1;
    }

    if (esp_mqtt_client_start(*handle)!= ESP_OK) {
        ESP_LOGE(TAG, "Failed to start the mqtt client");
        return 1;
    }

    return 0;
}

char* mqtt_get_parsed_string() {
    if (xSemaphoreTake(mqtt_message.mutex, pdMS_TO_TICKS(MQTT_WAIT_FOR_MUTEX_MS)) == pdTRUE) {
        char* s = mqtt_message.parsed_string;
        xSemaphoreGive(mqtt_message.mutex);
        return s;
    }

    return NULL;
}

void mqtt_main_loop(void *pvParam) {
    mqtt_message_t msg;

    while(1) {
        if (xQueueReceive(message_queue_handle, &msg, portMAX_DELAY) == pdTRUE) {
            mqtt_save_data(msg.topic, msg.payload);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}