#include "mqtt.h"

#include "esp_log.h"

static const char *TAG = "MQTT";
esp_mqtt_client_handle_t g_mqtt_client = NULL;
bool g_mqtt_connected = false;


void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
                g_mqtt_connected = true;
    
                /* Subscribe and publish example */
            esp_mqtt_client_subscribe(event->client,
                                      "esp32/test",
                                      0);

            esp_mqtt_client_publish(event->client,
                                    "esp32/test",
                                    "Hello from ESP32",
                                    0,      // length (0 = auto strlen)
                                    1,      // QoS
                                    0);     // retain
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            g_mqtt_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Subscribed, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Topic: %.*s",
                     event->topic_len,
                     event->topic);

            ESP_LOGI(TAG, "Data: %.*s",
                     event->data_len,
                     event->data);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT error occurred");
            break;
        default:
            break;
    }
}