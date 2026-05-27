/**
 * @file    bsp_tools.h
 * @author  Xun Wei
 * @brief   BSP 通用 RTOS 辅助工具接口
 * @date    2026-05-27
 *
 * @note    该文件提供基于 CMSIS-RTOS2 thread flags 的回调任务创建接口，
 *          用于把中断或通信回调中的复杂处理转移到独立线程中执行。
 */
#pragma once

#include "cmsis_os2.h"
#include "bsp_log.h"

/**
 * @brief 创建一个等待 thread flag 唤醒的回调任务
 * @param name 任务名称，必须以 '\0' 结尾
 * @param cbk 回调函数指针
 * @param ins 回调函数参数，通常为模块实例指针
 * @param priority 任务优先级
 * @retval 回调任务使用的 thread flag bit，用于唤醒对应任务
 * @note 任务启动后会先执行一次回调，随后每次收到返回的 flag bit 后再次执行。
 */
uint32_t CreateCallbackTask(char *name, void *cbk, void *ins, osPriority_t priority);
