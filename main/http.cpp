#include "esp_http_server.h"

static constexpr char html_page[] = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Reflect</title>
</head>
<body>
  <p>Hello World</p>
</body>
</html>
)HTML";

esp_err_t root_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

const httpd_uri_t root = {.uri = "/",
                          .method = HTTP_GET,
                          .handler = root_get_handler,
                          .user_ctx = nullptr};

void reflect_http(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(httpd_start(&server, &config));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root));
}
