// app
#include "robot_def.h"
#include "robot_cmd.h"
// module
#include "remote_control.h"
#include "ins_task.h"
#include "message_center.h"
#include "dji_motor.h"
#include "LK4005.h"
#include "picture_transmission.h"
#include "referee_task.h"

/* cmd应用包含的模块实例指针和交互信息存储*/

static RC_ctrl_t* rc_data;                 // 遥控器数据,初始化时返回
static PictureTransmissionCtrl_s* pt_data; // 图传数据，初始化时返回

static Publisher_t* gimbal_cmd_pub;            // 云台控制消息话题
static Subscriber_t* gimbal_feed_sub;          // 云台反馈信息订阅者
static Gimbal_Ctrl_Cmd_s gimbal_cmd_send;      // 传递给云台的控制信息
static Gimbal_Upload_Data_s gimbal_fetch_data; // 从云台获取的反馈信息

static Publisher_t* shoot_cmd_pub;           // 发射控制消息话题
static Subscriber_t* shoot_feed_sub;         // 发射反馈信息订阅者
static Shoot_Ctrl_Cmd_s shoot_cmd_send;      // 传递给发射的控制信息
static Shoot_Upload_Data_s shoot_fetch_data; // 从发射获取的反馈信息

static System_State_e current_sys_state = SYS_ESTOP;
static System_State_e last_sys_state = SYS_ESTOP;

static referee_info_t *referee_data;
static Referee_Interactive_info_t ui_data;

volatile uint8_t ui_redraw_seq = 0;

static uint8_t detect_time = 0;
static uint8_t reverse_time = 0;
static uint8_t pt_friction_flag = 0;
static uint8_t last_fn_2 = 0;
static loader_mode_e ui_shoot_mode = LOAD_1_BULLET;

static gimbal_mode_e last_gimbal_mode;

/**
 * @brief 控制输入源选择枚举
 */
typedef enum
{
    ROBOT_CMD_INPUT_SOURCE_DBUS = 0,                /**< 使用 DBUS 遥控器链路 */
    ROBOT_CMD_INPUT_SOURCE_PICTURE_TRANSMISSION = 1 /**< 使用图传遥控器链路 */
} RobotCmdInputSource_e;

#define ROBOT_CMD_ACTIVE_INPUT_SOURCE ROBOT_CMD_INPUT_SOURCE_PICTURE_TRANSMISSION /**< 当前启用的遥控链路 */

// BMI088Instance *bmi088_test; // 云台IMU
// BMI088_Data_t bmi088_data;
void RobotCMDInit()
{
    // BMI088_Init_Config_s bmi088_config = {
    //     .cali_mode = BMI088_CALIBRATE_ONLINE_MODE,
    //     .work_mode = BMI088_BLOCK_TRIGGER_MODE,
    //     .spi_acc_config = {
    //         .spi_handle = &hspi1,
    //         .GPIOx = GPIOA,
    //         .cs_pin = GPIO_PIN_4,
    //         .spi_work_mode = SPI_DMA_MODE,
    //     },
    //     .acc_int_config = {
    //         .GPIOx = GPIOC,
    //         .GPIO_Pin = GPIO_PIN_4,
    //         .exti_mode = GPIO_EXTI_MODE_RISING,
    //     },
    //     .spi_gyro_config = {
    //         .spi_handle = &hspi1,
    //         .GPIOx = GPIOB,
    //         .cs_pin = GPIO_PIN_0,
    //         .spi_work_mode = SPI_DMA_MODE,
    //     },
    //     .gyro_int_config = {
    //         .GPIO_Pin = GPIO_PIN_5,
    //         .GPIOx = GPIOC,
    //         .exti_mode = GPIO_EXTI_MODE_RISING,
    //     },
    //     .heat_pwm_config = {
    //         .htim = &htim10,
    //         .channel = TIM_CHANNEL_1,
    //         .period = 1,
    //     },
    //     .heat_pid_config = {
    //         .Kp = 0.5,
    //         .Ki = 0,
    //         .Kd = 0,
    //         .DeadBand = 0.1,
    //         .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
    //         .IntegralLimit = 100,
    //         .MaxOut = 100,
    //     },
    // };
    // bmi088_test = BMI088Register(&bmi088_config);
    rc_data = RemoteControlInit(&huart3);       // 修改为对应串口,注意如果是自研板dbus协议串口需选用添加了反相器的那个
    pt_data = PictureTransmissionInit(&huart6); // C板uart6刚好是3pin线
    referee_data = UITaskInit(&huart1, &ui_data);
    /* 云台链路改为最新值语义，原因是控制任务只需要当前最新目标值，
       没有必要维护历史控制指令队列。 */
    gimbal_cmd_pub = PubRegister("gimbal_cmd", sizeof(Gimbal_Ctrl_Cmd_s));
    gimbal_feed_sub = SubRegister("gimbal_feed", sizeof(Gimbal_Upload_Data_s));
    /* 发射机构同样只关心当前最新控制命令和当前最新反馈状态，
       因而也适合切换到仅保留最新值的消息中心。 */
    shoot_cmd_pub = PubRegister("shoot_cmd", sizeof(Shoot_Ctrl_Cmd_s));
    shoot_feed_sub = SubRegister("shoot_feed", sizeof(Shoot_Upload_Data_s));
}

/**
 * @brief 根据遥控器状态切换控制模式与状态
 *
 */
static void ModeSwitch()
{
    if (switch_is_down(rc_data[TEMP].rc.switch_left) && switch_is_down(rc_data[TEMP].rc.switch_right))
        current_sys_state = SYS_ESTOP;
    else if (switch_is_mid(rc_data[TEMP].rc.switch_left))
        current_sys_state = SYS_RC_CONTROL;
    else if (switch_is_up(rc_data[TEMP].rc.switch_left))
        current_sys_state = SYS_PC_CONTROL;
    else
        current_sys_state = SYS_SAFE_STANDBY;
}

/**
 * @brief 根据图传遥控器模式开关切换控制模式与状态
 */
static void PictureTransmissionModeSwitch()
{
    switch (pt_data[PICTURE_TRANSMISSION_TEMP].rc.mode_sw)
    {
    case 0:
        current_sys_state = SYS_ESTOP;
        break;
    case 1:
        current_sys_state = SYS_RC_CONTROL;
        break;
    case 2:
        current_sys_state = SYS_PC_CONTROL;
        break;
    default:
        current_sys_state = SYS_SAFE_STANDBY;
        break;
    }
}

/**
 * @brief 检测遥控器是否离线，若离线将所有电机关闭
 *
 */
static void RCIsOnLine()
{
    uint8_t input_is_online;

    if (ROBOT_CMD_ACTIVE_INPUT_SOURCE == ROBOT_CMD_INPUT_SOURCE_PICTURE_TRANSMISSION)
    {
        input_is_online = PictureTransmissionIsOnline();
    } else
    {
        input_is_online = RemoteControlIsOnline();
    }

    if (input_is_online)
    {
        return;
    } else
    {
        gimbal_cmd_send.gimbal_mode = GIMBAL_ZERO_FORCE;
        shoot_cmd_send.friction_mode = FRICTION_OFF;
        shoot_cmd_send.load_mode = LOAD_STOP;
        ui_shoot_mode = LOAD_STOP;
    }
}

/**
 * @brief 根据当前选择的输入源切换系统模式
 */
static void InputSourceModeSwitch()
{
    if (ROBOT_CMD_ACTIVE_INPUT_SOURCE == ROBOT_CMD_INPUT_SOURCE_PICTURE_TRANSMISSION)
    {
        PictureTransmissionModeSwitch();
    } else
    {
        ModeSwitch();
    }
}

/**
 * @brief 控制输入为遥控器(调试时)的模式和控制量设置
 * @note  仅调试时使用，最好还是将编码器模式取消
 */
static void RemoteControl()
{
    // 切换模式时防抽搐
    if (last_sys_state != SYS_RC_CONTROL)
    {
        gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
        gimbal_cmd_send.pitch = gimbal_fetch_data.gimbal_imu_data.Pitch;
        last_gimbal_mode = GIMBAL_ZERO_FORCE; // 强制刷新模式
    }

    shoot_cmd_send.bullet_speed = SMALL_AMU_18;

    if (rc_data[TEMP].rc.dial < -500)
        shoot_cmd_send.load_mode = LOAD_BURSTFIRE;
    else if (rc_data[TEMP].rc.dial > 500)
        shoot_cmd_send.load_mode = LOAD_3_BULLET;
    else
        shoot_cmd_send.load_mode = LOAD_STOP;

    // 按照摇杆的输出大小进行角度增量,增益系数需调整
    float yaw_add = 0.0003f * (float)rc_data[TEMP].rc.rocker_l_;
    float pitch_add = 0.0004f * (float)rc_data[TEMP].rc.rocker_r1;
    if (switch_is_down(rc_data[TEMP].rc.switch_right))
    {
        if (last_gimbal_mode != GIMBAL_SELF_MODE)
        {
            gimbal_cmd_send.yaw = gimbal_fetch_data.yaw_motor_angle;
            gimbal_cmd_send.pitch = gimbal_fetch_data.pitch_motor_angle;
            gimbal_cmd_send.gimbal_mode = GIMBAL_SELF_MODE;
            last_gimbal_mode = gimbal_cmd_send.gimbal_mode;
            return;
        }
        gimbal_cmd_send.gimbal_mode = GIMBAL_SELF_MODE;
        gimbal_cmd_send.yaw += yaw_add;
        gimbal_cmd_send.pitch -= pitch_add;

        shoot_cmd_send.friction_mode = FRICTION_OFF;
    } else if (switch_is_mid(rc_data[TEMP].rc.switch_right))
    {
        if (last_gimbal_mode != GIMBAL_GYRO_MODE)
        {
            gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
            gimbal_cmd_send.pitch = gimbal_fetch_data.gimbal_imu_data.Pitch;
            gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;
            last_gimbal_mode = gimbal_cmd_send.gimbal_mode;
            return;
        }
        gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;
        gimbal_cmd_send.yaw -= yaw_add;
        gimbal_cmd_send.pitch -= pitch_add;

        shoot_cmd_send.friction_mode = FRICTION_OFF;
        shoot_cmd_send.load_mode = LOAD_STOP;
    } else if (switch_is_up(rc_data[TEMP].rc.switch_right))
    {
        if (last_gimbal_mode != GIMBAL_GYRO_MODE)
        {
            gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
            gimbal_cmd_send.pitch = gimbal_fetch_data.gimbal_imu_data.Pitch;
            gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;
            last_gimbal_mode = gimbal_cmd_send.gimbal_mode;
            return;
        }
        gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;
        gimbal_cmd_send.yaw -= yaw_add;
        gimbal_cmd_send.pitch -= pitch_add;

        // 堵转检测与反转逻辑
        if (reverse_time > 0)
        {
            shoot_cmd_send.load_mode = LOAD_REVERSE;
            reverse_time--;
            detect_time = 0;
        }

        if (shoot_fetch_data.loader_data.real_current < LOADER_MAX_CURRENT)
            detect_time++;
        else
            detect_time = 0;

        if (detect_time > JAM_DETECT_TIME)
        {
            reverse_time = JAM_REVERSE_TIME;
            detect_time = 0;
            shoot_cmd_send.load_mode = LOAD_REVERSE;
        }

        shoot_cmd_send.friction_mode = FRICTION_ON;
        shoot_cmd_send.shoot_freq = MAX_FREQ;
    }

    // 云台软限位
    if (gimbal_cmd_send.gimbal_mode == GIMBAL_GYRO_MODE)
    {
        gimbal_cmd_send.is_reverse = MOTOR_DIRECTION_REVERSE;
        if (gimbal_cmd_send.pitch > PITCH_MAX_ANGLE_IMU) gimbal_cmd_send.pitch = PITCH_MAX_ANGLE_IMU;
        if (gimbal_cmd_send.pitch < PITCH_MIN_ANGLE_IMU) gimbal_cmd_send.pitch = PITCH_MIN_ANGLE_IMU;
        if (gimbal_fetch_data.yaw_motor_angle > YAW_MAX_ANGLE_CODE - YAW_LIMIT_BAND)
        {
            if (yaw_add > 0)
                gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
        }
        if (gimbal_fetch_data.yaw_motor_angle < YAW_MIN_ANGLE_CODE + YAW_LIMIT_BAND)
        {
            if (yaw_add < 0)
                gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
        }
    } else if (gimbal_cmd_send.gimbal_mode == GIMBAL_SELF_MODE)
    {
        gimbal_cmd_send.is_reverse = MOTOR_DIRECTION_NORMAL;
        if (gimbal_cmd_send.pitch > PITCH_MAX_ANGLE_CODE) gimbal_cmd_send.pitch = PITCH_MAX_ANGLE_CODE;
        if (gimbal_cmd_send.pitch < PITCH_MIN_ANGLE_CODE) gimbal_cmd_send.pitch = PITCH_MIN_ANGLE_CODE;
        if (gimbal_cmd_send.yaw > YAW_MAX_ANGLE_CODE) gimbal_cmd_send.yaw = YAW_MAX_ANGLE_CODE;
        if (gimbal_cmd_send.yaw < YAW_MIN_ANGLE_CODE) gimbal_cmd_send.yaw = YAW_MIN_ANGLE_CODE;
    }

    last_gimbal_mode = gimbal_cmd_send.gimbal_mode;
}

/**
 * @brief 根据图传遥控器独立按键更新发射机构控制量
 * @note  图传遥控器模式下固定使用 IMU 反馈控制，发射控制优先级为 pause > trigger > wheel
 */
static void PictureTransmissionShootControl()
{
    uint8_t fn_1_last = pt_data[PICTURE_TRANSMISSION_LAST].rc.fn_1;
    uint8_t fn_1_temp = pt_data[PICTURE_TRANSMISSION_TEMP].rc.fn_1;
    uint8_t pause_last = pt_data[PICTURE_TRANSMISSION_LAST].rc.pause;
    uint8_t pause_temp = pt_data[PICTURE_TRANSMISSION_TEMP].rc.pause;
    int16_t wheel = pt_data[PICTURE_TRANSMISSION_TEMP].rc.wheel;

    shoot_cmd_send.bullet_speed = SMALL_AMU_18;
    shoot_cmd_send.shoot_freq = MAX_FREQ;

    if ((fn_1_last == 0) && (fn_1_temp == 1))
    {
        /* fn_1 按下沿切换摩擦轮开关状态。 */
        pt_friction_flag = (uint8_t)!pt_friction_flag;
    }

    if ((pause_last == 1) && (pause_temp == 1))
    {
        /* pause 长按期间强制关闭摩擦轮并停止拨弹。 */
        shoot_cmd_send.friction_mode = FRICTION_OFF;
        shoot_cmd_send.load_mode = LOAD_STOP;
        detect_time = 0;
        reverse_time = 0;
        return;
    }

    shoot_cmd_send.friction_mode = (pt_friction_flag != 0) ? FRICTION_ON : FRICTION_OFF;

    if (shoot_cmd_send.friction_mode == FRICTION_OFF)
    {
        shoot_cmd_send.load_mode = LOAD_STOP;
        detect_time = 0;
        reverse_time = 0;
        return;
    }

    if (reverse_time > 0)
    {
        shoot_cmd_send.load_mode = LOAD_REVERSE;
        reverse_time--;
        detect_time = 0;
        return;
    }

    if (pt_data[PICTURE_TRANSMISSION_TEMP].rc.trigger != 0)
    {
        /* trigger 高电平期间保持单发模式，具体只打一发由 shoot 模块的模式切换逻辑保证。 */
        shoot_cmd_send.load_mode = LOAD_1_BULLET;
    } else if (wheel > 500)
    {
        shoot_cmd_send.load_mode = LOAD_BURSTFIRE;
    } else if (wheel < -500)
    {
        shoot_cmd_send.load_mode = LOAD_3_BULLET;
    } else
    {
        shoot_cmd_send.load_mode = LOAD_STOP;
    }

    if (shoot_cmd_send.load_mode == LOAD_BURSTFIRE || shoot_cmd_send.load_mode == LOAD_1_BULLET || shoot_cmd_send.load_mode == LOAD_3_BULLET)
    {
        if (shoot_fetch_data.loader_data.real_current < LOADER_MAX_CURRENT)
        {
            detect_time++;
        } else
        {
            detect_time = 0;
        }
    } else
    {
        detect_time = 0;
    }

    if (detect_time > JAM_DETECT_TIME)
    {
        reverse_time = JAM_REVERSE_TIME;
        detect_time = 0;
        shoot_cmd_send.load_mode = LOAD_REVERSE;
    }
}

/**
 * @brief 图传链路遥控器模式下的模式和控制量设置
 * @note  图传遥控器模式下固定使用 IMU 反馈控制云台
 */
static void PictureTransmissionRemoteControl()
{
    float yaw_add = 0.0003f * (float)pt_data[PICTURE_TRANSMISSION_TEMP].rc.rocker_l_;
    float pitch_add = 0.0004f * (float)pt_data[PICTURE_TRANSMISSION_TEMP].rc.rocker_r1;

    if (last_sys_state == SYS_SAFE_STANDBY || last_sys_state == SYS_ESTOP)
    {
        gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
        gimbal_cmd_send.pitch = gimbal_fetch_data.gimbal_imu_data.Pitch;
    }

    gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;
    gimbal_cmd_send.yaw -= yaw_add;
    gimbal_cmd_send.pitch -= pitch_add;

    PictureTransmissionShootControl();

    gimbal_cmd_send.is_reverse = MOTOR_DIRECTION_REVERSE;
    if (gimbal_cmd_send.pitch > PITCH_MAX_ANGLE_IMU) gimbal_cmd_send.pitch = PITCH_MAX_ANGLE_IMU;
    if (gimbal_cmd_send.pitch < PITCH_MIN_ANGLE_IMU) gimbal_cmd_send.pitch = PITCH_MIN_ANGLE_IMU;
    if (gimbal_fetch_data.yaw_motor_angle > YAW_MAX_ANGLE_CODE)
    {
        if (yaw_add > 0)
            gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
    }
    if (gimbal_fetch_data.yaw_motor_angle < YAW_MIN_ANGLE_CODE)
    {
        if (yaw_add < 0)
            gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
    }

    last_gimbal_mode = gimbal_cmd_send.gimbal_mode;
}

/**
 * @brief 输入为键鼠时模式和控制量设置
 *
 */
static void MouseKeyControl()
{
    static uint8_t selected_fire_mode = 0;
    // 切换模式时防抽搐
    if (last_sys_state != SYS_PC_CONTROL)
    {
        gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
        gimbal_cmd_send.pitch = gimbal_fetch_data.gimbal_imu_data.Pitch;
    }

    // 键鼠强制IMU控制

    if (last_gimbal_mode != GIMBAL_GYRO_MODE)
    {
        gimbal_cmd_send.is_reverse = MOTOR_DIRECTION_REVERSE;
        gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
        gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.Pitch;
        gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;
    }

    // 0: 单发, 1: 连发, 2: 手动反转, 3: 停止(E键切换)
    selected_fire_mode = rc_data[TEMP].key_count[KEY_PRESS][Key_E] % 4;
    switch (selected_fire_mode)
    {
    case 0:
        ui_shoot_mode = LOAD_1_BULLET;
        break;
    case 1:
        ui_shoot_mode = LOAD_3_BULLET;
        break;
    case 2:
        ui_shoot_mode = LOAD_BURSTFIRE;
        break;
    default:
        ui_shoot_mode = LOAD_STOP;
        break;
    }

    // 摩擦轮开关 (F键切换)
    if (rc_data[TEMP].key_count[KEY_PRESS][Key_F] % 2 != 0)
        shoot_cmd_send.friction_mode = FRICTION_ON;
    else
        shoot_cmd_send.friction_mode = FRICTION_OFF;

    // 3. 发射执行与卡弹逻辑 (鼠标左键触发)
    if (shoot_cmd_send.friction_mode == FRICTION_ON)
    {
        shoot_cmd_send.shoot_freq = MAX_FREQ;

        // 如果在自动反转退弹中，直接覆写所有玩家指令
        if (reverse_time > 0)
        {
            shoot_cmd_send.load_mode = LOAD_REVERSE;
            reverse_time--;
            detect_time = 0;
        } else
        {
            if (rc_data[TEMP].mouse.press_l == 1)
            {
                switch (selected_fire_mode)
                {
                case 0:
                    shoot_cmd_send.load_mode = LOAD_1_BULLET;
                    break;
                case 1:
                    shoot_cmd_send.load_mode = LOAD_3_BULLET;
                    break;
                case 2:
                    shoot_cmd_send.load_mode = LOAD_BURSTFIRE;
                    break;
                default:
                    shoot_cmd_send.load_mode = LOAD_STOP;
                    break;
                }

                if (rc_data[TEMP].key_count[KEY_PRESS][Key_G] % 2 == 1)
                {
                    shoot_cmd_send.load_mode = LOAD_REVERSE;
                }
            } else
            {
                // 松开左键，立即停止拨弹
                shoot_cmd_send.load_mode = LOAD_STOP;
            }

            // 堵转检测逻辑
            if ((shoot_cmd_send.load_mode == LOAD_BURSTFIRE || shoot_cmd_send.load_mode == LOAD_1_BULLET) && shoot_fetch_data.loader_data.real_current < LOADER_MAX_CURRENT)
                detect_time++;
            else
                detect_time = 0;
            if (detect_time > JAM_DETECT_TIME)
            {
                reverse_time = JAM_REVERSE_TIME;
                detect_time = 0;
                shoot_cmd_send.load_mode = LOAD_REVERSE;
            }
        }
    } else
    {
        // 如果摩擦轮没开，强制断开拨弹并清空卡弹时间
        shoot_cmd_send.load_mode = LOAD_STOP;
        detect_time = 0;
        reverse_time = 0;
    }

    float yaw_add = -(float)rc_data[TEMP].mouse.x / 660.0f * 1.0f;
    float pitch_add = -(float)rc_data[TEMP].mouse.y / 660.0f * 0.7f;

    // 云台灵敏度降低3倍，有自瞄后改为自瞄模式
    if (rc_data[TEMP].mouse.press_r == 1)
    {
        yaw_add /= 3.0f;
        pitch_add /= 3.0f;
    }

    // 由于imu安装问题，所有正方向与电机的旋转正方向相反
    gimbal_cmd_send.yaw -= yaw_add;
    gimbal_cmd_send.pitch -= pitch_add;

    // 云台软限位
    gimbal_cmd_send.is_reverse = MOTOR_DIRECTION_REVERSE;
    if (gimbal_cmd_send.pitch > PITCH_MAX_ANGLE_IMU) gimbal_cmd_send.pitch = PITCH_MAX_ANGLE_IMU;
    if (gimbal_cmd_send.pitch < PITCH_MIN_ANGLE_IMU) gimbal_cmd_send.pitch = PITCH_MIN_ANGLE_IMU;
    if (gimbal_fetch_data.yaw_motor_angle > YAW_MAX_ANGLE_CODE - YAW_LIMIT_BAND)
    {
        if (yaw_add > 0)
            gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
    }
    if (gimbal_fetch_data.yaw_motor_angle < YAW_MIN_ANGLE_CODE + YAW_LIMIT_BAND)
    {
        if (yaw_add < 0)
            gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
    }

    last_gimbal_mode = gimbal_cmd_send.gimbal_mode;
}

/**
 * @brief 图传链路键鼠模式下的模式和控制量设置
 * @note  保持与现有 DBUS 键鼠控制逻辑一致，仅将输入源替换为图传链路键鼠数据
 */
static void PictureTransmissionMouseKeyControl()
{
    static uint8_t selected_fire_mode = 0;
    static uint8_t selected_bullet_speed = 0;
    float yaw_add = (float)pt_data[PICTURE_TRANSMISSION_TEMP].mouse.x / 660.0f * 1.3f;
    float pitch_add = (float)pt_data[PICTURE_TRANSMISSION_TEMP].mouse.y / 660.0f * 1.1f;

    gimbal_cmd_send.is_reverse = MOTOR_DIRECTION_REVERSE;
    gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;

    selected_fire_mode = pt_data[PICTURE_TRANSMISSION_TEMP].key_count[PICTURE_TRANSMISSION_KEY_PRESS][PICTURE_TRANSMISSION_KEY_E] % 3;
    switch (selected_fire_mode)
    {
    case 0:
        ui_shoot_mode = LOAD_1_BULLET;
        break;
    case 1:
        ui_shoot_mode = LOAD_3_BULLET;
        break;
    default:
        ui_shoot_mode = LOAD_BURSTFIRE;
        break;
    }

    selected_bullet_speed = pt_data[PICTURE_TRANSMISSION_TEMP].key_count[PICTURE_TRANSMISSION_KEY_PRESS][PICTURE_TRANSMISSION_KEY_Q] % 3;
    switch (selected_bullet_speed)
    {
    case 0:
        shoot_cmd_send.bullet_speed = SMALL_AMU_18;
        break;
    case 1:
        shoot_cmd_send.bullet_speed = SMALL_AMU_17;
        break;
    default:
        shoot_cmd_send.bullet_speed = SMALL_AMU_16;
        break;
    }

    if (pt_data[PICTURE_TRANSMISSION_TEMP].key_count[PICTURE_TRANSMISSION_KEY_PRESS][PICTURE_TRANSMISSION_KEY_R] % 2 != 0)
        shoot_cmd_send.friction_mode = FRICTION_ON;
    else
        shoot_cmd_send.friction_mode = FRICTION_OFF;

    if (shoot_cmd_send.friction_mode == FRICTION_ON)
    {
        shoot_cmd_send.shoot_freq = MAX_FREQ;

        if (reverse_time > 0)
        {
            shoot_cmd_send.load_mode = LOAD_REVERSE;
            reverse_time--;
            detect_time = 0;
        } else
        {
            if (pt_data[PICTURE_TRANSMISSION_TEMP].mouse.press_left == 1)
            {
                switch (selected_fire_mode)
                {
                case 0:
                    shoot_cmd_send.load_mode = LOAD_1_BULLET;
                    break;
                case 1:
                    shoot_cmd_send.load_mode = LOAD_3_BULLET;
                    break;
                case 2:
                    shoot_cmd_send.load_mode = LOAD_BURSTFIRE;
                    break;
                }

            }
            else
            {
                shoot_cmd_send.load_mode = LOAD_STOP;
            }

            if ((shoot_cmd_send.load_mode == LOAD_BURSTFIRE
                || shoot_cmd_send.load_mode == LOAD_1_BULLET
                || shoot_cmd_send.load_mode == LOAD_3_BULLET)
                && shoot_fetch_data.loader_data.real_current < LOADER_MAX_CURRENT)
                detect_time++;
            else
                detect_time = 0;
            if (detect_time > JAM_DETECT_TIME)
            {
                reverse_time = JAM_REVERSE_TIME;
                detect_time = 0;
                shoot_cmd_send.load_mode = LOAD_REVERSE;
            }
        }
    } else
    {
        shoot_cmd_send.load_mode = LOAD_STOP;
        detect_time = 0;
        reverse_time = 0;
    }

    if (pt_data[PICTURE_TRANSMISSION_TEMP].mouse.press_right == 1)
    {
        yaw_add /= 3.0f;
        pitch_add /= 3.0f;
    }

    gimbal_cmd_send.yaw -= yaw_add;
    gimbal_cmd_send.pitch -= pitch_add;

    gimbal_cmd_send.is_reverse = MOTOR_DIRECTION_REVERSE;
    if (gimbal_cmd_send.pitch > PITCH_MAX_ANGLE_IMU) gimbal_cmd_send.pitch = PITCH_MAX_ANGLE_IMU;
    if (gimbal_cmd_send.pitch < PITCH_MIN_ANGLE_IMU) gimbal_cmd_send.pitch = PITCH_MIN_ANGLE_IMU;
    if (gimbal_fetch_data.yaw_motor_angle > YAW_MAX_ANGLE_CODE - YAW_LIMIT_BAND)
    {
        if (yaw_add > 0)
            gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
    }
    if (gimbal_fetch_data.yaw_motor_angle < YAW_MIN_ANGLE_CODE + YAW_LIMIT_BAND)
    {
        if (yaw_add < 0)
            gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
    }
    last_gimbal_mode = gimbal_cmd_send.gimbal_mode;
}

/**
 * @brief 根据当前选择的输入源执行遥控器模式控制逻辑
 */
static void InputSourceRemoteControl()
{
    if (ROBOT_CMD_ACTIVE_INPUT_SOURCE == ROBOT_CMD_INPUT_SOURCE_PICTURE_TRANSMISSION)
    {
        PictureTransmissionRemoteControl();
    } else
    {
        RemoteControl();
    }
}

/**
 * @brief 根据当前选择的输入源执行键鼠模式控制逻辑
 */
static void InputSourceMouseKeyControl()
{
    if (ROBOT_CMD_ACTIVE_INPUT_SOURCE == ROBOT_CMD_INPUT_SOURCE_PICTURE_TRANSMISSION)
    {
        PictureTransmissionMouseKeyControl();
    } else
    {
        MouseKeyControl();
    }
}

/**
 * @brief 根据裁判系统反馈的剩余热量做强制限制
 */
static void ColorrificRestriction()
{
    if (referee_data->GameRobotState.shooter_barrel_heat_limit
        - referee_data->PowerHeatData.shooter_17mm_1_barrel_heat < 60)
        shoot_cmd_send.load_mode = LOAD_STOP;
}

/* 机器人核心控制任务,200Hz频率运行(必须高于视觉发送频率) */
// DT7
// 左拨杆
// UP (上) = 键鼠模式 (PC)
// MID (中) = 遥控器模式 (RC)
// DOWN (下) = 云台关闭/待机 (Safe)
// 右拨杆(仅RC模式下生效)
// UP (上) = 摩擦轮开 + 允许拨弹 + 云台陀螺仪增稳
// MID (中) = 摩擦轮开 + 禁止拨弹 + 云台陀螺仪增稳
// DOWN (下) = 摩擦轮关 + 禁止拨弹 + 云台编码器跟随
// 两侧拨杆都为DOWN时急停

// VT03
// 中拨钮
// 左 = Safe
// 中 = RC
// 右 = PC
void RobotCMDTask()
{
    // BMI088Acquire(bmi088_test,&bmi088_data) ;
    SubGetMessage(shoot_feed_sub, &shoot_fetch_data);
    SubGetMessage(gimbal_feed_sub, &gimbal_fetch_data);

    InputSourceModeSwitch();
    switch (current_sys_state)
    {
    case SYS_ESTOP:
    case SYS_SAFE_STANDBY:
        gimbal_cmd_send.gimbal_mode = GIMBAL_ZERO_FORCE;
        shoot_cmd_send.friction_mode = FRICTION_OFF;
        shoot_cmd_send.load_mode = LOAD_STOP;
        break;
    case SYS_RC_CONTROL:
        InputSourceRemoteControl();
        break;
    case SYS_PC_CONTROL:
        InputSourceMouseKeyControl();
        break;
    }
    // 多一层判断，只要摩擦轮不开就不能拨弹
    ColorrificRestriction();
    if (shoot_cmd_send.friction_mode == FRICTION_OFF)
        shoot_cmd_send.load_mode = LOAD_STOP;

    // 遥控器离线处理
    RCIsOnLine();

    ui_data.gimbal_mode = gimbal_cmd_send.gimbal_mode;
    ui_data.friction_mode = shoot_cmd_send.friction_mode;
    ui_data.loader_mode = ui_shoot_mode;
    ui_data.speed_mode = shoot_cmd_send.bullet_speed;

    // 刷新UI
    if (pt_data[PICTURE_TRANSMISSION_TEMP].rc.fn_2 == 1 && last_fn_2 == 0)
    {
        ui_redraw_seq++;
    }
    last_fn_2 = pt_data[PICTURE_TRANSMISSION_TEMP].rc.fn_2;

    last_sys_state = current_sys_state;

    PubPushMessage(shoot_cmd_pub, (void*)&shoot_cmd_send);
    PubPushMessage(gimbal_cmd_pub, (void*)&gimbal_cmd_send);
}
