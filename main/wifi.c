/* wifi.c --- IoT library for ESP32 and ESP8266  -*- mode:C; -*-
 * 
 * Copyright (C) 2022, Rytis Paškauskas <rytis.paskauskas@gmail.com>
 *
 * Author: Rytis Paškauskas
 * URL: https://github.com/rytis-paskauskas
 * Created: 2022-02-18
 * Last modified: 2022-02-18 19:37:07 (CET) +0100
 *
 * TODO:
 * - handler disconnect error codes
 * - retry wifi reconnection a few times
 * - warn about low rssi (better do it to a dedicated topic)
 * - power off the device if wifi is stopped (?)
 *
 */
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>

void wifi_init_basic()
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
	    .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    const char* tag = "WIFI EVENT HANDLER";
    ESP_LOGI(tag, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    /* Hardware layer */
    if(base==WIFI_EVENT)
    {
	switch ((wifi_event_t)event_id) {
	case WIFI_EVENT_STA_START:
	    ESP_LOGI(tag, "wifi sta started event");
	    ESP_ERROR_CHECK(esp_wifi_connect());
	    break;
	case WIFI_EVENT_STA_CONNECTED:
	    ESP_LOGI(tag, "wifi sta connected event");
	    ESP_LOGI(tag, "on channel: %d", ((wifi_event_sta_connected_t*)event_data)->channel);
	    ESP_LOGI(tag, "auth mode: %d", ((wifi_event_sta_connected_t*)event_data)->authmode);
	    break;
	case WIFI_EVENT_STA_DISCONNECTED:
	    ESP_LOGI(tag, "wifi sta disconnected event\n");
	    ESP_LOGI(tag, "reason: %d", ((wifi_event_sta_disconnected_t*)event_data)->reason);
	    /* TODO: Wait and retry for a few times */
	    break;
	case WIFI_EVENT_STA_BSS_RSSI_LOW:
	    ESP_LOGI(tag, "wifi RSSI low event\n");
	    /* TODO: Send a warning to MQTT */
	    /* int32_t rssi=((wifi_event_bss_rssi_low_t*)event_data)->rssi; */
	    /* msg_id =esp_mqtt_client_publish(client, topic, payload, 0, 1, 0); */
	    break;
	case WIFI_EVENT_STA_STOP:
	    ESP_LOGI(tag, "wifi sta stop event\n");
	    /* TODO: We should assume the device should be powered off (?) */
	    break;
	default:
	    ESP_LOGI(tag, "unhandled event with id=%d\n", event_id);
	    break;
	}
    }
}
