/**
 * @file referee.C
 * @author kidneygood (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-11-18
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "referee_task.h"
#include "robot_def.h"
#include "rm_referee.h"
#include "referee_UI.h"
#include "string.h"
#include "cmsis_os.h"

static Referee_Interactive_info_t *Interactive_data; // UI绘制需要的机器人状态数据
static referee_info_t *referee_recv_info;            // 接收到的裁判系统数据
uint8_t UI_Seq;                                      // 包序号，供整个referee文件使用
// @todo 不应该使用全局变量

/**
 * @brief  判断各种ID，选择客户端ID
 * @param  referee_info_t *referee_recv_info
 * @retval none
 * @attention
 */
static void DeterminRobotID()
{
    // id小于7是红色,大于7是蓝色,0为红色，1为蓝色   #define Robot_Red 0    #define Robot_Blue 1
    referee_recv_info->referee_id.Robot_Color = referee_recv_info->GameRobotState.robot_id > 7 ? Robot_Blue : Robot_Red;
    referee_recv_info->referee_id.Robot_ID = referee_recv_info->GameRobotState.robot_id;
    referee_recv_info->referee_id.Cilent_ID = 0x0100 + referee_recv_info->referee_id.Robot_ID; // 计算客户端ID
    referee_recv_info->referee_id.Receiver_Robot_ID = 0;
}

static void MyUIRefresh(referee_info_t *referee_recv_info, Referee_Interactive_info_t *_Interactive_data);
static void UIChangeCheck(Referee_Interactive_info_t *_Interactive_data); // 模式切换检测
// static void RobotModeTest(Referee_Interactive_info_t *_Interactive_data); // 测试用函数，实现模式自动变化

referee_info_t *UITaskInit(UART_HandleTypeDef *referee_usart_handle, Referee_Interactive_info_t *UI_data)
{
    referee_recv_info = RefereeInit(referee_usart_handle); // 初始化裁判系统的串口,并返回裁判系统反馈数据指针
    Interactive_data = UI_data;                            // 获取UI绘制需要的机器人状态数据
    referee_recv_info->init_flag = 1;
    return referee_recv_info;
}

void UITask()
{
    MyUIRefresh(referee_recv_info, Interactive_data);
}

static Graph_Data_t UI_shoot[10]; // 射击准线
static String_Data_t UI_State_sta[6];  // 机器人状态,静态只需画一次
static String_Data_t UI_State_dyn[6];  // 机器人状态,动态先add才能change
// static uint32_t shoot_line_location[10] = {540, 960, 490, 515, 565};

void MyUIInit()
{
    if (!referee_recv_info->init_flag)
        vTaskDelete(NULL); // 如果没有初始化裁判系统则直接删除ui任务
    while (referee_recv_info->GameRobotState.robot_id == 0)
        osDelay(100); // 若还未收到裁判系统数据,等待一段时间后再检查

    DeterminRobotID();                                            // 确定ui要发送到的目标客户端
    UIDelete(&referee_recv_info->referee_id, UI_Data_Del_ALL, 0); // 清空UI

    // 绘制发射基准圆
    UICircleDraw(&UI_shoot[0], "sl0", UI_Graph_ADD, 7, UI_Color_Purplish_red, 5, 960, 568, 10);
    UIGraphRefresh(&referee_recv_info->referee_id, 1, UI_shoot[0]);

    UICircleDraw(&UI_shoot[1], "sl1", UI_Graph_ADD, 7, UI_Color_Purplish_red, 3, 960, 568, 1);
    UIGraphRefresh(&referee_recv_info->referee_id, 1, UI_shoot[1]);

    // 绘制车辆状态标志指示
    UICharDraw(&UI_State_sta[1], "ss1", UI_Graph_ADD, 8, UI_Color_Yellow, 15, 2, 80, 700, "gimbal:");
    UICharRefresh(&referee_recv_info->referee_id, UI_State_sta[1]);
    UICharDraw(&UI_State_sta[2], "ss2", UI_Graph_ADD, 8, UI_Color_Orange, 15, 2, 80, 650, "(E)shoot:");
    UICharRefresh(&referee_recv_info->referee_id, UI_State_sta[2]);
    UICharDraw(&UI_State_sta[3], "ss3", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 80, 600, "(R)frict:");
    UICharRefresh(&referee_recv_info->referee_id, UI_State_sta[3]);
    UICharDraw(&UI_State_sta[4], "ss4", UI_Graph_ADD, 8, UI_Color_Green, 15, 2, 80, 550, "(Q)speed_mode:");
    UICharRefresh(&referee_recv_info->referee_id, UI_State_sta[4]);

    switch (Interactive_data->gimbal_mode)
    {
        case GIMBAL_ZERO_FORCE:
            UICharDraw(&UI_State_dyn[1], "sd1", UI_Graph_ADD, 8, UI_Color_Yellow, 15, 2, 270, 700, "zeroforce");
            break;
        case GIMBAL_GYRO_MODE:
            UICharDraw(&UI_State_dyn[1], "sd1", UI_Graph_ADD, 8, UI_Color_Yellow, 15, 2, 270, 700, "   gyro  ");
            break;
        case GIMBAL_SELF_MODE:
            UICharDraw(&UI_State_dyn[1], "sd1", UI_Graph_ADD, 8, UI_Color_Yellow, 15, 2, 270, 700, "   self  ");
            break;
        default:
            UICharDraw(&UI_State_dyn[1], "sd1", UI_Graph_ADD, 8, UI_Color_Yellow, 15, 2, 270, 700, "zeroforce");
            break;
    }
    UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[1]);

    switch (Interactive_data->loader_mode)
    {
        case LOAD_STOP:
            UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 270, 650, "  stop ");
            break;
        case LOAD_1_BULLET:
            UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 270, 650, " single");
            break;
        case LOAD_3_BULLET:
            UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 270, 650, " tribe ");
            break;
        case LOAD_BURSTFIRE:
            UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 270, 650, " burst ");
            break;
        case LOAD_REVERSE:
            UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 270, 650, "reserve");
            break;
        default:
            UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 270, 650, "  stop ");
            break;
    }
    UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[2]);

    UICharDraw(&UI_State_dyn[3], "sd3", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 270, 600, Interactive_data->friction_mode == FRICTION_ON ? "   on " : "   off");
    UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[3]);

    switch (Interactive_data->speed_mode)
    {
        case SMALL_AMU_18:
            UICharDraw(&UI_State_dyn[4], "sd4", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 300, 550, " max");
            break;
        case SMALL_AMU_17:
            UICharDraw(&UI_State_dyn[4], "sd4", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 300, 550, " mid");
            break;
        case SMALL_AMU_16:
            UICharDraw(&UI_State_dyn[4], "sd4", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 300, 550, " min");
            break;
        default:
            UICharDraw(&UI_State_dyn[4], "sd4", UI_Graph_ADD, 8, UI_Color_Pink, 15, 2, 300, 550, " max");
            break;
    }
    UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[4]);

    Interactive_data->gimbal_last_mode = Interactive_data->gimbal_mode;
    Interactive_data->loader_last_mode = Interactive_data->loader_mode;
    Interactive_data->friction_last_mode = Interactive_data->friction_mode;
    Interactive_data->speed_last_mode = Interactive_data->speed_mode;
    Interactive_data->Referee_Interactive_Flag.gimbal_flag = 0;
    Interactive_data->Referee_Interactive_Flag.shoot_flag = 0;
    Interactive_data->Referee_Interactive_Flag.friction_flag = 0;
    Interactive_data->Referee_Interactive_Flag.speed_flag = 0;
}

// 测试用函数，实现模式自动变化,用于检查该任务和裁判系统是否连接正常
// static uint8_t count = 0;
// static uint16_t count1 = 0;
// static void RobotModeTest(Referee_Interactive_info_t *_Interactive_data) // 测试用函数，实现模式自动变化
// {
//     count++;
//     if (count >= 50)
//     {
//         count = 0;
//         count1++;
//     }
//     switch (count1 % 4)
//     {
//     case 0:
//     {
//         _Interactive_data->gimbal_mode = GIMBAL_ZERO_FORCE;
//         _Interactive_data->loader_mode = LOAD_BURSTFIRE;
//         _Interactive_data->friction_mode = FRICTION_ON;
//         break;
//     }
//     case 1:
//     {
//         _Interactive_data->gimbal_mode = GIMBAL_GYRO_MODE;
//         _Interactive_data->loader_mode = LOAD_1_BULLET;
//         _Interactive_data->friction_mode = FRICTION_ON;
//         break;
//     }1
//     case 2:
//     {
//         _Interactive_data->gimbal_mode = GIMBAL_SELF_MODE;
//         _Interactive_data->loader_mode = LOAD_STOP;
//         _Interactive_data->friction_mode = FRICTION_OFF;
//         break;
//     }
//     default:
//         _Interactive_data->gimbal_mode = GIMBAL_GYRO_MODE;
//         _Interactive_data->loader_mode = LOAD_3_BULLET;
//         _Interactive_data->friction_mode = FRICTION_ON;
//         break;
//     }
// }

static void MyUIRefresh(referee_info_t *referee_recv_info, Referee_Interactive_info_t *_Interactive_data)
{
    UIChangeCheck(_Interactive_data);
    // gimbal
    if (_Interactive_data->Referee_Interactive_Flag.gimbal_flag == 1)
    {
        switch (_Interactive_data->gimbal_mode)
        {
            case GIMBAL_ZERO_FORCE:
                UICharDraw(&UI_State_dyn[1], "sd1", UI_Graph_Change, 8, UI_Color_Yellow, 15, 2, 270, 700, "zeroforce");
                break;
            case GIMBAL_GYRO_MODE:
                UICharDraw(&UI_State_dyn[1], "sd1", UI_Graph_Change, 8, UI_Color_Yellow, 15, 2, 270, 700, "   gyro  ");
                break;
            case GIMBAL_SELF_MODE:
                UICharDraw(&UI_State_dyn[1], "sd1", UI_Graph_Change, 8, UI_Color_Yellow, 15, 2, 270, 700, "   self  ");
                break;
        }

        UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[1]);
        _Interactive_data->Referee_Interactive_Flag.gimbal_flag = 0;
    }
    // shoot
    if (_Interactive_data->Referee_Interactive_Flag.shoot_flag == 1)
    {
        switch (_Interactive_data->loader_mode)
        {
            case LOAD_STOP:
                UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 270, 650, "  stop ");
                break;
            case LOAD_1_BULLET:
                UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 270, 650, " single");
                break;
            case LOAD_3_BULLET:
                UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 270, 650, " tribe ");
                break;
            case LOAD_BURSTFIRE:
                UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 270, 650, " burst ");
                break;
            case LOAD_REVERSE:
                UICharDraw(&UI_State_dyn[2], "sd2", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 270, 650, "reserve");
                break;
        }

        UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[2]);
        _Interactive_data->Referee_Interactive_Flag.shoot_flag = 0;
    }
    // friction
    if (_Interactive_data->Referee_Interactive_Flag.friction_flag == 1)
    {
        UICharDraw(&UI_State_dyn[3], "sd3", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 270, 600, _Interactive_data->friction_mode == FRICTION_ON ? "   on " : "    off");
        UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[3]);
        _Interactive_data->Referee_Interactive_Flag.friction_flag = 0;
    }

    if (_Interactive_data->Referee_Interactive_Flag.speed_flag == 1)
    {
        switch (_Interactive_data->speed_mode)
        {
        case SMALL_AMU_18:
            UICharDraw(&UI_State_dyn[4], "sd4", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 300, 550, " max");
            break;
        case SMALL_AMU_17:
            UICharDraw(&UI_State_dyn[4], "sd4", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 300, 550, " mid");
            break;
        case SMALL_AMU_16:
            UICharDraw(&UI_State_dyn[4], "sd4", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 300, 550, " min");
            break;
        default:
            break;
        }
        UICharRefresh(&referee_recv_info->referee_id, UI_State_dyn[4]);
        _Interactive_data->Referee_Interactive_Flag.speed_flag = 0;
    }

}

/**
 * @brief  模式切换检测,模式发生切换时，对flag置位
 * @param  Referee_Interactive_info_t *_Interactive_data
 * @retval none
 * @attention
 */
static void UIChangeCheck(Referee_Interactive_info_t *_Interactive_data)
{
    if (_Interactive_data->gimbal_mode != _Interactive_data->gimbal_last_mode)
    {
        _Interactive_data->Referee_Interactive_Flag.gimbal_flag = 1;
        _Interactive_data->gimbal_last_mode = _Interactive_data->gimbal_mode;
    }

    if (_Interactive_data->loader_mode != _Interactive_data->loader_last_mode)
    {
        _Interactive_data->Referee_Interactive_Flag.shoot_flag = 1;
        _Interactive_data->loader_last_mode = _Interactive_data->loader_mode;
    }

    if (_Interactive_data->friction_mode != _Interactive_data->friction_last_mode)
    {
        _Interactive_data->Referee_Interactive_Flag.friction_flag = 1;
        _Interactive_data->friction_last_mode = _Interactive_data->friction_mode;
    }

    if (_Interactive_data->speed_mode != _Interactive_data->speed_last_mode)
    {
        _Interactive_data->Referee_Interactive_Flag.speed_flag = 1;
        _Interactive_data->speed_last_mode = _Interactive_data->speed_mode;
    }
}
