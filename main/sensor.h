/* sensor.h ---                                                    -*- mode:C; -*-
 * Part of IoT library for ESP32 and ESP8266 
 * 
 * Copyright (C) 2022, Rytis Paškauskas <rytis.paskauskas@gmail.com>
 *
 * Author: Rytis Paškauskas
 * URL: https://github.com/rytis-paskauskas
 * Created: 2022-02-18
 * Last modified: 2022-02-18 13:12:33 (CET) +0100
 *
 * High-level abstractions for sensor resource initialization,
 * de-initialization, and for running a task that makes measurements
 * periodically and publishes them to the MQTT broker
 * 
 * Uses Kconfig parameters:
 * -
 */
#ifndef sensor_h_defined
#define sensor_h_defined
#include <freertos/task.h>
#include "sht3x.h"

extern sht3x_t sensor_dev;
extern TaskHandle_t sensor_tsk_h;

void sensor_init();
void sensor_deinit();
void sensor_deploy(void*client);

#endif /* sensor_h_defined */
