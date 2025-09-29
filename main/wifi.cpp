#include "esp_wifi.h"
#include "esp_log.h"
#include <cstring>

#define LOG_TAG "wifi"

static bool g_wifi_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  static int s_retry_num = 0;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 5) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(LOG_TAG, "retry to connect to the AP");
    }
    ESP_LOGI(LOG_TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(LOG_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    g_wifi_connected = true;
  }
}

void reflect_wifi(void) {
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler, NULL));

  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(LOG_TAG, "Connecting to WiFi SSID: %s", CONFIG_WIFI_NAME);
  wifi_config_t wifi_config;
  memset(&wifi_config, 0, sizeof(wifi_config));
  strncpy((char *)wifi_config.sta.ssid, (char *)CONFIG_WIFI_NAME,
          sizeof(wifi_config.sta.ssid));
  strncpy((char *)wifi_config.sta.password, (char *)CONFIG_WIFI_PASSWORD,
          sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_config(
      static_cast<wifi_interface_t>(ESP_IF_WIFI_STA), &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_connect());

  // block until we get an IP address
  while (!g_wifi_connected) {
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
