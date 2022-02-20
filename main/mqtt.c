/* mqtt.c ---                                                     -*- mode:C-*-
 * Part of IoT library for ESP32 and ESP8266
 * 
 * Copyright (C) 2022, Rytis Paškauskas <rytis.paskauskas@gmail.com>
 *
 * Author: Rytis Paškauskas
 * URL: tbd
 * Created: 2022-02-18
 * Last modified: 2022-02-18 20:19:38 (CET) +0100
 *
 */

#include <esp_log.h>
#include <mqtt_client.h>
#include "sensor.h"

extern const uint8_t broker_crt_start[]   asm("_binary_broker_crt_start");
extern const uint8_t broker_crt_end[]   asm("_binary_broker_crt_end");
static const char *TAG = "mqtt";
static void mqtt_generic_client_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data);

void mqtt_ssl_client_start(void)
{
    const esp_mqtt_client_config_t cfg = {
        .uri = CONFIG_MQTT_BROKER_URI,
        .cert_pem = (const char *)broker_crt_start,
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_generic_client_event_handler,NULL);
    esp_mqtt_client_start(client);
}

static void mqtt_generic_client_event_handler(void *args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
	sensor_init();
	sensor_deploy(event->client);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
	ESP_LOGI(TAG, "SHT3x measurements stop.");
	sensor_deinit();
        break;

    case MQTT_EVENT_SUBSCRIBED:
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
#if !defined(CONFIG_IDF_TARGET_ESP8266)
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
#endif
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

