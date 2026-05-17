//
// Created by k0921 on 2026/3/28.
//

#include "svpwm.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/mcpwm.h"

/* 三个静态函数分别封装 U/V/W 三相占空比写入。 */
static void M0_U_Set_Duty(float duty) {
    mcpwm_set_duty(MCPWM_UNIT_M0, MCPWM_TIMER_0, MCPWM_GEN_A, duty);
}

static void M0_V_Set_Duty(float duty) {
    mcpwm_set_duty(MCPWM_UNIT_M0, MCPWM_TIMER_1, MCPWM_GEN_A, duty);
}

static void M0_W_Set_Duty(float duty) {
    mcpwm_set_duty(MCPWM_UNIT_M0, MCPWM_TIMER_2, MCPWM_GEN_A, duty);
}

float Limiting(float min_val, float max_val, float real_val) {
    if (min_val > max_val) {
        const float tmp = min_val;
        min_val = max_val;
        max_val = tmp;
    }

    if (real_val > max_val) {
        return max_val;
    }
    if (real_val < min_val) {
        return min_val;
    }
    return real_val;
}

void ParkInverse(float uq, float ud, motor_t *motor) {
    if (motor == NULL) {
        return;
    }

    const float cos_theta = cosf(motor->electric_angle);
    const float sin_theta = sinf(motor->electric_angle);

    /* dq 旋转坐标系转换为静止 alpha-beta 坐标系。 */
    motor->u_alpha = ud * cos_theta - uq * sin_theta;
    motor->u_beta = ud * sin_theta + uq * cos_theta;
}

void SVPWM_Set_Duty(motor_t *motor) {
    if (motor == NULL) {
        return;
    }

    /* 输出前统一限幅，避免异常占空比写入 MCPWM。 */
    motor->duty_u = Limiting(0.0f, MAXDUTY, motor->duty_u);
    motor->duty_v = Limiting(0.0f, MAXDUTY, motor->duty_v);
    motor->duty_w = Limiting(0.0f, MAXDUTY, motor->duty_w);

    M0_U_Set_Duty(motor->duty_u);
    M0_V_Set_Duty(motor->duty_v);
    M0_W_Set_Duty(motor->duty_w);
}

void SVPWM_Core(motor_t *motor) {
    if (motor == NULL) {
        return;
    }

    /*
     * 通过三条参考轴 vref1/vref2/vref3 的符号确定空间矢量所在扇区。
     * n 为三位编码，sector_map 将编码映射到 SVPWM 的 1~6 扇区。
     */
    const float half = 0.5f;
    const float vref1 = motor->u_beta;
    const float vref2 = (SQRT3 * motor->u_alpha - motor->u_beta) * half;
    const float vref3 = (-SQRT3 * motor->u_alpha - motor->u_beta) * half;

    const uint8_t a = (vref1 > 0.0f) ? 1U : 0U;
    const uint8_t b = (vref2 > 0.0f) ? 1U : 0U;
    const uint8_t c = (vref3 > 0.0f) ? 1U : 0U;
    const uint8_t n = (uint8_t)(4U * c + 2U * b + a);
    static const uint8_t sector_map[8] = {0, 2, 6, 1, 4, 3, 5, 0};
    const uint8_t sector = sector_map[n];

    float t1 = 0.0f;
    float t2 = 0.0f;

    /* 根据扇区计算两个有效矢量的作用时间。 */
    switch (sector) {
        case 1:
            t2 = K * vref1;
            t1 = K * vref2;
            break;
        case 2:
            t2 = -K * vref2;
            t1 = -K * vref3;
            break;
        case 3:
            t1 = K * vref1;
            t2 = K * vref3;
            break;
        case 4:
            t2 = -K * vref1;
            t1 = -K * vref2;
            break;
        case 5:
            t2 = K * vref2;
            t1 = K * vref3;
            break;
        case 6:
            t1 = -K * vref1;
            t2 = -K * vref3;
            break;
        default:
            break;
    }

    if (t1 < 0.0f) {
        t1 = 0.0f;
    }
    if (t2 < 0.0f) {
        t2 = 0.0f;
    }

    /*
     * 当有效矢量时间超过一个 PWM 周期时，按比例缩放 t1/t2。
     * 这样可以保持方向不变，同时避免负的零矢量时间。
     */
    float t0 = TS - t1 - t2;
    if (t0 < 0.0f) {
        const float active_time = t1 + t2;
        if (active_time > 0.0f) {
            const float scale = TS / active_time;
            t1 *= scale;
            t2 *= scale;
        } else {
            t1 = 0.0f;
            t2 = 0.0f;
        }
        t0 = 0.0f;
    }

    /*
     * 七段式 SVPWM 分配：将零矢量时间均分到周期两端，
     * 再按当前扇区换算成 U/V/W 三相导通时间。
     */
    const float t0_half = t0 * half;
    float ta = TS * half;
    float tb = TS * half;
    float tc = TS * half;

    switch (sector) {
        case 1:
            ta = t1 + t2 + t0_half;
            tb = t2 + t0_half;
            tc = t0_half;
            break;
        case 2:
            ta = t1 + t0_half;
            tb = t1 + t2 + t0_half;
            tc = t0_half;
            break;
        case 3:
            ta = t0_half;
            tb = t1 + t2 + t0_half;
            tc = t2 + t0_half;
            break;
        case 4:
            ta = t0_half;
            tb = t1 + t0_half;
            tc = t1 + t2 + t0_half;
            break;
        case 5:
            ta = t2 + t0_half;
            tb = t0_half;
            tc = t1 + t2 + t0_half;
            break;
        case 6:
            ta = t1 + t2 + t0_half;
            tb = t0_half;
            tc = t1 + t0_half;
            break;
        default:
            break;
    }

    const float duty_scale = MAXDUTY / TS;
    motor->duty_u = ta * duty_scale;
    motor->duty_v = tb * duty_scale;
    motor->duty_w = tc * duty_scale;
}

float NormalizeAngleDegToRad(float angle_deg) {
    float angle = fmodf(angle_deg, 360.0f);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    return angle * (PI / 180.0f);
}

void Motor_Get_Electric_Angle(motor_t *motor, float shaft_angle) {
    if (motor == NULL) {
        return;
    }

    /*
     * 机械角度先减零偏，再乘极对数和方向得到电角度。
     * NormalizeAngleDegToRad 会将结果归一化到 0~2π。
     */
    motor->electric_angle = NormalizeAngleDegToRad(
        (shaft_angle - motor->zero_bias_shaft_angle) * motor->motor_pp * motor->dir);
}

void SVPWM_Controller(float shaft_angle, float uq, float ud, motor_t *motor) {
    if (motor == NULL) {
        return;
    }

    Motor_Get_Electric_Angle(motor, shaft_angle);
    ParkInverse(uq, ud, motor);
    SVPWM_Core(motor);
    SVPWM_Set_Duty(motor);
}
