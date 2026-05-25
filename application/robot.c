#include "bsp_init.h"
#include "robot.h"
#include "robot_def.h"
#include "robot_task.h"
#include "message_center.h"
#include "gimbal.h"
#include "shoot.h"
#include "robot_cmd.h"

/**
 * @brief 机器人初始化函数
 */
void RobotInit(void)
{
    // 初始化阶段关闭中断，避免外设尚未完成配置时进入中断流程。
    // 初始化过程中如需短延时，仅允许使用 DWT_Delay。
    __disable_irq();

    BSPInit();
    MessageCenterInit();

    // 当前工程固定为云台单板，因此仅初始化指令、云台和发射机构应用。
    RobotCMDInit();
    GimbalInit();
    ShootInit();

    // 运行期不再允许新增话题或订阅者，避免任务启动后修改通信拓扑。
    MessageCenterFreezeRegistry();

    OSTaskInit();

    __enable_irq();
}

/**
 * @brief 机器人核心任务函数
 */
void RobotTask(void)
{
    RobotCMDTask();
    GimbalTask();
    ShootTask();
}
