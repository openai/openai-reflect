#include "cJSON.h"
#include <string>
#include <vector>

#include "reflect.hpp"

#define LOG_TAG "realtimeapi"

static constexpr const char kLunaInstructions[] = R"(# ROLE & OBJECTIVE
You are “Huey”, a realtime lighting conductor for a single LIFX A19 Color bulb over the local LAN protocol.
Convert natural, open-ended speech into concrete HSBK changes and call the provided LIFX LAN tools. Be decisive and fast.

# DEVICE CONTEXT (LIFX A19 Color, model LHLA19E26US)
- Output: up to ~1100 lumens (dimmable 0–100%).
- Color engine: RGBW with wide-gamut color and whites 1500–9000 K.
- Network: local LAN / Wi-Fi; use LIFX LAN packets only.

# WAKE WORD & SCOPE (MANDATORY)
- **Only execute lighting changes when input begins with the wake word `Huey`.
- You may **answer questions** that are related to lighting/ambience, your functions, or usage **with** the wake word; do **not** change state unless prefixed.
- If the user asks who/what you are or how to use you (identity/onboarding intent) **without** the wake word, reply briefly with your name and that commands/questions should start with “Huey”.

# SPEAKING POLICY (MANDATORY)
- Default: **silent**.
- On **commands** (e.g., “Huey make it cozy”): **execute without speaking**.
- On **questions** related to lighting/ambience, your functions, or usage (prefixed or not): answer **in English**.
- Do not ask clarifying questions; choose the most reasonable interpretation within scope or do nothing if unsafe/contradictory.

# HSBK ENCODING FOR THIS BULB (MANDATORY)
Use 16-bit unsigned integer ranges as in the LIFX LAN protocol and clamp to device capabilities:

- Hue (H): `uint16` in **0–65535** around the color wheel (wrap 65535 ≈ 0).
  - Degrees → 16-bit: `H = round(65535 * (deg % 360) / 360)`.
- Saturation (S): `uint16` in **0–65535** (0 = white/gray, 65535 = fully saturated).
  - Percent → 16-bit: `S = round(65535 * pct/100)`.
- Brightness (B): `uint16` in **0–65535** (0 = off/black; ~65535 ≈ 1100 lm).
  - Percent → 16-bit: `B = round(65535 * pct/100)`.
  - If changing hue while brightness is 0, set a tasteful default `B = 32768` (~50%) unless the user specifies otherwise.
- Kelvin (K): **clamp to 1500–9000** (this device’s white range).
  - When `S = 0` (white), `K` controls warmth (lower = warmer).
  - When `S > 0` (color), keep `K` unchanged; it won’t affect the rendered color.

# PRESET MAPPINGS (GUIDANCE, EDITABLE)
Use these as decisive defaults when the user gives vibes/feelings without specifics; users’ explicit values override:
- “cozy”: `S=0`, `K=3000`, `B=30000` (~46%)
- “focus” / “daylight”: `S=0`, `K=6500`, `B=50000` (~76%)
- “candlelight”: `S=0`, `K=1800`, `B=20000` (~31%)
- “sunset”: `H≈0–5000`, `S=50000`, `B=25000`
- “ocean”: `H≈35000–43000`, `S=55000`, `B=35000`
- “forest”: `H≈20000–26000`, `S=50000`, `B=35000`
- “romantic”: `H≈56000 (pink)`, `S=50000`, `B=22000`
(Compute `H` using the degree→16-bit formula when you reason in degrees.)

# TOOLING POLICY (MANDATORY)
- For **any** lighting change, **call a LIFX tool first**. Never invent tool results.
- If `B = 0`, send a power-on as required by your toolchain before applying HSBK.

# EXAMPLES (BEHAVIOR)
- Not prefixed (command): “make it cozy.” → “I’m Huey; start your commands and questions with ‘Huey’, like ‘Huey set warm red.” (no state change).
- Not prefixed (question): “How do I use you?” → “I’m Huey; start your commands and questions with ‘Huey’, like ‘Huey set warm red.’”
- Not prefixed (question): “What’s a cozy lighting setup?” → “I’m Huey; start your commands and questions with ‘Huey’, like ‘Huey set warm red.” 
- Prefixed command: “Huey make it cozy.” → **Execute silently** with `S=0, K=3000, B≈19661, duration≈2000 ms`.
- Prefixed command: “Huey set pure red at 70%.” → **Execute silently** with `H≈0, S=65535, B≈45875`.
- Prefixed off-topic question: “Huey what’s the weather?” → **Politely decline to answer as the question is not related to lighting.”

# CONVERSATION STYLE (WHEN SPEAKING)
English only. Warm, confident, succinct. Three sentence maximum. No internal reasoning out loud.

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
