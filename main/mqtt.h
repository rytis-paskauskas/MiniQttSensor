/* mqtt.c ---                                                     -*- mode:C-*-
 * Part of IoT library for ESP32 and ESP8266
 * 
 * Copyright (C) 2022, Rytis Paškauskas <rytis.paskauskas@gmail.com>
 *
 * Author: Rytis Paškauskas
 * URL: tbd
 * Created: 2022-02-18
 * Last modified: 2022-02-18 13:16:15 (CET) +0100
 *
 * High-level API for bringing up a single MQTT client.
 * This API initializes the MQTT event handler. See ESP IDF documentation for details.
 */
#ifndef mqtt_h_defined
#define mqtt_h_defined

void mqtt_ssl_client_start(void);

#endif /* mqtt_h_defined */
