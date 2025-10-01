#include "cJSON.h"
#include <vector>
#include <string>

#include "reflect.hpp"

#define LOG_TAG "realtimeapi"

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

void set_required_parameters(cJSON *parameters, std::vector<std::string> params) {
  cJSON* required = cJSON_AddArrayToObject(parameters, "required");
  assert(required != nullptr);

  for (auto param : params) {
    auto val = cJSON_CreateString(param.c_str());
    assert(val != nullptr);

    assert(cJSON_AddItemToArray(required, val));
  }
}

void add_duration_parameter(cJSON* properties, std::string description, int defaultValue, int minimum, int maximum) {
  cJSON* duration = cJSON_AddObjectToObject(properties, "duration");
  assert(duration != nullptr);

  assert(cJSON_AddStringToObject(duration, "type", "number") != nullptr);
  assert(cJSON_AddStringToObject(duration, "description", description.c_str()) != nullptr);
  assert(cJSON_AddNumberToObject(duration, "default", defaultValue) != nullptr);
  assert(cJSON_AddNumberToObject(duration, "minimum", minimum) != nullptr);
  assert(cJSON_AddNumberToObject(duration, "default", maximum) != nullptr);
}

void add_set_light_power(cJSON *tools) {
  auto tool = cJSON_CreateObject();
  assert(tool != nullptr);

  assert(cJSON_AddStringToObject(tool, "type", "function") != nullptr);
  assert(cJSON_AddStringToObject(tool, "name", "lifx_lan.set_light_power") != nullptr);
  assert(cJSON_AddStringToObject(tool, "description", "LAN SetLightPower (117). Turn on/off with optional fade.") != nullptr);

  auto parameters = cJSON_CreateObject();
  assert(parameters != nullptr);
  assert(cJSON_AddItemToObject(tool, "parameters", parameters));
  assert(cJSON_AddStringToObject(parameters, "type", "object") != nullptr);

  auto properties = cJSON_AddObjectToObject(parameters, "properties");
  assert(properties != nullptr);

  auto on = cJSON_AddObjectToObject(properties, "on");
  assert(on != nullptr);
  assert(cJSON_AddStringToObject(on, "type", "boolean") != nullptr);
  assert(cJSON_AddStringToObject(on, "description", "true=on, false=off") != nullptr);

  add_duration_parameter(properties, "duration of transition in milliseconds", 0, 0, 1000);

  set_required_parameters(parameters, std::vector<std::string>{"on", "duration"});
  assert(cJSON_AddItemToArray(tools, tool));
}

void send_session_update(PeerConnection *peer_connection) {
  auto root = cJSON_CreateObject();
  assert(root != nullptr);

  assert(cJSON_AddStringToObject(root, "type", "session.update") != nullptr);

  auto session = cJSON_CreateObject();
  assert(session != nullptr);

  assert(cJSON_AddStringToObject(session, "instructions", kLunaInstructions) != nullptr);
  assert(cJSON_AddStringToObject(session, "type", "realtime") != nullptr);

  auto tools = cJSON_AddArrayToObject(session, "tools");
  assert(tools != nullptr);

  add_set_light_power(tools);

  assert(cJSON_AddItemToObject(root, "session", session));

  auto serialized = cJSON_PrintUnformatted(root);
  assert(serialized != nullptr);

  peer_connection_datachannel_send(peer_connection, serialized,
                                   strlen(serialized));
  cJSON_free(serialized);
  cJSON_Delete(root);
}
