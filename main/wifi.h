/* wifi.h --- IoT library for ESP32 and ESP8266 -*- mode:C; -*-
 * 
 * Copyright (C) 2022, Rytis Paškauskas <rytis.paskauskas@gmail.com>
 *
 * Author: Rytis Paškauskas
 * URL: https://github.com/rytis-paskauskas
 * Created: 2022-02-18
 * Last modified: 2022-02-18 18:45:07 (CET) +0100
 *
 *
 */
#ifndef wifi_h_defined
#define wifi_h_defined

void wifi_init_basic();
void wifi_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void *event_data);


#endif /* wifi_h_defined */
