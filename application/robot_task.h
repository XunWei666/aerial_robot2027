/**
 * @file    robot_task.h
 * @author  Xun Wei
 * @brief   机器人应用层 RTOS 任务创建与任务入口实现
 * @date    2026-05-27
 *
 * @note    该文件仅用于任务初始化，并由 robot.c 包含。当前实现使用
 *          CMSIS-RTOS2 接口创建 INS、电机、守护、机器人主控与 UI 任务，
 *          各任务内部仍保持原有业务调用顺序和周期。
 */
#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"

#include "robot.h"
#include "ins_task.h"
#include "gimbal.h"
#include "motor_task.h"
#include "referee_task.h"
#include "daemon.h"
#include "buzzer.h"

#include "bsp_log.h"

osThreadId_t insTaskHandle;
osThreadId_t robotTaskHandle;
osThreadId_t motorTaskHandle;
osThreadId_t daemonTaskHandle;
osThreadId_t uiTaskHandle;

uint8_t ui_drawn_flag = 0;
static uint8_t last_ui_redraw_seq = 0;
extern volatile uint8_t ui_redraw_seq;

/**
 * @brief INS 姿态解算任务入口
 * @param argument 任务启动参数，当前未使用
 */
void StartINSTASK(void *argument);

/**
 * @brief 电机控制任务入口
 * @param argument 任务启动参数，当前未使用
 */
void StartMOTORTASK(void *argument);

/**
 * @brief 守护任务入口
 * @param argument 任务启动参数，当前未使用
 */
void StartDAEMONTASK(void *argument);

/**
 * @brief 机器人主控任务入口
 * @param argument 任务启动参数，当前未使用
 */
void StartROBOTTASK(void *argument);

/**
 * @brief UI 刷新任务入口
 * @param argument 任务启动参数，当前未使用
 */
void StartUITASK(void *argument);

/**
 * @brief 初始化机器人持续运行任务
 * @note 任务创建顺序和原 CMSIS-RTOS v1 版本保持一致。
 */
void OSTaskInit()
{
    const osThreadAttr_t insTask_attributes = {
        .name = "instask",
        .stack_size = 1024,
        .priority = osPriorityAboveNormal,
    };
    insTaskHandle = osThreadNew(StartINSTASK, NULL, &insTask_attributes);

    const osThreadAttr_t motorTask_attributes = {
        .name = "motortask",
        .stack_size = 512,
        .priority = osPriorityNormal,
    };
    motorTaskHandle = osThreadNew(StartMOTORTASK, NULL, &motorTask_attributes);

    const osThreadAttr_t daemonTask_attributes = {
        .name = "daemontask",
        .stack_size = 128,
        .priority = osPriorityNormal,
    };
    daemonTaskHandle = osThreadNew(StartDAEMONTASK, NULL, &daemonTask_attributes);

    const osThreadAttr_t robotTask_attributes = {
        .name = "robottask",
        .stack_size = 1024,
        .priority = osPriorityNormal,
    };
    robotTaskHandle = osThreadNew(StartROBOTTASK, NULL, &robotTask_attributes);

    const osThreadAttr_t uiTask_attributes = {
        .name = "uitask",
        .stack_size = 1024,
        .priority = osPriorityNormal,
    };
    uiTaskHandle = osThreadNew(StartUITASK, NULL, &uiTask_attributes);
}

/**
 * @brief 运行 INS 初始化和 1 kHz 姿态解算任务
 * @param argument 任务启动参数，当前未使用
 */
__attribute__((noreturn)) void StartINSTASK(void *argument)
{
    (void)argument;
    static float ins_start;
    static float ins_dt;
    INS_Init();
    for (;;)
    {
        ins_start = DWT_GetTimeline_ms();
        INS_Task();
        ins_dt = DWT_GetTimeline_ms() - ins_start;
        if (ins_dt > 1)
            ;
        osDelay(1);
    }
}

/**
 * @brief 运行 1 kHz 电机控制任务
 * @param argument 任务启动参数，当前未使用
 */
__attribute__((noreturn)) void StartMOTORTASK(void *argument)
{
    (void)argument;
    static float motor_dt;
    static float motor_start;
    for (;;)
    {
        motor_start = DWT_GetTimeline_ms();
        MotorControlTask();
        motor_dt = DWT_GetTimeline_ms() - motor_start;
        if (motor_dt > 1)
            ;
        osDelay(1);
    }
}

/**
 * @brief 运行 100 Hz 守护任务
 * @param argument 任务启动参数，当前未使用
 */
__attribute__((noreturn)) void StartDAEMONTASK(void *argument)
{
    (void)argument;
    static float daemon_dt;
    static float daemon_start;
    for (;;)
    {
        // 100Hz
        daemon_start = DWT_GetTimeline_ms();
        DaemonTask();
        daemon_dt = DWT_GetTimeline_ms() - daemon_start;
        if (daemon_dt > 10)
            ;
        osDelay(10);
    }
}

/**
 * @brief 运行机器人主控任务
 * @param argument 任务启动参数，当前未使用
 */
__attribute__((noreturn)) void StartROBOTTASK(void *argument)
{
    (void)argument;
    static float robot_dt;
    static float robot_start;
    for (;;)
    {
        robot_start = DWT_GetTimeline_ms();
        RobotTask();
        robot_dt = DWT_GetTimeline_ms() - robot_start;
        if (robot_dt > 5)
            ;
        osDelay(5);
    }
}

/**
 * @brief 运行裁判系统 UI 初始化与刷新任务
 * @param argument 任务启动参数，当前未使用
 */
__attribute__((noreturn)) void StartUITASK(void *argument)
{
    (void)argument;
    for (;;)
    {
        if (last_ui_redraw_seq != ui_redraw_seq)
        {
            last_ui_redraw_seq = ui_redraw_seq;
            MyUIInit();
            ui_drawn_flag = 1;
            UITask();
            osDelay(10);
            continue;
        }

        if (ui_drawn_flag)
        {
            UITask();
            osDelay(10);
        } else
        {
            osDelay(1);
        }
    }
}
