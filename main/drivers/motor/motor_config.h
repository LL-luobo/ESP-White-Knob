//
// Created by k0921 on 2026/3/28.
//

#ifndef WHITE_KNOB_MOTOR_HARDWARE_H
#define WHITE_KNOB_MOTOR_HARDWARE_H

#include "pid.h"

/* 电机 M0 使用的 MCPWM 单元和 PWM 基础参数。 */
#define MCPWM_UNIT_M0           MCPWM_UNIT_0                            // 选择 MCPWM 单元
#define MCPWM_DEADTIME_MODE_M0  MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE       // 死区模式：A 相同相，B 相互补
#define MCPWM_FREQUENCYM_M0     30000                                   // PWM 频率，单位 Hz
#define DEAD_TIME_NS            200                                     // 死区时间，单位 ns

/* 电机旋转方向。三相线接线顺序变化时，可用该方向修正电角度。 */
typedef enum {
    FORWARD = 1,
    BACKWARD = -1,
}motor_dir;

/**
 * @brief 电机控制运行参数。
 *
 * duty_u/duty_v/duty_w 为三相 PWM 占空比；u_alpha/u_beta 为 SVPWM 使用的
 * alpha-beta 坐标系电压分量；electric_angle 由机械角度、极对数和零偏换算得到。
 */
typedef struct {
    float duty_u, duty_v, duty_w;       // U/V/W 三相 PWM 占空比，范围 0~MAXDUTY
    float u_alpha, u_beta;              // alpha/beta 坐标系电压分量
    float electric_angle;               // 电角度，单位 rad
    float zero_bias_shaft_angle;        // 机械零偏角度，单位 deg
    int motor_pp;                       // 电机极对数
    motor_dir dir;                      // 电机方向，按三相线接线顺序配置
}motor_t;

extern pid_cfg_t pid_pos;
extern pid_cfg_t pid_vel;

extern motor_t M0;

/**
 * @brief 初始化电机参数、PID 和 MCPWM。
 *
 * @param shaft_angle 当前机械角度，预留给零点校准流程使用。
 */
void motor_init(float shaft_angle);

#endif //WHITE_KNOB_MOTOR_HARDWARE_H
