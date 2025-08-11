#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstring>

#include <opus.h>
#include <peer.h>

static SemaphoreHandle_t lock;
PeerConnection *peer_connection = NULL;
#define TICK_INTERVAL 15

static void onicecandidate_task(char *description, void *user_data) {
  strcpy((char *)user_data, description);
}

void reflect_new_peer_connection(char *offer, char *answer) {
  PeerConfiguration peer_connection_config = {
      .ice_servers = {},
      .audio_codec = CODEC_OPUS,
      .video_codec = CODEC_NONE,
      .datachannel = DATA_CHANNEL_STRING,
      .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {},
      .onvideotrack = NULL,
      .on_request_keyframe = NULL,
      .user_data = answer,
  };

  assert(xSemaphoreTake(lock, pdMS_TO_TICKS(portMAX_DELAY)) == pdTRUE);
  if (peer_connection != NULL) {
    peer_connection_close(peer_connection);
  }

  peer_connection = peer_connection_create(&peer_connection_config);
  assert(peer_connection != NULL);
  xSemaphoreGive(lock);

  peer_connection_onicecandidate(peer_connection, onicecandidate_task);
  peer_connection_oniceconnectionstatechange(
      peer_connection,
      [](PeerConnectionState state, void *user_data) -> void {});
  peer_connection_set_remote_description(peer_connection, offer);

  while (strlen(answer) == 0) {
    vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL));
  }
}

void reflect_peer_connection_loop() {
  peer_init();
  lock = xSemaphoreCreateMutex();

  while (1) {
    if (xSemaphoreTake(lock, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (peer_connection != nullptr) {
        peer_connection_loop(peer_connection);
      }

      xSemaphoreGive(lock);
    }
    vTaskDelay(pdMS_TO_TICKS(TICK_INTERVAL));
  }
}
