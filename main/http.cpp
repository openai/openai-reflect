#include "esp_http_server.h"

#define SDP_BUFFER_SIZE 5000

void reflect_new_peer_connection(char *offer, char *answer);

static constexpr char html_page[] = R"HTML(
<html>
<head>
  <meta charset="UTF-8">
  <title>OpenAI Realtime Audio Bridge</title>
  <style>
    body { font-family: sans-serif; padding: 1rem; }
    #log { white-space: pre-wrap; border: 1px solid #ccc; padding: 1rem; height: 300px; overflow-y: scroll; }
  </style>
</head>
<body>
  <h1>Reflect</h1>

  <label>OpenAI API Key:
    <input type="password" id="apiKey" placeholder="sk-..." size="42">
  </label><br/>
  <label>Voice:
    <input type="text" id="voice" value="alloy">
  </label><br/>
  <label>Gain&nbsp;(1-5):
    <input type="number" min="1" max="5" step="1" value="3" id="gainInput">
  </label><br/>
  <button onclick='window.startReflect()'>Start Reflect</button>

  <h2>Status</h2>
  <div id="log"></div>

  <script>
    window.startReflect = async () => {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true })
      const [audioTrack] = stream.getAudioTracks()

      const reflectPeerConnection = new RTCPeerConnection()
      reflectPeerConnection.addTrack(audioTrack, stream)
      reflectPeerConnection.createDataChannel('')

      const offer = await reflectPeerConnection.createOffer({ offerToReceiveAudio: false })
      await reflectPeerConnection.setLocalDescription(offer)

      reflectPeerConnection.onicegatheringstatechange = async () => {
        if (reflectPeerConnection.iceGatheringState === "complete") {
          const offer = reflectPeerConnection.localDescription.sdp.split("\r\n").filter(line => {
            if (!line.includes("a=candidate")) {
              return true
            }

            return line.includes("192.168.4") && line.includes("udp")
          }).join("\r\n")

          const res = await fetch("/connect", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: offer,
          })
          let answer = await res.text()
          answer = answer.replace("BUNDLE audio datachannel", "BUNDLE 0 1")
                         .replace("a=mid:audio", "a=mid:0")
                         .replace("a=mid:datachannel", "a=mid:1")
          await reflectPeerConnection.setRemoteDescription({type: 'answer', sdp: answer})
        }
      }
    }
  </script>
</body>
</html>
)HTML";

esp_err_t root_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t post_handler(httpd_req_t *req) {
  int total_len = req->content_len;
  int cur_len = 0;
  int received = 0;
  auto offer = (char *)calloc(SDP_BUFFER_SIZE, sizeof(char));
  auto answer = (char *)calloc(SDP_BUFFER_SIZE, sizeof(char));

  if (total_len >= SDP_BUFFER_SIZE) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Content too long");
    return ESP_FAIL;
  }

  while (cur_len < total_len) {
    received = httpd_req_recv(req, offer + cur_len, total_len - cur_len);
    if (received <= 0) {
      if (received == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;
      }
      return ESP_FAIL;
    }
    cur_len += received;
  }

  reflect_new_peer_connection(offer, answer);
  httpd_resp_send(req, answer, strlen(answer));
  free(offer);
  free(answer);
  return ESP_OK;
}

static const httpd_uri_t connect_uri = {.uri = "/connect",
                                        .method = HTTP_POST,
                                        .handler = post_handler,
                                        .user_ctx = NULL};

const httpd_uri_t root_uri = {.uri = "/",
                              .method = HTTP_GET,
                              .handler = root_get_handler,
                              .user_ctx = nullptr};

void reflect_http(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(httpd_start(&server, &config));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &connect_uri));
}
