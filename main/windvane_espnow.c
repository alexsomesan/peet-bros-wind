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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "windvane_espnow.h"
#include "peetbros.h"

void app_main()
{
    printf("%s\nDirection Filter: %f\n", VERSION, filterGain);

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

    xTaskCreate(loop, "main loop", 4*1024, NULL, 2, NULL);
}
