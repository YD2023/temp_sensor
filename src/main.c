#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
//#include "ssd1306.h"
//#include "ssd1306_version.h"
#include "DHT.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include "esp_system.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "yt_ssd1306.h"

static const char *TAG = "DHT";
volatile float hum_now = 0;
volatile float temp_now = 0;



void DHT_task(void *pvParameter) {
    setDHTgpio(GPIO_NUM_4);
    ESP_LOGI(TAG, "Starting DHT Task\n\n");
    ESP_LOGI(TAG, "=== Reading DHT ===\n");
    int ret = readDHT();
    errorHandler(ret);

    //ESP_LOGI(TAG, "Hum: %.1f Tmp: %.1f\n", getHumidity(), getTemperature());
    hum_now = getHumidity();
    temp_now = getTemperature();
    // -- wait at least 2 sec before reading again ------------
    // The interval of whole process must be beyond 2 seconds !!
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

void ssd1306_task(void *pvParameter) {
    while(1)
    {   
        char humidity_str[16];
        char temp_str[16];
        
        snprintf(humidity_str, sizeof(humidity_str), "Hum: %.1f", hum_now);
        snprintf(temp_str, sizeof(temp_str), "Tmp: %.1f", temp_now);
        
        ssd1306_print_str(18, 0, "Hello KDC!", false);
        ssd1306_print_str(18, 17, humidity_str, false);
        ssd1306_print_str(28, 27, temp_str, false);
        ssd1306_print_str(38, 37, "Have a", false);
        ssd1306_print_str(28, 47, "Good Day!", false);

        ssd1306_display();
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void app_main() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    init_ssd1306();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_log_level_set("*", ESP_LOG_INFO);

    esp_rom_gpio_pad_select_gpio(GPIO_NUM_4);

    // Create tasks
    xTaskCreate(&DHT_task, "DHT_task", 2048, NULL, 5, NULL);
    xTaskCreate(&ssd1306_task, "ssd1306_task", 4096, NULL, 5, NULL);
}