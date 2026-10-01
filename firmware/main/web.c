#include "web.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "servo.h"

static const char *TAG = "web";

#define STREAM_BOUNDARY "deskroboframe"

static const char INDEX_HTML[] =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Desk Robo</title><style>"
    "body{font-family:sans-serif;background:#111;color:#eee;margin:0;padding:16px;text-align:center}"
    "img{width:100%;max-width:640px;border-radius:8px;background:#000}"
    "label{display:block;margin:16px auto;max-width:640px;text-align:left}"
    "input{width:100%}</style></head><body>"
    "<h2>Desk Robo</h2><img id=cam>"
    "<label>Pan <span id=pv>90</span><input id=pan type=range min=0 max=180 value=90></label>"
    "<label>Tilt <span id=tv>90</span><input id=tilt type=range min=0 max=180 value=90></label>"
    "<p id=st></p><script>"
    "cam.src=location.protocol+'//'+location.hostname+':81/stream';"
    "let t;function send(){clearTimeout(t);t=setTimeout(()=>{"
    "pv.textContent=pan.value;tv.textContent=tilt.value;"
    "fetch('/servo?pan='+pan.value+'&tilt='+tilt.value)},40)}"
    "pan.oninput=tilt.oninput=send;"
    "setInterval(()=>fetch('/status').then(r=>r.json()).then(s=>"
    "st.textContent='heap '+s.heap+' B, PSRAM '+s.psram+' B, up '+s.uptime_s+' s, last reset: '+s.reset),2000);"
    "</script></body></html>";

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return err;
}

static esp_err_t stream_handler(httpd_req_t *req)
{
    char part[96];
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=" STREAM_BOUNDARY);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    int64_t last = esp_timer_get_time();
    for (;;) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "frame capture failed");
            return ESP_FAIL;
        }
        int n = snprintf(part, sizeof(part),
                         "\r\n--" STREAM_BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                         (unsigned)fb->len);
        esp_err_t err = httpd_resp_send_chunk(req, part, n);
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        esp_camera_fb_return(fb);
        if (err != ESP_OK) {
            break;  // viewer closed the page
        }

        int64_t now = esp_timer_get_time();
        ESP_LOGD(TAG, "frame %ums", (unsigned)((now - last) / 1000));
        last = now;
    }
    return ESP_OK;
}

static esp_err_t servo_handler(httpd_req_t *req)
{
    char query[64];
    char value[16];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "pan", value, sizeof(value)) == ESP_OK) {
            servo_set_target(SERVO_PAN, strtof(value, NULL));
        }
        if (httpd_query_key_value(query, "tilt", value, sizeof(value)) == ESP_OK) {
            servo_set_target(SERVO_TILT, strtof(value, NULL));
        }
    }

    char body[64];
    snprintf(body, sizeof(body), "{\"pan\":%.1f,\"tilt\":%.1f}",
             servo_get_position(SERVO_PAN), servo_get_position(SERVO_TILT));
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static const char *reset_reason_str(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_SW: return "software";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    default: return "other";
    }
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char body[160];
    snprintf(body, sizeof(body),
             "{\"heap\":%u,\"psram\":%u,\"uptime_s\":%u,\"reset\":\"%s\"}",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)(esp_timer_get_time() / 1000000),
             reset_reason_str(esp_reset_reason()));
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

esp_err_t web_start(void)
{
    httpd_handle_t control = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    ESP_ERROR_CHECK(httpd_start(&control, &config));

    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_handler},
        {.uri = "/capture", .method = HTTP_GET, .handler = capture_handler},
        {.uri = "/servo", .method = HTTP_GET, .handler = servo_handler},
        {.uri = "/status", .method = HTTP_GET, .handler = status_handler},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(control, &routes[i]);
    }

    httpd_handle_t stream = NULL;
    httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
    stream_config.server_port = 81;
    stream_config.ctrl_port = 32769;
    stream_config.max_open_sockets = 3;
    ESP_ERROR_CHECK(httpd_start(&stream, &stream_config));
    const httpd_uri_t stream_route = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler};
    httpd_register_uri_handler(stream, &stream_route);

    ESP_LOGI(TAG, "control on :80, stream on :81/stream");
    return ESP_OK;
}
