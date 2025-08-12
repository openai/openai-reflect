#include "esp_http_server.h"

#define SDP_BUFFER_SIZE 5000

void reflect_new_peer_connection(char *offer, char *answer);

extern const char html_page[] asm("_binary_index_html_start");

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
