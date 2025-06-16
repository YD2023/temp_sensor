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
#include "freertos/ringbuf.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "yt_ssd1306.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"



static const char *TAG = "DHT";


//mini struct forsensor readings
typedef struct {
    float temperature;
    float humidity;
} sensor_data_t;

// freeRTOS handles
static QueueHandle_t sensorQueue;
static RingbufHandle_t sensorRingBuf;
static SemaphoreHandle_t dataSemaphore;
static EventGroupHandle_t initEventGroup;

#define INIT_BIT_QUEUE (1 << 0)
#define INIT_BIT_RINGBUF (1 << 1)
#define INIT_BIT_SEMAPHORE (1 << 2)
#define INIT_BIT_BLE (1 << 3)

// BLE advertising data: Eddystone TLM only
static uint8_t adv_data[] = {
    0x02, 0x01, 0x06,              // flags
    0x03, 0x03, 0xAA, 0xFE,        // complete List of 16-bit Service UUIDs: Eddystone
    0x11, 0x16, 0xAA, 0xFE,        // service Data: length=17, type=0x16, UUID=0xFEAA
    0x20,                          // frame Type: TLM
    0x00,                          // version
    0x00, 0x00,                    // battery voltage (n/a)
    0x00, 0x00,                    // temperature (changes)
    0x00, 0x00, 0x00, 0x00,        // n/a
    0x00, 0x00, 0x00, 0x00         // n/a
};

// scan response data to show unique device name in rFConnect
static uint8_t scan_rsp_data[] = {
    0x0B, 0x09, 'T', 'e', 'm', 'p', 'S', 'e', 'n', 's', 'o', 'r'
};

static const char *ble_tag = "BLE_BEACON";
static uint8_t own_addr_type;

// BLE event callback GAP 
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(ble_tag, "Advertising complete, restarting");
            struct ble_gap_adv_params adv_params;
            memset(&adv_params, 0, sizeof(adv_params));
            adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
            adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
            ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
            break;
        default:
            break;
    }
    return 0;
}

// call BLE sync callback when stack is ready
static void ble_app_on_sync(void) {
    int rc;
    
    ESP_LOGI(ble_tag, "BLE stack synchronized");
    
    // guess the device address type
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(ble_tag, "Failed to infer address type; rc=%d", rc);
        return;
    }
    
    // set advertisement data
    rc = ble_gap_adv_set_data(adv_data, sizeof(adv_data));
    if (rc != 0) {
        ESP_LOGE(ble_tag, "Failed to set advertisement data; rc=%d", rc);
        return;
    }
    
    // set scan response data
    rc = ble_gap_adv_rsp_set_data(scan_rsp_data, sizeof(scan_rsp_data));
    if (rc != 0) {
        ESP_LOGE(ble_tag, "Failed to set scan response data; rc=%d", rc);
        return;
    }
    
    // start advertising with proper parameters
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(ble_tag, "Failed to start advertising; rc=%d", rc);
        return;
    }
    
    ESP_LOGI(ble_tag, "Advertising started successfully");
}

// BLE reset callback
static void ble_app_on_reset(int reason) {
    ESP_LOGE(ble_tag, "Resetting state; reason=%d", reason);
}

// update the BLE advertisement with temperature
static void update_ble_advertisement(float temperature) {
    // Convert temperature to fixed-point format (multiply by 256)
    int16_t temp_fixed = (int16_t)(temperature * 256.0f);
    
    adv_data[13] = (temp_fixed >> 8) & 0xFF;  // High byte
    adv_data[14] = temp_fixed & 0xFF;         // Low byte
    
    // end advertising
    ble_gap_adv_stop();
    
    // update advertisement data
    int rc = ble_gap_adv_set_data(adv_data, sizeof(adv_data));
    if (rc != 0) {
        ESP_LOGE(ble_tag, "Failed to update advertisement data; rc=%d", rc);
        return;
    }
    
    // Set scan response data
    rc = ble_gap_adv_rsp_set_data(scan_rsp_data, sizeof(scan_rsp_data));
    if (rc != 0) {
        ESP_LOGE(ble_tag, "Failed to set scan response data; rc=%d", rc);
        return;
    }
    
    // Restart advertising with proper parameters
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(ble_tag, "Failed to restart advertising; rc=%d", rc);
        return;
    }
}

void DHT_task(void *pvParameter) {
    sensor_data_t sensor_data;
    
    // Wait for initialization to complete
    xEventGroupWaitBits(initEventGroup, INIT_BIT_QUEUE, pdFALSE, pdTRUE, portMAX_DELAY);
    
    setDHTgpio(GPIO_NUM_4);
    ESP_LOGI(TAG, "Starting DHT Task\n\n");
    
    while(1) {
        ESP_LOGI(TAG, "=== Reading DHT ===\n");
        int ret = readDHT();
        errorHandler(ret);

        // get readings
        sensor_data.temperature = getTemperature();
        sensor_data.temperature = (sensor_data.temperature * 1.8) + 32;
        sensor_data.humidity = getHumidity();
        
        // send data to queue
        if (xQueueSend(sensorQueue, &sensor_data, pdMS_TO_TICKS(100)) != pdPASS) {
            ESP_LOGE(TAG, "Failed to send data to queue");
        }
        
        ESP_LOGI(TAG, "Hum: %.1f Tmp: %.1f\n", sensor_data.humidity, sensor_data.temperature);
        
        // wait 1 second before next reading
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void process_display_task(void *pvParameter) {
    sensor_data_t sensor_data;
    
    // Wait for initialization to complete
    xEventGroupWaitBits(initEventGroup, INIT_BIT_QUEUE | INIT_BIT_RINGBUF | INIT_BIT_SEMAPHORE, 
                       pdFALSE, pdTRUE, portMAX_DELAY);
    
    ESP_LOGI(TAG, "Starting Process & Display Task\n");
    
    while(1) {
        // wait for new data from queue
        if (xQueueReceive(sensorQueue, &sensor_data, portMAX_DELAY) == pdPASS) {
            // store data in buffer
            BaseType_t ret = xRingbufferSend(sensorRingBuf, &sensor_data, sizeof(sensor_data_t), pdMS_TO_TICKS(100));
            if (ret != pdPASS) {
                ESP_LOGE(TAG, "Failed to store data in ring buffer");
            }
            
            // update display
            char humidity_str[16];
            char temp_str[16];
            
            snprintf(humidity_str, sizeof(humidity_str), "Hum: %.1f", sensor_data.humidity);
            snprintf(temp_str, sizeof(temp_str), "Tmp: %.1f F", sensor_data.temperature);
            
            ssd1306_print_str(18, 0, "Hello 1113!", false);
            ssd1306_print_str(18, 17, humidity_str, false);
            ssd1306_print_str(28, 27, temp_str, false);
            ssd1306_print_str(38, 37, "Have a", false);
            ssd1306_print_str(28, 47, "Good Day!", false);
            
            ssd1306_display();
            
            // signal BLE task that new data is available
            xSemaphoreGive(dataSemaphore);
        }
    }
}

// BLE host task
static void ble_host_task(void *param) {
    ESP_LOGI(ble_tag, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_beacon_task(void *pvParameter) {
    sensor_data_t sensor_data;
    size_t item_size;
    
    // initialization wait time
    xEventGroupWaitBits(initEventGroup, INIT_BIT_QUEUE | INIT_BIT_RINGBUF | INIT_BIT_SEMAPHORE | INIT_BIT_BLE, 
                       pdFALSE, pdTRUE, portMAX_DELAY);
    
    ESP_LOGI(ble_tag, "Starting BLE Beacon Task");
    
    while(1) {
        // wait for new data
        if (xSemaphoreTake(dataSemaphore, portMAX_DELAY) == pdTRUE) {
            // get latest data from ring buffer
            void *data = xRingbufferReceive(sensorRingBuf, &item_size, pdMS_TO_TICKS(100));
            if (data != NULL) {
                memcpy(&sensor_data, data, sizeof(sensor_data_t));
                vRingbufferReturnItem(sensorRingBuf, data);
                
                // update BLE advertisement with new temperature
                update_ble_advertisement(sensor_data.temperature);
                ESP_LOGI(ble_tag, "Updated BLE advertisement with temperature: %.1f°F", sensor_data.temperature);
            }
        }
    }
}

void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // synchronized initialization with an event group
    initEventGroup = xEventGroupCreate();
    if (initEventGroup == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // Initialize display
    init_ssd1306();
    
    // Initialize NimBLE
    nimble_port_init();
    
    // Initialize the NimBLE host configuration with proper callbacks
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.reset_cb = ble_app_on_reset;
    ble_hs_cfg.store_status_cb = NULL;
    
    // Initialize services
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    // Set the default device name
    ble_svc_gap_device_name_set("TempSensor");
    
    // Start the NimBLE host task
    nimble_port_freertos_init(ble_host_task);
    
    // Create ring buffer for storing sensor history
    sensorRingBuf = xRingbufferCreate(1024, RINGBUF_TYPE_BYTEBUF);
    if (sensorRingBuf == NULL) {
        ESP_LOGE(TAG, "Failed to create ring buffer");
        return;
    }
    xEventGroupSetBits(initEventGroup, INIT_BIT_RINGBUF);
    
    // Create semaphore for data ready notification
    dataSemaphore = xSemaphoreCreateBinary();
    if (dataSemaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }
    xEventGroupSetBits(initEventGroup, INIT_BIT_SEMAPHORE);
    
    // Create queue for sensor data
    sensorQueue = xQueueCreate(5, sizeof(sensor_data_t));
    if (sensorQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }
    xEventGroupSetBits(initEventGroup, INIT_BIT_QUEUE);
    
    // Set BLE initialization complete
    xEventGroupSetBits(initEventGroup, INIT_BIT_BLE);

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_rom_gpio_pad_select_gpio(GPIO_NUM_4);

    // Create tasks with appropriate priorities
    xTaskCreatePinnedToCore(DHT_task, "DHT_task", 2048, NULL, 3, NULL, 1);
    xTaskCreate(process_display_task, "process_display_task", 4096, NULL, 4, NULL);
    xTaskCreate(ble_beacon_task, "ble_beacon_task", 4096, NULL, 5, NULL);
}