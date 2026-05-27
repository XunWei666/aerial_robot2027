/**
 * @file    bsp_tools.c
 * @author  Xun Wei
 * @brief   BSP 通用 RTOS 辅助工具实现
 * @date    2026-05-27
 *
 * @note    当前实现使用静态数组保存回调任务信息，并使用 CMSIS-RTOS2
 *          thread flags 替代 CMSIS-RTOS v1 signals。
 */
#include <stdarg.h>

#include "cmsis_os2.h"
#include "bsp_log.h"
#include "bsp_tools.h"

#define MX_SIG_LIST_SIZE 32 /**< 最大回调任务 flag 数量 */

typedef struct
{
    void *callback; /**< 回调函数指针 */
    uint32_t sig;   /**< 唤醒该任务使用的 thread flag bit */
    void *ins;      /**< 传递给回调函数的实例参数 */
} CallbackTask_t;

static uint8_t sig_idx = 0;
static uint32_t tmp_sig = 1;

static osThreadId_t cbkid_list[MX_SIG_LIST_SIZE];
static CallbackTask_t cbkinfo_list[MX_SIG_LIST_SIZE];

/**
 * @brief 回调任务基础入口
 * @param cbk 指向 CallbackTask_t 的任务参数
 * @note 保持原有逻辑：任务启动后先执行一次回调，之后等待对应 flag 再循环执行。
 */
__attribute__((noreturn)) static void CallbackTaskBase(void *cbk)
{
    void (*cbk_func)(void const *) = (void (*)(void const *))((CallbackTask_t const *)cbk)->callback;
    void const *ins = ((CallbackTask_t const *)cbk)->ins;
    uint32_t sig = ((CallbackTask_t const *)cbk)->sig;

    for (;;)
    {
        cbk_func(ins);
        (void)osThreadFlagsWait(sig, osFlagsWaitAny, osWaitForever);
    }
}

/**
 * @brief 创建一个等待 thread flag 唤醒的回调任务
 * @param name 任务名称，必须以 '\0' 结尾
 * @param cbk 回调函数指针
 * @param ins 回调函数参数，通常为模块实例指针
 * @param priority 任务优先级
 * @retval 回调任务使用的 thread flag bit，用于唤醒对应任务
 */
uint32_t CreateCallbackTask(char *name, void *cbk, void *ins, osPriority_t priority)
{
    if (sig_idx >= MX_SIG_LIST_SIZE)
        while (1)
            LOGERROR("[rtos:cbk_register] CreateCallbackTask: sig_idx >= MX_SIG_LIST_SIZE");

    cbkinfo_list[sig_idx].callback = cbk;
    cbkinfo_list[sig_idx].sig = tmp_sig << sig_idx;
    cbkinfo_list[sig_idx].ins = ins;

    const osThreadAttr_t thread_attr = {
        .name = name,
        .stack_size = 128,
        .priority = priority,
    };
    cbkid_list[sig_idx] = osThreadNew(CallbackTaskBase, (void *)&cbkinfo_list[sig_idx], &thread_attr);

    return cbkinfo_list[sig_idx++].sig;
}
