#include "esp_wifi.h"
#include "esp_log.h"    

#define WIFI_TAG "wifi"

static void wifi_event_handler(void* arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void* event_data);

void wifi_init(void);