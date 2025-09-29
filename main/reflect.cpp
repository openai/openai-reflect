#include <esp_err.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>

#include "reflect.hpp"

#define TAG "reflect"

extern "C" void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  reflect_display();
  reflect_audio();
  reflect_wifi();
  reflect_lifx();
  reflect_peer_connection_loop();
}
