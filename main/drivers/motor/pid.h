//
// Created by k0921 on 2026/3/28.
//

#ifndef WHITE_KNOB_PID_H
#define WHITE_KNOB_PID_H
#include <stdbool.h>

/**
 * @brief PID 控制器参数和运行状态。
 *
 * p/i/d 为控制系数；error_* 保存当前比例、积分、微分误差；
 * ret_val 为最近一次计算得到的控制输出。
 */
typedef struct {
    float p;                    // 比例系数
    float i;                    // 积分系数
    float d;                    // 微分系数
    float error_p;              // 比例误差
    float error_i;              // 积分误差累计值
    float max_limit_val_i;      // 积分限幅值；小于等于 0 表示不启用限幅
    float error_d;              // 微分误差
    float error_pre;            // 上一次误差，用于计算微分项
    bool is_first;              // 首次计算标志，用于避免微分项初始突变
    float ret_val;              // PID 输出值
}pid_cfg_t;

/**
 * @brief 清空 PID 运行状态，保留 p/i/d 和积分限幅配置。
 */
void pid_reset(pid_cfg_t *pid);

/**
 * @brief 计算一次 PID 输出。
 *
 * @param target_val 目标值
 * @param current_val 当前反馈值
 * @param pid PID 控制器配置和状态
 */
void pid_calculate(float target_val, float current_val, pid_cfg_t *pid);

/**
 * @brief 兼容旧拼写的 PID 计算接口，内部调用 pid_calculate。
 */
void pid_caculate(float target_val, float current_val, pid_cfg_t *pid);

#endif //WHITE_KNOB_PID_H
