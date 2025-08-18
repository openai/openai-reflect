#include "cJSON.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstring>

#include <opus.h>
#include <peer.h>

#define TICK_INTERVAL 15
void reflect_play_audio(uint8_t *, size_t);
void reflect_send_audio(PeerConnection *);
void send_lifx_set_power(int);
void send_lifx_set_waveform(uint16_t, uint16_t, uint16_t);
void send_lifx_set_color(uint16_t, uint16_t, uint16_t);
void reflect_set_spin(bool);

PeerConnection *peer_connection = NULL;

static void onicecandidate_task(char *description, void *user_data) {
  strcpy((char *)user_data, description);
}

static void handle_json(char *command, size_t, void *, uint16_t) {
  cJSON *root = cJSON_Parse(command);
  if (root != NULL) {
    cJSON *event = cJSON_GetObjectItemCaseSensitive(root, "event");
    if (event != NULL) {
      if (strcmp("color", event->valuestring) == 0) {
        send_lifx_set_color(
            cJSON_GetObjectItemCaseSensitive(root, "hue")->valueint,
            cJSON_GetObjectItemCaseSensitive(root, "saturation")->valueint,
            cJSON_GetObjectItemCaseSensitive(root, "brightness")->valueint);
      } else if (strcmp("waveform", event->valuestring) == 0) {
        send_lifx_set_waveform(
            cJSON_GetObjectItemCaseSensitive(root, "hue")->valueint,
            cJSON_GetObjectItemCaseSensitive(root, "saturation")->valueint,
            cJSON_GetObjectItemCaseSensitive(root, "brightness")->valueint);
      } else if (strcmp("power", event->valuestring) == 0) {
        send_lifx_set_power(
            cJSON_GetObjectItemCaseSensitive(root, "power")->valueint);
      }
    }
    cJSON_Delete(root);
  }
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

void reflect_new_peer_connection(char *offer, char *answer) {
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
      .user_data = answer,
  };

  peer_connection = peer_connection_create(&peer_connection_config);
  assert(peer_connection != NULL);

  peer_connection_onicecandidate(peer_connection, onicecandidate_task);
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
  peer_connection_set_remote_description(peer_connection, offer);
  peer_connection_ondatachannel(
      peer_connection, handle_json, [](void *) -> void {}, NULL);

  while (strlen(answer) == 0) {
    vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL));
  }
}

void reflect_peer_connection_loop() {
  peer_init();

  for (uint32_t i = 0;; i++) {
    if (peer_connection != nullptr) {
      peer_connection_loop(peer_connection);
    }

    if ((i % 3000) == 0) {
      send_lifx_set_power(peer_connection != nullptr);
      reflect_set_spin(peer_connection != nullptr);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
