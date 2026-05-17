//
// Created by k0921 on 2026/3/28.
//

#include "motor_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <string.h>
#include <encoder/as5047p.h>
#include "pid.h"
#include "pin_config.h"
#include "svpwm.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"

#define TAG "motor_config"

void pwm_init(void) {
    mcpwm_config_t mcpwm_config ={
        .frequency = MCPWM_FREQUENCYM_M0,     // PWM 频率
        .cmpr_a = 0,                           // A 通道初始占空比
        .cmpr_b = 0,                           // B 通道初始占空比
        .duty_mode = MCPWM_DUTY_MODE_0,
        .counter_mode = MCPWM_UP_DOWN_COUNTER  // 上下计数模式，中心对齐 PWM
    };

    /*
     * EG2133 驱动芯片只需要三路高边 PWM 输入，因此这里只初始化 A 通道。
     * 如果更换为需要六路互补 PWM 的驱动芯片，可恢复 B 通道和死区配置。
     */
    mcpwm_gpio_init(MCPWM_UNIT_M0, MCPWM0A, M0_IN1);
    // mcpwm_gpio_init(MCPWM_UNIT_M0, MCPWM0B, TMC_UL);
    mcpwm_gpio_init(MCPWM_UNIT_M0, MCPWM1A, M0_IN2);
    // mcpwm_gpio_init(MCPWM_UNIT_M0, MCPWM1B, TMC_VL);
    mcpwm_gpio_init(MCPWM_UNIT_M0, MCPWM2A, M0_IN3);
    // mcpwm_gpio_init(MCPWM_UNIT_M0, MCPWM2B, TMC_WL);

    // gpio_config_t enio_cfg = {
    //     .intr_type = GPIO_INTR_DISABLE,
    //     .mode = GPIO_MODE_OUTPUT_OD,
    //     .pin_bit_mask = (1ULL << TMC_EN),
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //     .pull_up_en = GPIO_PULLUP_ENABLE,   // 外部电路已上拉
    // };
    // gpio_config(&enio_cfg);

    /* 每个 MCPWM timer 对应一个相输出：U/V/W。 */
    mcpwm_init(MCPWM_UNIT_M0, MCPWM_TIMER_0, &mcpwm_config);
    mcpwm_init(MCPWM_UNIT_M0, MCPWM_TIMER_1, &mcpwm_config);
    mcpwm_init(MCPWM_UNIT_M0, MCPWM_TIMER_2, &mcpwm_config);

    // 死区需要在 mcpwm_init 之后启用，否则配置不会生效。
    // mcpwm_deadtime_enable(MCPWM_UNIT_M0, MCPWM_TIMER_0, MCPWM_DEADTIME_MODE_M0, DEAD_TIME_NS, DEAD_TIME_NS);
    // mcpwm_deadtime_enable(MCPWM_UNIT_M0, MCPWM_TIMER_1, MCPWM_DEADTIME_MODE_M0, DEAD_TIME_NS, DEAD_TIME_NS);
    // mcpwm_deadtime_enable(MCPWM_UNIT_M0, MCPWM_TIMER_2, MCPWM_DEADTIME_MODE_M0, DEAD_TIME_NS, DEAD_TIME_NS);

    // 使能 TMC6300。
    // gpio_set_level(TMC_EN, 1);

    /* 启动三相 PWM 输出。 */
    mcpwm_start(MCPWM_UNIT_M0, MCPWM_TIMER_0);
    mcpwm_start(MCPWM_UNIT_M0, MCPWM_TIMER_1);
    mcpwm_start(MCPWM_UNIT_M0, MCPWM_TIMER_2);
}

pid_cfg_t pid_pos;
pid_cfg_t pid_vel;

/**
 * @brief 初始化位置环和速度环 PID 参数。
 */
void pid_init(void) {
    memset(&pid_pos, 0, sizeof(pid_cfg_t));
    memset(&pid_vel, 0, sizeof(pid_cfg_t));

    /* 位置环输出作为 q 轴电压或速度目标修正量使用。 */
    pid_pos = (pid_cfg_t){
        .p = 0.15f,
        .i = 0.0f,
        .d = 0.0f,
        .is_first = true,
        .max_limit_val_i = 1000.0f,
    };

    /* 速度环输出最终会被限幅为 q 轴电压。 */
    pid_vel = (pid_cfg_t){
        .p = 1.0f,
        .i = 0.0f,
        .d = 0.5f,
        .is_first = true,
        .max_limit_val_i = 1000.0f,
    };
}

motor_t M0;

void motor_zero_calibration(float shaft_angle, motor_t *motor) {
    /*
     * 给定固定 alpha 轴电压，使转子吸附到电角度 0 的方向。
     * 稳定后读取当前机械角度，作为后续电角度换算的零偏。
     */
    motor->u_alpha = 3.0f;
    motor->u_beta  = 0;

    for (int i=0; i<500; i++) {
        SVPWM_Core(motor);
        SVPWM_Set_Duty(motor);
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    motor->zero_bias_shaft_angle = shaft_angle;

    // 停止输出。如果需要校准后立即断电，可恢复下面几行。
    // motor->duty_u = 0.0f;
    // motor->duty_v = 0.0f;
    // motor->duty_w = 0.0f;
    // SVPWM_Set_Duty(motor);
}

void motor_init(float shaft_angle) {
    /* 初始化电机默认参数。zero_bias_shaft_angle 可由校准流程覆盖。 */
    M0 = (motor_t){
        .duty_u = 0.0f,
        .duty_v = 0.0f,
        .duty_w = 0.0f,
        .motor_pp = 7,
        .dir = BACKWARD,
        .u_alpha = 0.0f,
        .u_beta = 0.0f,
        .zero_bias_shaft_angle = 29.3f,
    };
    pid_init();
    pwm_init();

    /* 如需上电自动标定零点，可启用该调用。 */
    // motor_zero_calibration(shaft_angle, &M0);
}
