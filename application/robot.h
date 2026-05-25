#ifndef ROBOT_H
#define ROBOT_H

/* 当前工程仅负责云台与发射机构的初始化和任务调度。 */

/**
 * @brief 机器人初始化函数
 * @note 该函数应当在启动 RTOS 之前调用。
 */
void RobotInit(void);

/**
 * @brief 机器人核心任务函数
 * @note 该函数会按固定周期调度云台与发射机构应用任务。
 */
void RobotTask(void);

#endif
