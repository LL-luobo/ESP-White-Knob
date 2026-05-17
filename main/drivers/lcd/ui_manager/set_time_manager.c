//
// Created by k0921 on 2026/4/23.
//


#include "set_time_manager.h"

#include <math.h>

#include "gui_guider.h"
#include "config/ui_manager.h"
#include "time/get_my_time.h"
#include "smart_knob/motor_app.h"
#include <string.h>
#include <stdio.h>

// 三个滚轮的焦点索引：0 = 时, 1 = 分, 2 = 秒
static int roller_focus = 0;

//棘轮步数
static int detent_steps = 0;

static bool  first_rotate  = true;   // 首次旋转时不跳变

// 构建 "0\n1\n...\nmax_val" 格式的选项字符串
static void build_roller_options(char *buf, size_t buf_size, int max_val) {
    if (buf_size == 0) return;
    buf[0] = '\0';
    size_t remain = buf_size;
    for (int i = 0; i <= max_val; i++) {
        char tmp[16];  // 足够容纳 int 值
        if (i < max_val)
            snprintf(tmp, sizeof(tmp), "%d\n", i);
        else
            snprintf(tmp, sizeof(tmp), "%d", i);

        size_t len = strlen(tmp);
        if (len >= remain) {  // 缓冲区不足，截断
            strncat(buf, tmp, remain - 1);
            break;
        }
        strcat(buf, tmp);
        remain -= len;
    }
}

/**
 * 进入设置时间界面
 */
void set_time_enter(void) {
    // 如界面已被删除则重新创建
    if (guider_ui.screen_set_time_del) {
        setup_scr_screen_set_time(&guider_ui);
        guider_ui.screen_set_time_del = false;
    }

    //设置旋钮触感反馈模式
    g_haptic_state.mode = MODE_DETENT;
    g_haptic_state.params.detent.steps = 24;
    g_haptic_state.params.detent.width = 0;
    g_vibratic_next_mode = MODE_DETENT;

    //获取棘轮步数
    detent_steps = g_haptic_state.params.detent.steps;

    // 初始化角度步进相关变量
    first_rotate = true;        // 下次进入旋转回调时重新记录参考角度

    // 使用完整的 0~23 / 0~59 选项替换原初始化时的示例选项
    char opts[256];
    build_roller_options(opts, sizeof(opts), 23);
    lv_roller_set_options(guider_ui.screen_set_time_roller_1, opts, LV_ROLLER_MODE_INFINITE);
    build_roller_options(opts, sizeof(opts), 59);
    lv_roller_set_options(guider_ui.screen_set_time_roller_2, opts, LV_ROLLER_MODE_INFINITE);
    build_roller_options(opts, sizeof(opts), 59);
    lv_roller_set_options(guider_ui.screen_set_time_roller_3, opts, LV_ROLLER_MODE_INFINITE);

    // 全部归零
    lv_roller_set_selected(guider_ui.screen_set_time_roller_1, 0, LV_ANIM_OFF);
    lv_roller_set_selected(guider_ui.screen_set_time_roller_2, 0, LV_ANIM_OFF);
    lv_roller_set_selected(guider_ui.screen_set_time_roller_3, 0, LV_ANIM_OFF);

    // 焦点默认指向“时”滚轮
    roller_focus = 0;

    lv_scr_load(guider_ui.screen_set_time);
}

/**
 * 退出设置时间界面（删除屏幕）
 */
void set_time_exit(void) {
    if (guider_ui.screen_set_time) {
        lv_obj_del(guider_ui.screen_set_time);
        guider_ui.screen_set_time_del = true;
        guider_ui.screen_set_time = NULL;
    }
    roller_focus = 0;   // 重置焦点

    //设置旋钮触感为自由旋转
    g_haptic_state.mode = MODE_FREE;
    g_vibratic_next_mode = MODE_FREE;
}

static uint16_t hour, min, sec;

void set_time_set_mhs(uint16_t *h, uint16_t *m, uint16_t *s) {
    *h = hour;
    *m = min;
    *s = sec;
}

/**
 * 短按：确认当前滚轮选择，并切换到下一个滚轮（时 → 分 → 秒 → 保存退出）
 */
void set_time_short_press(void) {
    if (roller_focus < 2) {
        // 移到下一个滚轮
        roller_focus++;
        //分和秒使用60档的棘轮
        g_haptic_state.params.detent.steps = 60;
        // 重置参考角度
        first_rotate = true;
    }
    else {
        // 已处于“秒”滚轮，视为完成设定：读取三个滚轮的值并保存时间
        hour = lv_roller_get_selected(guider_ui.screen_set_time_roller_1);
        min  = lv_roller_get_selected(guider_ui.screen_set_time_roller_2);
        sec  = lv_roller_get_selected(guider_ui.screen_set_time_roller_3);

        // 返回倒计时界面
        screen_manager_set_active(&potatotime_interface);
    }
}

/**
 * 长按：直接返回菜单，不保存修改
 */
void set_time_long_press(void) {
    screen_manager_set_active(&menu_interface);
}

/**
 * 旋转编码器事件，根据当前聚焦的滚轮调整其选中的值
 * @param delta 旋转角度增量（输入/消耗）
 * @param angle 当前绝对角度（未使用）
 */
void set_time_rotate(float *delta, const float *angle) {
    // 1. 获取当前滚轮
    lv_obj_t *roller = NULL;
    switch (roller_focus) {
        case 0: roller = guider_ui.screen_set_time_roller_1; break;
        case 1: roller = guider_ui.screen_set_time_roller_2; break;
        case 2: roller = guider_ui.screen_set_time_roller_3; break;
        default: return;
    }

    static float last_pos = 0.0f;
    static int pending_steps = 0;   // 待处理的步数（带符号）

    // 首次进入，初始化参考
    if (first_rotate) {
        first_rotate = false;
        pending_steps = 0;
        int detent_steps = g_haptic_state.params.detent.steps;
        if (detent_steps != 0) {
            float spacing = 360.0f / detent_steps;
            last_pos = round(*angle / spacing) * spacing;
        }
        return;
    }

    int detent_steps = g_haptic_state.params.detent.steps;
    if (detent_steps == 0) return;
    float spacing = 360.0f / detent_steps;

    // 计算卡位中心并累加步数变化
    float target_pos = round(*angle / spacing) * spacing;
    int step_change = (int)roundf((target_pos - last_pos) / spacing);
    pending_steps += step_change;
    last_pos = target_pos;

    // 本次最多处理 ±1 步
    while (pending_steps != 0) {
        // 计算卡位中心并累加步数变化
        float target_pos = round(*angle / spacing) * spacing;
        int step_change = (int)roundf((target_pos - last_pos) / spacing);
        pending_steps += step_change;
        last_pos = target_pos;

        int dir = (pending_steps > 0) ? 1 : -1;
        uint16_t count = lv_roller_get_option_cnt(roller);
        int cur = (int)lv_roller_get_selected(roller);

        // 移动 1 步，带循环
        int new_index = cur + dir;
        new_index = new_index % (int)count;
        if (new_index < 0) new_index += (int)count;

        lv_roller_set_selected(roller, (uint16_t)new_index, LV_ANIM_ON);

        // 消耗 1 步
        pending_steps -= dir;
    }
}

/**
 * 注册设置时间界面的回调接口
 */
void set_time_register(void) {
    set_time_interface.enter        = set_time_enter;
    set_time_interface.exit         = set_time_exit;
    set_time_interface.rotate       = set_time_rotate;
    set_time_interface.rotate_stop  = NULL;
    set_time_interface.short_press  = set_time_short_press;
    set_time_interface.release_press = NULL;
    set_time_interface.long_press   = set_time_long_press;
    set_time_interface.is_registered = true;
}