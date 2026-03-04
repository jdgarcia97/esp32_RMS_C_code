#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include "mqtt_client.h"
#include "heartbeat.h"
#include "wifi.h"
#include "mqtt.h"



#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "RMS";

/* GPTimer ISR routine */
static bool IRAM_ATTR timer_isr_callback(gptimer_handle_t timer,
                                         const gptimer_alarm_event_data_t *edata,
                                         void *user_ctx)
{
    // Minimal work only
    // For now just increment a counter

    volatile uint32_t *counter = (uint32_t *)user_ctx;
    (*counter)++;

    return false;  // no context switch requested
}

void start_10khz_timer(uint32_t *isr_count)
{

    gptimer_handle_t gptimer = NULL;

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000,  // 1 MHz resolution (1 tick = 1 us)
    };

    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_isr_callback,
    };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, isr_count));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 100,   // 100 ticks = 100 us
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}

void monitor_task(void *pv)
{
    uint32_t last = 0;

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint32_t now = *((uint32_t *)pv);
        uint32_t diff = now - last;

        ESP_LOGI("MON", "ISR per second: %" PRIu32, diff);
        last = now;
    }
}
/************************************************************
 * Busy Work (not optimized away)
 ************************************************************/
static inline void simulate_work(volatile uint32_t iterations)
{
    volatile uint32_t dummy = 0;
    for (uint32_t i = 0; i < iterations; i++)
    {
        dummy += i;
    }
    (void)dummy;
}

/************************************************************
 * Generic Periodic Task
 ************************************************************/
typedef struct
{
    const char *name;
    uint32_t period_ms;
    uint32_t work_iterations;
    uint32_t core_id;

} task_config_t;

static void periodic_task(void *param)
{
    task_config_t *cfg = (task_config_t *)param;

    const TickType_t period_ticks = pdMS_TO_TICKS(cfg->period_ms);
    const int64_t period_us = (int64_t)cfg->period_ms * 1000;

    TickType_t last_wake = xTaskGetTickCount();

    int64_t expected_release_us = esp_timer_get_time() + period_us;

    int64_t min_jitter = 1000000000;
    int64_t max_jitter = -1000000000;
    int64_t total_jitter= 0;
    uint32_t sample_count = 0;

    ESP_LOGI(cfg->name, "Started");

    while (1)
    {

        /* Wait until scheduled release */
        vTaskDelayUntil(&last_wake, period_ticks);
        int64_t actual_start_us = esp_timer_get_time();
        int64_t jitter = actual_start_us - expected_release_us;

        if(jitter < min_jitter) min_jitter = jitter;
        if(jitter > max_jitter) max_jitter = jitter;

        total_jitter += jitter;
        sample_count++;
        expected_release_us += period_us;   

        simulate_work(cfg->work_iterations);
        if( sample_count % 50 == 0)
        {
            int64_t avg = total_jitter / sample_count;
            ESP_LOGI(cfg->name,
            "Jitter(us): min=%lld max=%lld avg=%lld",min_jitter, max_jitter, avg);
        }
    }
}

/* Task 4 Wifi Scan every 5 seconds. */
void wifi_scan_task(void *pv)
{
    while (1)
    {
        ESP_LOGI("SCAN", "Starting WiFi scan...");

        esp_err_t err = esp_wifi_scan_start(NULL, true);  // blocking scan

        if (err == ESP_OK)
        {
            uint16_t ap_count = 0;
            esp_wifi_scan_get_ap_num(&ap_count);
            ESP_LOGI("SCAN", "Scan done. APs found: %u", ap_count);
        }
        else
        {
            ESP_LOGE("SCAN", "Scan error: %d", err);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));  // every 5 seconds
    }
}
/* Heart beat task. */
static void heartbeat_task(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(5000);
    TickType_t last_wake = xTaskGetTickCount();

    while (1)
    {
        if (g_mqtt_client && g_mqtt_connected)
        {
            esp_mqtt_client_publish(
                g_mqtt_client,
                "esp32/heartbeat",
                "alive",
                0,
                1,
                0
            );

            ESP_LOGI("HEARTBEAT", "Heartbeat sent");
        }

        vTaskDelayUntil(&last_wake, period);
    }
}


/************************************************************
 * Main
 ************************************************************/
void app_main(void)
{
    ESP_LOGI(TAG, "Starting RMS demo...");
    test_function();

    static uint32_t isr_count = 0;
    start_10khz_timer(&isr_count);

    wifi_init();  // WiFi driver runs on Core 0 (high priority system task)

    static task_config_t task1 = {
        .name = "T1_50ms",
        .period_ms = 50,
        .work_iterations = 120000,   // Tune for desired execution time
        .core_id = 1 
    };

    static task_config_t task2 = {
        .name = "T2_100ms",
        .period_ms = 100,
        .work_iterations = 250000,
        .core_id = 1 
    };

    static task_config_t task3 = {
        .name = "T3_200ms",
        .period_ms = 200,
        .work_iterations = 400000,
        .core_id = 0
    };

    static task_config_t task4 = {
        .name = "T4_WIFI_SCAN",
        .period_ms = 5000,
        .work_iterations = 0,
        .core_id = 0 
    };  

    /* RMS priority assignment (shorter period = higher priority) */

    xTaskCreatePinnedToCore(periodic_task,
                            "Task1",
                            4096,
                            &task1,
                            20,
                            NULL,
                            task1.core_id);  // Core 1

    xTaskCreatePinnedToCore(periodic_task,
                            "Task2",
                            4096,
                            &task2,
                            15,
                            NULL,
                            task2.core_id);  // Core 1

    xTaskCreatePinnedToCore(periodic_task,
                            "Task3",
                            4096,
                            &task3,
                            10,
                            NULL,
                            task3.core_id);  // Core 0
                            
    /* xTaskCreatePinnedToCore(wifi_scan_task,
                            "WiFiScanTask",
                            4096,
                            NULL,
                            18,
                            NULL,
                            task4.core_id);  // Core 0 */
        
    xTaskCreatePinnedToCore(
                            monitor_task,
                            "Monitor",
                            4096,
                            &isr_count,
                            5,          // ← THIS is the priority
                            NULL,
                        1
);


    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://192.168.86.23",   // ← Replace with your Debian IP
    };

    g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(g_mqtt_client); 

    xTaskCreatePinnedToCore(
    heartbeat_task,
    "Heartbeat",
    4096,
    NULL,
    2,          // Low priority
    NULL,
    0           // Core 0 (with MQTT)
);

}
