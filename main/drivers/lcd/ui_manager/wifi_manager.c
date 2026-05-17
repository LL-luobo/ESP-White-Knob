//
// Created by k0921 on 2026/4/23.
//

#include "wifi_manager.h"
#include "gui_guider.h"
#include "config/ui_manager.h"
#include "wifi/ap_wifi.h"
#include "time/sntp_config.h"
#include "esp_log.h"

#define TAG "wifi_manager"


static bool is_wifi_inited = false;

static bool is_wifi_connected = false;

lv_obj_t *label_connect = NULL;

void wifi_state_callback(wifi_state state) {
    if (state == WIFI_STATE_CONNECTED) {
        is_wifi_connected = true;
        if (label_connect) {
            lv_obj_del(label_connect);
            label_connect = NULL;
        }
        ui_animation(guider_ui.screen_wifi_label_2,
                300,
                0,
                lv_obj_get_y(guider_ui.screen_menu_label_selected), -22,
                lv_anim_path_ease_out,
                0, 0, 0, 0,
                (lv_anim_exec_xcb_t)lv_obj_set_y,
                NULL, NULL, NULL);

        lv_label_set_text(guider_ui.screen_wifi_label_2, "Wifi已连接");

        vTaskDelay(pdMS_TO_TICKS(300));

        ui_animation(guider_ui.screen_wifi_label_2,
                300,
                300,
                -lv_obj_get_y(guider_ui.screen_menu_label_selected), 0,
                lv_anim_path_ease_out,
                0, 0, 0, 0,
                (lv_anim_exec_xcb_t)lv_obj_set_y,
                NULL, NULL, NULL);

        ESP_LOGI(TAG, "WIFI connected.");
        //进行NSTP时间校准
        my_nstp_init();
    }
    else if (state == WIFI_STATE_DISCONNECTED) {
        is_wifi_connected = false;
        lv_label_set_text(guider_ui.screen_wifi_label_2, "Wifi未连接");
        ESP_LOGI(TAG, "WIFI disconnected.");
    }
}

/**
 * 进入wifi界面前的回调函数
 */
void wifi_enter(void) {
    //加载wifi界面
    if(guider_ui.screen_wifi_del) {
        setup_scr_screen_wifi(&guider_ui);
        guider_ui.screen_wifi_del = false;
    }
    //配置wifi
    if (!is_wifi_inited) {
        ap_wifi_init(wifi_state_callback);
        is_wifi_inited = true;
    }

    if (is_wifi_connected)
        lv_label_set_text(guider_ui.screen_wifi_label_2, "Wifi已连接");

    lv_scr_load(guider_ui.screen_wifi);
}

/**
 * 退出wifi界面的操作
 */
void wifi_exit(void) {
    //删除wifi界面的屏幕
    if (guider_ui.screen_wifi) {
        lv_obj_del(guider_ui.screen_wifi);
        guider_ui.screen_wifi_del = true;
        guider_ui.screen_wifi = NULL;
    }
}



void wifi_short_press(void) {
    if (!is_wifi_connected) {
        label_connect = lv_label_create(lv_scr_act());
        lv_label_set_text(label_connect, "Wifi is Connecting ...");
        lv_obj_set_style_text_color(label_connect, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(label_connect, &lv_font_AlimamaDaoLiTi_16, LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_align(label_connect, LV_ALIGN_CENTER, 0, 80);
        ap_wifi_apcfg();
    }
}

/**
 * 长按触发的操作
 */
void wifi_long_press(void) {
    //返回菜单界面
    screen_manager_set_active(&menu_interface);
}

/**
 * 注册wifi界面
 */
void wifi_register(void) {
    wifi_interface.enter = wifi_enter;
    wifi_interface.exit = wifi_exit;
    wifi_interface.rotate = NULL;
    wifi_interface.rotate_stop = NULL;
    wifi_interface.short_press = wifi_short_press;
    wifi_interface.release_press = NULL;
    wifi_interface.long_press = wifi_long_press;
    wifi_interface.is_registered = true;
}