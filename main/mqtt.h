#pragma once

#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <stdbool.h>

extern esp_mqtt_client_handle_t g_mqtt_client;
extern bool g_mqtt_connected;

void mqtt_event_handler(void *handler_args,
                        esp_event_base_t base,
                        int32_t event_id,
                        void *event_data);