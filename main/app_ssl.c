/* app_ssl.c --- Basic IoT with encrypted MQTT -*- mode:C; -*-
 * 
 * Copyright (C) 2021-2022, Rytis Paškauskas <rytis.paskauskas@gmail.com>
 *
 * Author: Rytis Paškauskas
 * URL: https://github.com/rytis-paskauskas
 * Created: 2021-05-20 18:11:35 2021 (+0200)
 * Last modified: 2022-02-19 12:31:53 (CET) +0100
 *
 * Basic IoT application for sending sht3x measurements (temperature
 * and humidity) over MQTT with SSL encryption.
 *
 * Tested with esp32 and esp8266 boards.
 * Build it with ESP IDF (esp32) or ESP8266_RTOS_SDK (esp8266)
 * Customize with Kconfig:
 * - idf.py menuconfig (esp32)
 * - make   menuconfig (esp8266)
 * 
 * 
 */
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include "sensor.h"
#include "mqtt.h"
#include "wifi.h"

static void event_handler(void* args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    const char* tag = "MAIN HANDLER";
    ESP_LOGI(tag, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    /* Network layer */
    if(base==IP_EVENT)
    {
	switch((ip_event_t)event_id) {
	case IP_EVENT_STA_GOT_IP:
	    mqtt_ssl_client_start();
	    break;
	default:
	    ESP_LOGI(tag, "IP_EVENT: unhandled event_id: %d", event_id);
	    break;
	}
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_basic();
}
