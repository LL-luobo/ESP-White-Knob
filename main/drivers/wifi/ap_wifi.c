//
// Created by k0921 on 2026/3/21.
//

#include "ap_wifi.h"
#include <string.h>
#include <cJSON.h>
#include "my_wifi.h"
#include "esp_spiffs.h"
#include "web_sever.h"
#include "esp_log.h"
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "ap_wifi"

#define SPIFFS_MOUNT    "/spiffs"   //挂载点
#define HTML_PATH       "/spiffs/AP_Connected.html"

#define APCFG_BIT   (BIT0)

static char current_ssid[32];
static char current_password[32];

static char * html_code = NULL;
static EventGroupHandle_t apcfg_ev;

static char *init_web_page_buffer(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_MOUNT,
        .format_if_mount_failed = false,
        .max_files = 5,
        .partition_label = "html",  //分区表名字
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    struct stat st;
    if (stat(HTML_PATH, &st)) {
        ESP_LOGE(TAG, "apcfg.html not found");
        return NULL;
    }

    char *buf = (char *)malloc(st.st_size + 1);
    if (!buf) {
        return NULL;
    }
    memset(buf, 0, st.st_size + 1);

    FILE *fp = fopen(HTML_PATH, "r");
    if (fread(buf, st.st_size, 1, fp) == 0)
    {
        free(buf);
        buf = NULL;
        ESP_LOGE(TAG, "fread failed");
    }
    fclose(fp);
    return buf;
}


static void ap_wifi_task(void * param) {
    EventBits_t ev;
    while (1) {
        ev = xEventGroupWaitBits(apcfg_ev, APCFG_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10 * 1000));
        if (ev & APCFG_BIT) {
            web_ws_stop();
            ESP_LOGI(TAG, "AP stopped and STA wifi connecting...");
            wifi_manager_connect(current_ssid, current_password);
            // vTaskDelete(NULL);
        }
    }
}


// SemaphoreHandle_t wifi_sem = NULL;

void ap_wifi_init(p_wifi_state_cb f) {
    wifi_manager_init(f);
    html_code = init_web_page_buffer();
    apcfg_ev = xEventGroupCreate();//创建事件组
    xTaskCreatePinnedToCore(ap_wifi_task, "ap_wifi_task", 4096, NULL, 2, NULL, 1);
    // wifi_sem = xSemaphoreCreateBinary();

    // ----- 尝试自动连接已保存的网络 -----
    char saved_ssid[32] = {0};
    char saved_pass[64] = {0};
    if (load_wifi_credentials(saved_ssid, sizeof(saved_ssid), saved_pass, sizeof(saved_pass))) {
        ESP_LOGI(TAG, "Try to connect saved WiFi: %s", saved_ssid);
        wifi_manager_connect(saved_ssid, saved_pass);
    }
    else {
        ESP_LOGI(TAG, "No saved WiFi credentials, need AP config");
    }
}


void wifi_scan_handle(int num, wifi_ap_record_t * ap_recoeds) {
    cJSON * root = cJSON_CreateArray();
    // cJSON_AddArrayToObject(root, "wifi_list");
    for (int i = 0; i < num; i++) {
        cJSON * wifi_js = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_js, "ssid", (char *)ap_recoeds[i].ssid);
        cJSON_AddNumberToObject(wifi_js, "rssi", ap_recoeds[i].rssi);
        cJSON_AddItemToArray(root, wifi_js);
    }
    char * data = cJSON_Print(root);
    ESP_LOGI(TAG, "ws send:%s", data);
    web_ws_send((uint8_t *)data, strlen(data));
    cJSON_free(data);
    cJSON_Delete(root);
}


static void ws_receive_handle(uint8_t *payload, int len) {
    cJSON * root = cJSON_Parse((char *)payload);
    if (root) {
        cJSON * scan_js = cJSON_GetObjectItem(root, "scan");
        cJSON * ssid_js = cJSON_GetObjectItem(root, "ssid");
        cJSON * password_js = cJSON_GetObjectItem(root, "password");

        if (scan_js) {
            char * scan_value = cJSON_GetStringValue(scan_js);
            if (strcmp(scan_value, "start") == 0) {
                wifi_manager_scan(wifi_scan_handle);
            }
        }
        else if (ssid_js && password_js) {
            char * ssid_value = cJSON_GetStringValue(ssid_js);
            char * password_value = cJSON_GetStringValue(password_js);
            snprintf(current_ssid, sizeof(current_ssid), "%s", ssid_value);
            snprintf(current_password, sizeof(current_password), "%s", password_value);
            ESP_LOGI(TAG, "Receive ssid:%s,password:%s, now stop http server", current_ssid, current_password);
            save_wifi_credentials(current_ssid, current_password); // 持久化保存
            xEventGroupSetBits(apcfg_ev, APCFG_BIT);
        }
        else {
            ESP_LOGI(TAG, "Receive unknown message: %s", payload);
        }
        cJSON_Delete(root);
    } else {
        ESP_LOGE(TAG, "JSON parse failed");
    }
}

//进入AP配网模式
void ap_wifi_apcfg(void) {
    wifi_manager_ap();
    ws_cfg_t ws_cfg = {
        .html_code = html_code,
        .receive_fn = ws_receive_handle,
    };
    web_ws_start(&ws_cfg);
}