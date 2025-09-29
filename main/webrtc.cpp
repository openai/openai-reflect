#include "cJSON.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstring>

#include "reflect.hpp"

#define TICK_INTERVAL 15
PeerConnection *peer_connection = NULL;

static void on_datachannel_message(char *, size_t, void *, uint16_t) {
}

StaticTask_t send_audio_task_buffer;
void reflect_send_audio_task(void *user_data) {
  auto peer_connection = (PeerConnection *)user_data;
  while (1) {
    int64_t start_us = esp_timer_get_time();
    reflect_send_audio(peer_connection);

    int64_t elapsed_us = esp_timer_get_time() - start_us;
    int64_t ms_sleep = TICK_INTERVAL - (elapsed_us / 1000);

    if (ms_sleep > 0) {
      vTaskDelay(pdMS_TO_TICKS(ms_sleep));
    }
  }
}

void reflect_new_peer_connection() {
  PeerConfiguration peer_connection_config = {
      .ice_servers = {},
      .audio_codec = CODEC_OPUS,
      .video_codec = CODEC_NONE,
      .datachannel = DATA_CHANNEL_STRING,
      .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {
        reflect_play_audio(data, size);
      },
      .onvideotrack = NULL,
      .on_request_keyframe = NULL,
      .user_data = NULL,
  };

  peer_connection = peer_connection_create(&peer_connection_config);
  assert(peer_connection != NULL);

  peer_connection_oniceconnectionstatechange(
      peer_connection, [](PeerConnectionState state, void *user_data) -> void {
        if (state == PEER_CONNECTION_CONNECTED) {
          StackType_t *stack_memory = (StackType_t *)heap_caps_malloc(
              30000 * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
          assert(stack_memory != nullptr);
          xTaskCreateStaticPinnedToCore(
              reflect_send_audio_task, "audio_publisher", 30000,
              peer_connection, 7, stack_memory, &send_audio_task_buffer, 0);
        }
      });
  peer_connection_ondatachannel(peer_connection, on_datachannel_message, [](void *) -> void {}, NULL);
}

void reflect_peer_connection_loop() {
  peer_init();
  reflect_new_peer_connection();

  auto offer = peer_connection_create_offer(peer_connection);
  auto answer = (char *)calloc(SDP_BUFFER_SIZE, sizeof(char));

  oai_http_request(offer, answer);
  peer_connection_set_remote_description(peer_connection, answer, SDP_TYPE_ANSWER);

  reflect_set_spin(true);
  send_lifx_set_power(true);

  while (true) {
    peer_connection_loop(peer_connection);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
