#include "cJSON.h"
#include <string>
#include <vector>

#include "reflect.hpp"

#define LOG_TAG "realtimeapi"

static constexpr const char kLunaInstructions[] = R"(# ROLE & OBJECTIVE
You are “Bulby,” a realtime lighting conductor for a LIFX bulb over the **local LAN protocol**.
Turn abstract, emotional speech into **concrete HSBK** changes; call tools to send LIFX LAN packets. Be decisive, and fast.

# DEVICE CONTEXT
- A19 Bulb: Up to ~1100 lm
- Audio: full-duplex; confirmations in 1 sentence max.

# HSBK ENCODING (MANDATORY)
- Hue (H): unsigned 16-bit, 0–65535 around the color wheel (wraps at 65535 ≈ 0).
- Saturation (S): unsigned 16-bit, 0–65535 (0 = white/gray, 65535 = fully saturated).
- Brightness (B): unsigned 16-bit, 0–65535 (0 = off/black).
- If changing hue while brightness is 0, set brightness to a tasteful default 32768 unless user specified otherwise.
- Kelvin (K): clamp to device range (typically 1500–9000). When S=0 (white), K determines warmth.

# CONVERSATION STYLE
Warm, confident, succinct. Always use English when responding. Avoid repetition. No internal reasoning out loud.

# WHEN INPUT IS UNCLEAR
If a request is ambiguous (e.g., “relax” at noon vs. midnight), ask **one brief clarifying question**; otherwise choose tasteful defaults and proceed.

# TOOLING POLICY (MANDATORY)
- For any lighting change, **call a LIFX tool first**
- Never invent tool results. If a tool fails, apologize once, state what you tried, and propose a simple next step (e.g., “retry,” “use steady color”).
)";

void set_required_parameters(cJSON *parameters,
                             std::vector<std::string> params) {
  cJSON *required = cJSON_AddArrayToObject(parameters, "required");
  assert(required != nullptr);

  for (auto param : params) {
    auto val = cJSON_CreateString(param.c_str());
    assert(val != nullptr);

    assert(cJSON_AddItemToArray(required, val));
  }
}

void add_number_parameter(cJSON *properties, std::string name,
                          std::string description, int defaultValue,
                          int minimum, int maximum) {
  cJSON *duration = cJSON_AddObjectToObject(properties, name.c_str());
  assert(duration != nullptr);

  assert(cJSON_AddStringToObject(duration, "type", "number") != nullptr);
  assert(cJSON_AddStringToObject(duration, "description",
                                 description.c_str()) != nullptr);
  assert(cJSON_AddNumberToObject(duration, "default", defaultValue) != nullptr);
  assert(cJSON_AddNumberToObject(duration, "minimum", minimum) != nullptr);
  assert(cJSON_AddNumberToObject(duration, "default", maximum) != nullptr);
}

void add_set_light_power(cJSON *tools) {
  auto tool = cJSON_CreateObject();
  assert(tool != nullptr);

  assert(cJSON_AddStringToObject(tool, "type", "function") != nullptr);
  assert(cJSON_AddStringToObject(tool, "name", "set_light_power") != nullptr);
  assert(cJSON_AddStringToObject(
             tool, "description",
             "LAN SetLightPower (117). Turn on/off with optional fade.") !=
         nullptr);

  auto parameters = cJSON_CreateObject();
  assert(parameters != nullptr);
  assert(cJSON_AddItemToObject(tool, "parameters", parameters));
  assert(cJSON_AddStringToObject(parameters, "type", "object") != nullptr);

  auto properties = cJSON_AddObjectToObject(parameters, "properties");
  assert(properties != nullptr);

  auto on = cJSON_AddObjectToObject(properties, "on");
  assert(on != nullptr);
  assert(cJSON_AddStringToObject(on, "type", "boolean") != nullptr);
  assert(cJSON_AddStringToObject(on, "description", "true=on, false=off") !=
         nullptr);

  add_number_parameter(properties, "duration",
                       "duration of transition in milliseconds", 0, 0, 1000);

  set_required_parameters(parameters,
                          std::vector<std::string>{"on", "duration"});
  assert(cJSON_AddItemToArray(tools, tool));
}

void add_set_color(cJSON *tools) {
  auto tool = cJSON_CreateObject();
  assert(tool != nullptr);

  assert(cJSON_AddStringToObject(tool, "type", "function") != nullptr);
  assert(cJSON_AddStringToObject(tool, "name", "set_color") != nullptr);
  assert(cJSON_AddStringToObject(
             tool, "description",
             "LAN SetColor (102). Set HSBK for whole device.") != nullptr);

  auto parameters = cJSON_CreateObject();
  assert(parameters != nullptr);
  assert(cJSON_AddItemToObject(tool, "parameters", parameters));
  assert(cJSON_AddStringToObject(parameters, "type", "object") != nullptr);

  auto properties = cJSON_AddObjectToObject(parameters, "properties");
  assert(properties != nullptr);

  add_number_parameter(properties, "hue", "", 0, 0, 65535);
  add_number_parameter(properties, "saturation", "", 0, 0, 65535);
  add_number_parameter(properties, "brightness", "", 0, 0, 65535);
  add_number_parameter(properties, "kelvin", "", 0, 0, 65535);
  add_number_parameter(properties, "duration", "", 0, 0, 4294967295);

  set_required_parameters(
      parameters, std::vector<std::string>{"hue", "saturation", "brightness",
                                           "kelvin", "duration"});
  assert(cJSON_AddItemToArray(tools, tool));
}

void send_session_update(PeerConnection *peer_connection) {
  auto root = cJSON_CreateObject();
  assert(root != nullptr);

  assert(cJSON_AddStringToObject(root, "type", "session.update") != nullptr);

  auto session = cJSON_CreateObject();
  assert(session != nullptr);

  assert(cJSON_AddStringToObject(session, "instructions", kLunaInstructions) !=
         nullptr);
  assert(cJSON_AddStringToObject(session, "type", "realtime") != nullptr);

  auto tools = cJSON_AddArrayToObject(session, "tools");
  assert(tools != nullptr);

  add_set_light_power(tools);
  add_set_color(tools);

  assert(cJSON_AddItemToObject(root, "session", session));

  auto serialized = cJSON_PrintUnformatted(root);
  assert(serialized != nullptr);

  peer_connection_datachannel_send(peer_connection, serialized,
                                   strlen(serialized));
  cJSON_free(serialized);
  cJSON_Delete(root);
}

void realtimeapi_parse_incoming(char *msg) {
  // Large inbound messages get chunked (and fail to parse)
  auto root = cJSON_Parse(msg);
  if (root == nullptr) {
    return;
  }

  auto type_item = cJSON_GetObjectItem(root, "type");
  if (type_item == nullptr) {
    return;
  }

  if (strcmp(type_item->valuestring, "response.function_call_arguments.done") !=
      0) {
    return;
  }

  auto argsString = cJSON_GetObjectItem(root, "arguments");
  assert(cJSON_IsString(argsString));

  auto args = cJSON_Parse(argsString->valuestring);
  assert(cJSON_IsObject(args));

  uint16_t hue = 0;
  uint16_t saturation = 0;
  uint16_t brightness = 0;
  uint16_t kelvin = 0;
  uint32_t duration = 0;
  bool on = false;

  auto hueObj = cJSON_GetObjectItem(args, "hue");
  if (hueObj != nullptr) {
    hue = hueObj->valueint;
  }

  auto saturationObj = cJSON_GetObjectItem(args, "saturation");
  if (saturationObj != nullptr) {
    saturation = saturationObj->valueint;
  }

  auto brightnessObj = cJSON_GetObjectItem(args, "brightness");
  if (brightnessObj != nullptr) {
    brightness = brightnessObj->valueint;
  }

  auto kelvinObj = cJSON_GetObjectItem(args, "kelvin");
  if (kelvinObj != nullptr) {
    kelvin = kelvinObj->valueint;
  }

  auto durationObj = cJSON_GetObjectItem(args, "duration");
  if (durationObj != nullptr) {
    duration = durationObj->valueint;
  }

  auto onObj = cJSON_GetObjectItem(args, "on");
  if (onObj != nullptr) {
    on = onObj->type == cJSON_True;
  }

  auto output_name_item = cJSON_GetObjectItem(root, "name");
  assert(cJSON_IsString(output_name_item));

  if (strcmp(output_name_item->valuestring, "set_color") == 0) {
    ESP_LOGI(LOG_TAG,
             "set_color hue(%d) saturation(%d) brightness(%d) kelvin(%d) "
             "duration(%d)",
             hue, saturation, brightness, kelvin, duration);
    send_lifx_set_color(hue, saturation, brightness, kelvin, duration);
  } else if (strcmp(output_name_item->valuestring, "set_light_power") == 0) {
    ESP_LOGI(LOG_TAG, "set_light_power on(%d) duration(%d)", on, duration);
    send_lifx_set_power(on, duration);
  }

  cJSON_Delete(args);
  cJSON_Delete(root);
}
