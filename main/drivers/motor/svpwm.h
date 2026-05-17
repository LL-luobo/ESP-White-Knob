//
// Created by k0921 on 2026/3/28.
//

#ifndef WHITE_KNOB_SVPWM_H
#define WHITE_KNOB_SVPWM_H

#include "motor_config.h"

#define PI 3.14159265f
#define SQRT3 1.73205080757f
#define SQRT3_2 0.86602540378f

/* 直流母线电压和 MCPWM 占空比范围。 */
#define DCBUSVALUE 5.0f
#define MAXDUTY 100.0f

/* SVPWM 时间基准和电压到矢量作用时间的换算系数。 */
#define TS (1.0f / MCPWM_FREQUENCYM_M0)
#define K (SQRT3 * TS / DCBUSVALUE)

/**
 * @brief 将数值限制在指定范围内。
 */
float Limiting(float min_val, float max_val, float real_val);

/**
 * @brief 将角度归一化到 0~2π。
 *
 * @param angle_deg 输入机械角度，单位 deg
 * @return 归一化后的弧度值，单位 rad
 */
float NormalizeAngleDegToRad(float angle_deg);

/**
 * @brief 根据机械角度计算电角度。
 */
void Motor_Get_Electric_Angle(motor_t *motor, float shaft_angle);

/**
 * @brief 反 Park 变换，将 dq 轴电压转换到 alpha-beta 坐标系。
 */
void ParkInverse(float uq, float ud, motor_t *motor);

/**
 * @brief 根据 alpha-beta 电压分量计算三相 PWM 占空比。
 */
void SVPWM_Core(motor_t *motor);

/**
 * @brief 将 motor 中的三相占空比写入 MCPWM。
 */
void SVPWM_Set_Duty(motor_t *motor);

/**
 * @brief SVPWM 控制入口：角度换算、反 Park 变换、占空比计算和输出。
 */
void SVPWM_Controller(float shaft_angle, float uq, float ud, motor_t *motor);

#endif //WHITE_KNOB_SVPWM_H
