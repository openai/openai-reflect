#include "cJSON.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstring>

#include "reflect.hpp"

#define LOG_TAG "webrtc"
#define TICK_INTERVAL 15
PeerConnection *peer_connection = NULL;

static constexpr const char kLunaInstructions[] = R"(# ROLE & OBJECTIVE
You are “Luna,” a realtime lighting conductor for a LIFX Luna lamp over the **local LAN protocol**.
Turn abstract, emotional speech into **concrete HSBK** changes; call tools to send LIFX LAN packets. Be decisive, safe, and fast.

# DEVICE CONTEXT
- LIFX Luna: white spectrum ~1500–9000K, up to ~1000 lm
- Audio: full-duplex; confirmations in 1 sentence max.

# CONVERSATION STYLE
Warm, confident, succinct. Always use English when responding. Avoid repetition. No internal reasoning out loud.

# WHEN INPUT IS UNCLEAR
If a request is ambiguous (e.g., “relax” at noon vs. midnight), ask **one brief clarifying question**; otherwise choose tasteful defaults and proceed.

# TOOLING POLICY (MANDATORY)
- For any lighting change, **call a LIFX tool first**; then:
  1) `ui.say` a short confirmation, and
- Never invent tool results. If a tool fails, apologize once, state what you tried, and propose a simple next step (e.g., “retry,” “use steady color”).

# SAFETY & COMFORT
- **No strobe**. Use periods ≥ 1200 ms for waveforms; prefer SINE or TRIANGLE for soothing effects.
- Night comfort (after 22:00 local): cap brightness at 40% unless the user insists.
- Keep brightness in [6%..85%] unless explicitly asked; kelvin within device range.

# MOOD → LIGHT HEURISTICS
(Adapt by time of day and environment keywords.)
- **Calm/Cozy/Wind-down** → 1700–2700 K, 15–35% brightness; optional SINE breathe (~6–8 s).
- **Focus/Crisp/Energize** → 4200–6500 K, 55–75%, static.
- **Romantic/Gentle** → low-sat peach/rose (hue≈345°, sat≈0.25), 20–35%.

# WHOLE-DEVICE WAVEFORMS
- Use **SetWaveform** (SINE/TRIANGLE/HALF_SINE/PULSE/SAW) for gentle “breathe,” one-shot “sign flicker,” or quick demo beats. Prefer `transient=true`.

# DEFAULTS
- Transition (`duration_ms`): 1500 unless the user asks for faster/slower.
- If the user says a single vibe word, pick a tasteful mapping and go.

# EXAMPLES
User: “Cozy reading nook.”
→ `lifx_lan.set_color {kelvin:2200, brightness:0.30, duration_ms:1500}`
→ Say: “Cozy warm white.”
→ Status: `COZY · 2200K` / `30%`

User: “One gentle sign flicker, then hold.”
→ `lifx_lan.set_waveform {waveform:"PULSE", period_ms:1600, cycles:1, transient:true, h:magenta…}`
→ `lifx_lan.set_color {...steady…}`
→ Say: “Done.”
→ Status: `PULSE · 1×` / `STEADY`
)";

static void on_datachannel_message(char *msg, size_t, void *, uint16_t) {
  ESP_LOGI(LOG_TAG, "DataChannel Message: %s", msg);
}
static void on_datachannel_onopen(void *userdata) {
  if (peer_connection_create_datachannel(peer_connection, DATA_CHANNEL_RELIABLE, 0, 0, (char *)"oai-events", (char *)"") != -1) {
    ESP_LOGI(LOG_TAG, "DataChannel Open");

    auto root = cJSON_CreateObject();
    if (!root) {
      ESP_LOGI(LOG_TAG, "Failed to create root for session.update");
      return;
    }

    if (!cJSON_AddStringToObject(root, "type", "session.update")) {
      ESP_LOGI(LOG_TAG, "Failed to set type for session.update");
      return;
    }

    auto session = cJSON_CreateObject();
    if (!session) {
      ESP_LOGI(LOG_TAG, "Failed to create session object for session.update");
      return;
    }

    if (!cJSON_AddStringToObject(session, "instructions", kLunaInstructions)) {
      ESP_LOGI(LOG_TAG, "Failed to set 'instructions' on session object");
      return;
    }

    if (!cJSON_AddStringToObject(session, "type", "realtime")) {
      ESP_LOGI(LOG_TAG, "Failed to set 'type' on session object");
      return;
    }

    if (!cJSON_AddItemToObject(root, "session", session)) {
      ESP_LOGI(LOG_TAG, "Failed to add session to root object");
      return;
    }

    auto serialized = cJSON_PrintUnformatted(root);
    if (!serialized) {
      ESP_LOGI(LOG_TAG, "Failed to generate serialized session.update");
      return;
    }

    peer_connection_datachannel_send(peer_connection, serialized, strlen(serialized));
    cJSON_free(serialized);
    cJSON_Delete(root);
  } else {
    ESP_LOGI(LOG_TAG, "DataChannel Failed");
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
  peer_connection_ondatachannel(peer_connection, on_datachannel_message, on_datachannel_onopen, NULL);
}

void reflect_peer_connection_loop() {
  vTaskPrioritySet(xTaskGetCurrentTaskHandle(), 10);
  peer_init();
  reflect_new_peer_connection();

  auto offer = peer_connection_create_offer(peer_connection);
  auto answer = (char *)calloc(SDP_BUFFER_SIZE, sizeof(char));

  oai_http_request(offer, answer);
  peer_connection_set_remote_description(peer_connection, answer,
                                         SDP_TYPE_ANSWER);

  reflect_set_spin(true);
  send_lifx_set_power(true);

  while (true) {
    peer_connection_loop(peer_connection);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
