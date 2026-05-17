//
// Created by k0921 on 2026/3/28.
//

#include "motor_control.h"

#include <math.h>

#include "motor_config.h"
#include "pid.h"
#include "svpwm.h"

/* 位置误差进入死区后停止输出，减少到位附近的抖动。 */
#define MOTOR_POS_DEADBAND_DEG 1.0f

/* q 轴电压输出限幅和小信号死区，避免过大驱动或微小输出抖动。 */
#define MOTOR_UQ_LIMIT 5.0f
#define MOTOR_UQ_DEADBAND 0.2f

static float angle_error_normalized(float target, float current) {
    /* 将角度误差压到 -180~180 deg，保证电机走最短方向。 */
    float error = fmodf(target - current, 360.0f);

    if (error > 180.0f) {
        error -= 360.0f;
    } else if (error < -180.0f) {
        error += 360.0f;
    }

    return error;
}

static float motor_limit_uq(float uq) {
    uq = Limiting(-MOTOR_UQ_LIMIT, MOTOR_UQ_LIMIT, uq);

    if (fabsf(uq) < MOTOR_UQ_DEADBAND) {
        return 0.0f;
    }
    return uq;
}

void motor_set_pos(float target_pos, float current_pos) {
    const float error = angle_error_normalized(target_pos, current_pos);

    if (fabsf(error) < MOTOR_POS_DEADBAND_DEG) {
        /* 已进入目标死区：停止输出并清空位置环状态，避免积分残留。 */
        SVPWM_Controller(current_pos, 0.0f, 0.0f, &M0);
        pid_reset(&pid_pos);
        return;
    }

    pid_calculate(0.0f, error, &pid_pos);
    SVPWM_Controller(current_pos, motor_limit_uq(pid_pos.ret_val), 0.0f, &M0);
}

void motor_set_vel(float target_vel, float current_vel, float shaft_angle) {
    pid_calculate(target_vel, current_vel, &pid_vel);
    SVPWM_Controller(shaft_angle, motor_limit_uq(pid_vel.ret_val), 0.0f, &M0);
}

void motor_set_pos_with_vel(float target_pos, float target_vel, float current_pos, float current_vel) {
    const float error = angle_error_normalized(target_pos, current_pos);

    if (fabsf(error) < MOTOR_POS_DEADBAND_DEG && fabsf(target_vel - current_vel) < MOTOR_POS_DEADBAND_DEG) {
        /* 位置和速度都进入死区后停止输出，同时重置两个环的历史状态。 */
        SVPWM_Controller(current_pos, 0.0f, 0.0f, &M0);
        pid_reset(&pid_pos);
        pid_reset(&pid_vel);
        return;
    }

    /* 位置环作为外环，其输出叠加到速度目标后交给速度环。 */
    pid_calculate(0.0f, error, &pid_pos);
    const float vel_target_with_pos = target_vel + pid_pos.ret_val;
    pid_calculate(vel_target_with_pos, current_vel, &pid_vel);
    SVPWM_Controller(current_pos, motor_limit_uq(pid_vel.ret_val), 0.0f, &M0);
}
