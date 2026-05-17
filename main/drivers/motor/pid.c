//
// Created by k0921 on 2026/3/28.
//

#include "pid.h"

#include <stddef.h>

void pid_reset(pid_cfg_t *pid) {
    if (pid == NULL) {
        return;
    }

    /* 只重置运行状态，不修改 PID 参数，便于同一控制器继续复用。 */
    pid->error_p = 0.0f;
    pid->error_i = 0.0f;
    pid->error_d = 0.0f;
    pid->error_pre = 0.0f;
    pid->ret_val = 0.0f;
    pid->is_first = true;
}

void pid_calculate(float target_val, float current_val, pid_cfg_t *pid) {
    if (pid == NULL) {
        return;
    }

    const float error = target_val - current_val;
    pid->error_p = error;
    pid->error_i += error;

    /* 积分限幅用于减小长时间误差导致的积分饱和。 */
    if (pid->max_limit_val_i > 0.0f) {
        if (pid->error_i > pid->max_limit_val_i) {
            pid->error_i = pid->max_limit_val_i;
        } else if (pid->error_i < -pid->max_limit_val_i) {
            pid->error_i = -pid->max_limit_val_i;
        }
    }

    /* 首次计算时用当前误差初始化上一拍误差，避免 D 项产生尖峰。 */
    if (pid->is_first) {
        pid->error_pre = error;
        pid->is_first = false;
    }

    pid->error_d = error - pid->error_pre;
    pid->ret_val = pid->p * pid->error_p + pid->i * pid->error_i + pid->d * pid->error_d;
    pid->error_pre = error;
}

void pid_caculate(float target_val, float current_val, pid_cfg_t *pid) {
    pid_calculate(target_val, current_val, pid);
}
