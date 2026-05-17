//
// Created by k0921 on 2026/3/20.
//

#include "web_sever.h"
#include "esp_log.h"
#include <esp_err.h>
#include <esp_http_server.h>

#define TAG "web_sever"

//网页
static const char* http_html = NULL;

//websocket接收数据回调函数
static ws_receive_cb ws_receive_fn = NULL;

static httpd_handle_t sever_handle;

static int client_fds = -1;

esp_err_t get_http_req(httpd_req_t *req) {
    return httpd_resp_send(req, http_html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handle_ws_req(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        client_fds = httpd_req_to_sockfd(req);
        return ESP_OK;
    }

    httpd_ws_frame_t pkt;
    esp_err_t ret;
    memset(&pkt, 0, sizeof(pkt));
    ret = httpd_ws_recv_frame(req, &pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t *buf = (uint8_t *)malloc(pkt.len + 1);
    if (buf == NULL) {
        return ESP_FAIL;
    }
    pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &pkt, pkt.len);
    if (ret == ESP_OK) {
        if (pkt.type == HTTPD_WS_TYPE_TEXT) {
            ESP_LOGI(TAG, "Get websocket message:%s", pkt.payload);
            if (ws_receive_fn) {
                ws_receive_fn(pkt.payload, pkt.len);
            }
        }
    }
    free(buf);
    return ret;

}

esp_err_t web_ws_start(ws_cfg_t * cfg) {
    if (cfg == NULL) {
        return ESP_FAIL;
    }
    http_html = cfg->html_code;
    ws_receive_fn = cfg->receive_fn;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&sever_handle, &config);
    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_http_req,
    };
    httpd_register_uri_handler(sever_handle, &uri_get);

    httpd_uri_t uri_ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handle_ws_req,
        .is_websocket = true,
    };
    httpd_register_uri_handler(sever_handle, &uri_ws);

    return ESP_OK;
}

esp_err_t web_ws_stop(void) {
    if (sever_handle) {
        httpd_stop(sever_handle);
        sever_handle = NULL;
    }
    return ESP_OK;
}

esp_err_t web_ws_send(uint8_t *data, int len) {
    httpd_ws_frame_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.payload = data;
    pkt.len = len;
    pkt.type = HTTPD_WS_TYPE_TEXT;
    return httpd_ws_send_data(sever_handle, client_fds, &pkt);
}