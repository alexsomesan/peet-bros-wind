/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
   This example shows how to use ESPNOW.
   Prepare two device, one for sending ESPNOW data and another for receiving
   ESPNOW data.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <rom/ets_sys.h>
#include "driver/gpio.h"
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "windvane_espnow.h"
#include "peetbros.h"

#define WIFI_CHANNEL 1
esp_now_peer_info_t slave;
static const char *TAG = "Windvane";
const uint8_t remoteMac[] = {0x36, 0x33, 0x33, 0x33, 0x33, 0x33};
    char windSentence[maxDataFrameSize];

static esp_err_t my_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "WiFi started");
        break;
    default:
        break;
    }
    return ESP_OK;
}

void app_main()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    gpio_set_direction(windDirPin, GPIO_MODE_INPUT);
    gpio_set_direction(windSpeedPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(windDirPin, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(windSpeedPin, GPIO_PULLUP_ONLY);
    gpio_pullup_en(windDirPin);
    gpio_pullup_en(windSpeedPin);

    gpio_set_intr_type(windDirPin, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(windSpeedPin, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(windDirPin, readWindSpeed, (void*)windDirPin);
    gpio_isr_handler_add(windSpeedPin, readWindDir, (void*)windSpeedPin);
    gpio_intr_enable(windSpeedPin);
    gpio_intr_enable(windDirPin);

    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(my_event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start());

    memcpy( &slave.peer_addr, &remoteMac, 6 );
    slave.channel = WIFI_CHANNEL;
    slave.encrypt = 0;
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_add_peer(&slave) );

    printf("%s\nDirection Filter: %f\n", VERSION, filterGain);

    xTaskCreate(loop, "main loop", 4*1024, NULL, 2, NULL);
}
