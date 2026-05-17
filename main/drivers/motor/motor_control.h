//
// Created by k0921 on 2026/3/28.
//

#ifndef WHITE_KNOB_MOTOR_CONTROL_H
#define WHITE_KNOB_MOTOR_CONTROL_H

/**
 * @brief 位置闭环控制。
 *
 * @param target_pos 目标机械角度，单位 deg
 * @param current_pos 当前机械角度，单位 deg
 */
void motor_set_pos(float target_pos, float current_pos);

/**
 * @brief 速度闭环控制。
 *
 * @param target_vel 目标速度
 * @param current_vel 当前速度
 * @param shaft_angle 当前机械角度，单位 deg
 */
void motor_set_vel(float target_vel, float current_vel, float shaft_angle);

/**
 * @brief 位置外环 + 速度内环控制。
 *
 * 位置环输出会叠加到目标速度上，再交给速度环生成 q 轴电压。
 */
void motor_set_pos_with_vel(float target_pos, float target_vel, float current_pos, float current_vel);

#endif //WHITE_KNOB_MOTOR_CONTROL_H
