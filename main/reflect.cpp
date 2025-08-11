#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#define TAG "reflect"

void reflect_display();
void reflect_wifi();
void reflect_http();
void reflect_peer_connection_loop();

extern "C" void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  reflect_display();
  reflect_wifi();
  reflect_http();
  reflect_peer_connection_loop();
}
