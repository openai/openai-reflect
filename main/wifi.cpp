#include "esp_wifi.h"
#include <cstring>

void reflect_wifi(void) {
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

  ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));

  esp_netif_ip_info_t ip;
  ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ip));
  ip.gw.addr = 0;
  ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip));

  ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

  wifi_config_t wifi_config;
  memset(&wifi_config, 0, sizeof(wifi_config));
  strcpy((char *)wifi_config.ap.ssid, "reflect");
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  wifi_config.ap.max_connection = 3;

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}
