/* sensor.c --- IoT library for ESP32 and ESP8266  -*- mode:C; -*-
 * 
 * Copyright (C) 2022, Rytis Paškauskas <rytis.paskauskas@gmail.com>
 *
 * Author: Rytis Paškauskas
 * URL: https://github.com/rytis-paskauskas
 * Created: 2022-02-18
 * Last modified: 2022-02-18 20:21:14 (CET) +0100
 *
 * TODO: 
 * - move CONFIG_IDF_TARGET_ESP8266 and CONFIG_IDF_TARGET_ESP32 into Kconfig
 * - move i2c pin definitions into Kconfig
 *
 */
#include <string.h>		/* memset */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <mqtt_client.h>
#include "sht3x.h"

/* hardware I2C pins */
#define ADDR SHT3X_I2C_ADDR_GND
/* #define ADDR SHT3X_I2C_ADDR_VDD */

#if defined(CONFIG_IDF_TARGET_ESP8266)
#define SDA_GPIO GPIO_NUM_4
#define SCL_GPIO GPIO_NUM_5
#else  /* ESP32 */
#define SCL_GPIO   GPIO_NUM_22
#define SDA_GPIO   GPIO_NUM_21
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S2)
#define APP_CPU_NUM PRO_CPU_NUM
#endif

static sht3x_t      sensor_dev;
static TaskHandle_t sensor_tsk_h;
static const TickType_t plus_delay = pdMS_TO_TICKS(CONFIG_MEASUREMENT_REFRESH_PERIOD*1000);
static const char *TAG = "sensor";
static void vSensorMeasureAndBroadcast(void *pvParameters);

void sensor_init()
{
    gpio_config_t pin_conf =
	{
	    .pin_bit_mask = (1ULL << SDA_GPIO | 1ULL << SCL_GPIO), 
	    .mode = GPIO_MODE_INPUT,
	    .pull_up_en = GPIO_PULLUP_ENABLE,
	    .pull_down_en = GPIO_PULLDOWN_DISABLE,
	    .intr_type = GPIO_INTR_DISABLE
	};
    ESP_ERROR_CHECK(gpio_config(&pin_conf));
    ESP_ERROR_CHECK(gpio_set_direction(SCL_GPIO,GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(i2cdev_init());
    memset(&sensor_dev, 0, sizeof sensor_dev);
    ESP_ERROR_CHECK(sht3x_init_desc(&sensor_dev, 0, ADDR, SDA_GPIO, SCL_GPIO));
    ESP_ERROR_CHECK(sht3x_init(&sensor_dev));
}

void sensor_deinit()
{
    if(sensor_tsk_h!= NULL)
    {
	vTaskDelete(sensor_tsk_h);
    }
}

void sensor_deploy(void*client)
{
    xTaskCreatePinnedToCore(vSensorMeasureAndBroadcast, "sensor", configMINIMAL_STACK_SIZE * 8, client, 5, &sensor_tsk_h, APP_CPU_NUM);
    configASSERT(sensor_tsk_h);
}

static void vSensorMeasureAndBroadcast(void *pvParameters)
{
    TickType_t now;
    int msg_id = 0;
    float tmpf =0.0, hmdf =0.0;
#if defined(CONFIG_IDF_TARGET_ESP8266)
    /* We don't have floating point operations */
    int tmpi;
    unsigned int hmdi;
#endif
    esp_mqtt_client_handle_t mqtt_client = (esp_mqtt_client_handle_t) pvParameters;
    /* Device's mac address is appended to the topic header to distinguish among multiple devices. 
       So if defaults are used, then the topics will be of the form sense/sht3x/aa:bb:cc:dd:ee::ff */
    /* Since the topic is composed of topic header + "/" + macstr, this is the exact size of it*/
    char *topic = malloc((strlen(CONFIG_MQTT_CLIENT_TOPIC_HEADER) + 19)*sizeof topic);
    if (topic==NULL)
    {
	/* ESP_LOGE(TAG, "out of memory"); */
	abort();
    }
    /* The payload is 
       {"measurement":{"temperature":-99.99,"humidity":100.00}} 
       size=57 bytes
       But since the compiler complained, I've made it a bit larger
    */
    char payload[68]; 		
    char macstr[18];
    uint8_t *macaddr = malloc(6*sizeof macaddr);
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(macaddr));
   
    for(int i=0;i!=6;i++)
    {
	sprintf(macstr + 3*i,"%.2x%c",macaddr[i],(i==5?'\0':':'));
    }
    sprintf(topic, CONFIG_MQTT_CLIENT_TOPIC_HEADER "/%s",macstr);
    now = xTaskGetTickCount();
    for(;;)
    {
        ESP_ERROR_CHECK(sht3x_measure(&sensor_dev, &tmpf, &hmdf));
#if defined(CONFIG_IDF_TARGET_ESP8266) /* no floating point in stdlib */
	tmpi = 100*tmpf;
	hmdi = 100*hmdf;
	sprintf(payload, "{\"measurement\":{\"temperature\":%d.%.2u,\"humidity\":%u.%.2u}}", tmpi/100,abs(tmpi)%100,hmdi/100,hmdi%100);
#else
	sprintf(payload, "{\"measurement\":{\"temperature\":%5.2f,\"humidity\":%5.2f}}", tmpf, hmdf);
#endif
        msg_id =esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        vTaskDelayUntil(&now, plus_delay);
    }
}
