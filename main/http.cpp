#include "esp_http_server.h"
#include <vector>

#define SDP_BUFFER_SIZE 5000

void reflect_new_peer_connection(char *offer, char *answer);

extern const char html_page[] asm("_binary_index_html_start");

esp_err_t root_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t post_handler(httpd_req_t *req) {
  std::vector<char> offer(SDP_BUFFER_SIZE);
  std::vector<char> answer(SDP_BUFFER_SIZE);

  if (req->content_len != httpd_req_recv(req, offer.data(), SDP_BUFFER_SIZE - 1)) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to read POST");
    return ESP_FAIL;
  }

  reflect_new_peer_connection(offer.data(), answer.data());
  httpd_resp_send(req, answer.data(), HTTPD_RESP_USE_STRLEN);
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
