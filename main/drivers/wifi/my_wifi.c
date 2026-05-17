// Created by k0921 on 2026/3/19.
//

#include "my_wifi.h"
#include <esp_mac.h>
#include <nvs.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>
#include <lwip/ip4_addr.h>
#include "ap_wifi.h"

#define TAG "my_wifi"

#define NVS_NAMESPACE "wifi_cfg"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "password"

// 最大重连次数，超过此次数后不再自动重连
#define MAX_CONNECT_COUNT   10
// 当前重连尝试次数
static int sta_connected_cnt = 0;

// 指向外部回调函数的指针，用于通知WiFi状态变化
static p_wifi_state_cb wifi_cb = NULL;

// WiFi连接状态标志，true表示已连接并获得IP
static bool is_sta_connected = false;

static esp_netif_t *esp_netif_ap_obj;

static SemaphoreHandle_t scan_sem = NULL;

//AP模式下的SSID名称
static const char * ap_ssid_name = "White_Knob_AP";
//AP模式下的密码
static const char * ap_password = "12345678";

/**
 * @brief 事件处理函数，处理WiFi事件和IP事件
 * @param arg 用户注册时传递的参数（此处为NULL）
 * @param event_base 事件基础ID（WIFI_EVENT或IP_EVENT）
 * @param event_id 具体事件ID
 * @param event_data 事件相关数据（未使用）
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                // Station启动后立即尝试连接
                wifi_mode_t mode;
                esp_wifi_get_mode(&mode);
                if(mode == WIFI_MODE_STA)
                    esp_wifi_connect();         //启动WIFI连接
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                // 断开连接，更新状态并通知回调
                if(is_sta_connected)
                {
                    if(wifi_cb)
                        wifi_cb(WIFI_STATE_DISCONNECTED);
                    is_sta_connected = false;
                }
                // 如果未达到最大重连次数，则尝试重连
                if (sta_connected_cnt < MAX_CONNECT_COUNT) {
                    wifi_mode_t mode;
                    esp_wifi_get_mode(&mode);
                    if(mode == WIFI_MODE_STA)
                        esp_wifi_connect();             //继续重连
                    sta_connected_cnt++;
                }
                ESP_LOGI(TAG,"connect to the AP fail,retry now");
                break;
            case WIFI_EVENT_STA_CONNECTED://WIFI连上路由器后，触发此事件
                ESP_LOGI(TAG, "connected to ap");
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                //有设备连接了热点，把它的MAC打印出来
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
                ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                        MAC2STR(event->mac), event->aid);
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                //有设备断开了热点
                wifi_event_ap_stadisconnected_t *event_2 = (wifi_event_ap_stadisconnected_t *) event_data;
                ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                        MAC2STR(event_2->mac), event_2->aid);
                break;
            default:
                break;
        }

    }
    else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            // 获得IP地址，表示完全连接成功
            ESP_LOGI(TAG, "Get wifi IP");
            is_sta_connected = true;
            if (wifi_cb) {
                wifi_cb(WIFI_STATE_CONNECTED);
            }
        }
    }
}

/**
 * @brief 初始化WiFi管理器
 * @param f 指向状态回调函数的指针，当WiFi状态变化时调用
 */
void wifi_manager_init(p_wifi_state_cb f) {
    // 初始化TCP/IP网络接口
    ESP_ERROR_CHECK(esp_netif_init());

    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // 创建默认的WiFi Station网络接口
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(sta_netif, "White_Knob");   // 在此设置你想要的名称

    esp_netif_ap_obj = esp_netif_create_default_wifi_ap();

    // 使用默认配置初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册WiFi事件处理（所有事件）
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    // 注册IP事件处理（仅关注获得IP事件）
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // 保存回调函数指针
    wifi_cb = f;

    scan_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(scan_sem);

    // 设置WiFi模式为Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );

    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start() );
}

/**
 * @brief 连接指定SSID的WiFi网络
 * @param ssid 要连接的WiFi SSID
 * @param password WiFi密码
 */
void wifi_manager_connect(const char * ssid, const char * password) {
    sta_connected_cnt = 0;
    wifi_config_t wifi_config = {
        .sta = {
            // 注意：此处阈值设置为WPA2，若需兼容WEP/WPA需调整
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 加密方式
            // 以下成员根据需要可配置，但此处使用默认值即可
        },
    };
    // 将传入的SSID和密码复制到配置结构中（最多31字节SSID，63字节密码，留一位给终止符）
    snprintf((char *)wifi_config.sta.ssid, 31, "%s", ssid);
    snprintf((char *)wifi_config.sta.password, 63, "%s", password);

    // 设置WiFi模式为Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    // 应用WiFi配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    // 以下代码段用于处理当前WiFi模式非STA的情况
    wifi_mode_t wifi_mode;
    esp_wifi_get_mode(&wifi_mode);
    if (wifi_mode == WIFI_MODE_STA) {
        // 如果已经是STA模式，先停止WiFi再重新设置（确保配置生效）
        esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_STA);
    }
    // 重置重连计数器（因为正在主动发起新连接）
    sta_connected_cnt = 0;
    // 再次应用配置并启动WiFi
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_start();
    // //wifi连接完成释放信号量
    // xSemaphoreGive(wifi_sem);
}

esp_err_t wifi_manager_ap(void) {
    wifi_mode_t wifi_mode;
    esp_wifi_get_mode(&wifi_mode);
    if (wifi_mode == WIFI_MODE_APSTA) {
        return ESP_OK;
    }
    else {
        ESP_LOGW(TAG, "WIFI_MODE_APSTA not supported. Retrying now....");
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    wifi_config_t wifi_config = {
        .ap = {
            .channel = 5,
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA2_PSK,
        }
    };
    snprintf((char *)wifi_config.ap.ssid, 32, "%s", ap_ssid_name);
    wifi_config.ap.ssid_len = strlen(ap_ssid_name);
    snprintf((char *)wifi_config.ap.password, 64, "%s", ap_password);

    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);

    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 100, 1);    //IP地址
    IP4_ADDR(&ip_info.gw, 192, 168, 100, 1);    //网关
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);//子网掩码

    esp_netif_dhcps_stop(esp_netif_ap_obj);//设置IP之前停用DHCP
    esp_netif_set_ip_info(esp_netif_ap_obj, &ip_info);
    esp_netif_dhcps_start(esp_netif_ap_obj);//设置IP成功之后停用DHCP

    return esp_wifi_start();
}

//扫描任务
static void scan_task(void * param) {
    p_wifi_scan_cb callback = (p_wifi_scan_cb)param;
    uint16_t ap_count = 0;
    uint16_t ap_num = 20;
    wifi_ap_record_t * ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_num);
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_num(&ap_count);
    esp_wifi_scan_get_ap_records(&ap_num, ap_list);
    ESP_LOGI(TAG, "Total ap count: %d, actual ap number:%d", ap_count, ap_num);
    if (callback) {
        callback(ap_num, ap_list);
    }
    free(ap_list);
    xSemaphoreGive(scan_sem);//释放信号量
    vTaskDelete(NULL);
}

esp_err_t wifi_manager_scan(p_wifi_scan_cb f) {
    if (pdTRUE == xSemaphoreTake(scan_sem, 0)) {
        esp_wifi_clear_ap_list();//清除上次扫描的信息
        return xTaskCreatePinnedToCore(scan_task, "scan", 8192, f, 3, NULL, 1);
    }
    return ESP_FAIL;
}

// 保存凭据
void save_wifi_credentials(const char *ssid, const char *password) {
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_str(handle, NVS_KEY_SSID, ssid));
    ESP_ERROR_CHECK(nvs_set_str(handle, NVS_KEY_PASS, password));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}

// 读取凭据，返回 true 表示读取到有效的凭据
bool load_wifi_credentials(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t len = ssid_len;
    esp_err_t err = nvs_get_str(handle, NVS_KEY_SSID, ssid, &len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    len = pass_len;
    err = nvs_get_str(handle, NVS_KEY_PASS, pass, &len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    nvs_close(handle);
    return true;
}