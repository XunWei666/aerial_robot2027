#include "LK4005.h"
#include "stdlib.h"
#include "general_def.h"
#include "daemon.h"
#include "bsp_dwt.h"
#include "bsp_log.h"
#include "usart.h"

static uint8_t idx;
static LKMotorInstance *lkmotor_instance[LK_MOTOR_MX_CNT] = {NULL};
static CANInstance *sender_instance; // 多电机发送时使用的caninstance(当前保存的是注册的第一个电机的caninstance)
// static char Data[64];

/**
 * @brief 电机反馈报文解析
 *
 * @param _instance 发生中断的caninstance
 */
static void LKMotorDecode(CANInstance *_instance)
{
    LKMotorInstance *motor = (LKMotorInstance *)_instance->id; // 通过caninstance保存的father id获取对应的motorinstance
    LKMotor_Measure_t *measure = &motor->measure;
    uint8_t *rx_buff = _instance->rx_buff;

    DaemonReload(motor->daemon); // 喂狗
    measure->feed_dt = DWT_GetDeltaT(&measure->feed_dwt_cnt);

    measure->last_ecd = measure->ecd;
    measure->ecd = (uint16_t)((rx_buff[7] << 8) | rx_buff[6]);

    measure->angle_single_round = ECD_ANGLE_COEF_LK * measure->ecd;

    measure->speed_rads = (1 - SPEED_SMOOTH_COEF) * measure->speed_rads +
                          DEGREE_2_RAD * SPEED_SMOOTH_COEF * (float)((int16_t)(rx_buff[5] << 8 | rx_buff[4]));

    measure->real_current = (1 - CURRENT_SMOOTH_COEF) * measure->real_current +
                            CURRENT_SMOOTH_COEF * (float)((int16_t)(rx_buff[3] << 8 | rx_buff[2]));

    measure->temperature = rx_buff[1];

    if (measure->init_flag == 0)
    {
        if (measure->ecd - measure->last_ecd > 32767)
            measure->total_round--;
        else if (measure->ecd - measure->last_ecd < -32768)
            measure->total_round++;
        measure->total_angle = measure->total_round * 360 + measure->angle_single_round;
    }
    else
    {
        measure->init_flag = 0;
        measure->total_angle = measure->angle_single_round;
    }

}

static void LKmotorPIDClear(LKMotorInstance *instance)
{
    instance->angle_PID.Pout = 0;
    instance->speed_PID.Pout = 0;
    instance->current_PID.Pout = 0;
    instance->angle_PID.Iout = 0;
    instance->speed_PID.Iout = 0;
    instance->current_PID.Iout = 0;
    instance->angle_PID.Dout = 0;
    instance->speed_PID.Dout = 0;
    instance->current_PID.Dout = 0;
    instance->angle_PID.Output = 0;
    instance->speed_PID.Output = 0;
    instance->current_PID.Output = 0;
}

static void LKMotorLostCallback(void *motor_ptr)
{
    LKMotorInstance *motor = (LKMotorInstance *)motor_ptr;
    LOGWARNING("[LKMotor] motor lost, id: %d", motor->motor_can_ins->tx_id);
}

LKMotorInstance *LKMotorInit(Motor_Init_Config_s *config)
{
    LKMotorInstance *motor = (LKMotorInstance *)malloc(sizeof(LKMotorInstance));
    // motor = (LKMotorInstance *)malloc(sizeof(LKMotorInstance));
    memset(motor, 0, sizeof(LKMotorInstance));

    motor->measure.init_flag = 1;
    motor->motor_settings = config->controller_setting_init_config;
    PIDInit(&motor->current_PID, &config->controller_param_init_config.current_PID);
    PIDInit(&motor->speed_PID, &config->controller_param_init_config.speed_PID);
    PIDInit(&motor->angle_PID, &config->controller_param_init_config.angle_PID);
    motor->other_angle_feedback_ptr = config->controller_param_init_config.other_angle_feedback_ptr;
    motor->other_speed_feedback_ptr = config->controller_param_init_config.other_speed_feedback_ptr;
    motor->gravity = config->controller_param_init_config.gravity;

    config->can_init_config.id = motor;
    config->can_init_config.can_module_callback = LKMotorDecode;
    config->can_init_config.rx_id = 0x140 + config->can_init_config.tx_id;
    config->can_init_config.tx_id = 0x141; // 这样在发送写入buffer的时候更方便,因为下标从0开始,LK多电机发送id为0x280
    motor->motor_can_ins = CANRegister(&config->can_init_config);

    if (idx == 0) // 用第一个电机的can instance发送数据
    {
        sender_instance = motor->motor_can_ins;
        sender_instance->tx_id = 0x141; //  修改tx_id为0x141,空中机器人仅使用一个瓴控电机4005，根据实际情况改变ID
    }

    LKMotorEnable(motor);
    DWT_GetDeltaT(&motor->measure.feed_dwt_cnt);
    lkmotor_instance[idx++] = motor;

    Daemon_Init_Config_s daemon_config = {
        .callback = LKMotorLostCallback,
        .owner_id = motor,
        .reload_count = 5, // 50ms
    };
    motor->daemon = DaemonRegister(&daemon_config);

    return motor;
}

void LKMotorChangeFeed(LKMotorInstance *motor, Closeloop_Type_e loop, Feedback_Source_e type)
{
    if (loop == ANGLE_LOOP)
    {
        motor->motor_settings.last_angle_feedback_source = motor->motor_settings.angle_feedback_source;
        motor->motor_settings.angle_feedback_source = type;
    }
    else if (loop == SPEED_LOOP)
    {
        motor->motor_settings.last_speed_feedback_source = motor->motor_settings.speed_feedback_source;
        motor->motor_settings.speed_feedback_source = type;
    }
}

static void LKMotorFillMailBox(uint8_t command, int16_t set)
{
    sender_instance->tx_buff[0] = command;
    sender_instance->tx_buff[1] = 0x00;
    sender_instance->tx_buff[2] = 0x00;
    sender_instance->tx_buff[3] = 0x00;
    sender_instance->tx_buff[4] = set;
    sender_instance->tx_buff[5] = set >> 8;
    sender_instance->tx_buff[6] = 0x00;
    sender_instance->tx_buff[7] = 0x00;
}

/* 第一个电机的can instance用于发送数据,向其tx_buff填充数据 */
// 其实只有一个瓴控电机
void LKMotorControl()
{
    float pid_measure, pid_ref;
    int16_t set;
    LKMotorInstance *motor;
    LKMotor_Measure_t *measure;
    Motor_Control_Setting_s *setting;

    for (size_t i = 0; i < idx; ++i)
    {
        motor = lkmotor_instance[i];
        measure = &motor->measure;
        setting = &motor->motor_settings;
        pid_ref = motor->pid_ref;
        if (motor->current_flag == MOTOR_ENALBED && motor->last_flag == MOTOR_STOP) {
            if (idx) // 如果有电机注册了
            {
                CANTransmit(sender_instance, 0.2f);
                LKMotorFillMailBox(LK_MOTOR_ENABLE, 0);
            }
            motor->angle_PID.Iout = 0;
            motor->speed_PID.Iout = 0;
        }

        if (motor->motor_settings.angle_feedback_source != motor->motor_settings.last_angle_feedback_source  || motor->motor_settings.speed_feedback_source != motor->motor_settings.last_speed_feedback_source)
        {
            LKmotorPIDClear(motor);
        }

        if (setting->motor_reverse_flag == MOTOR_DIRECTION_REVERSE)
            pid_ref *= -1;

        if ((setting->close_loop_type & ANGLE_LOOP) && setting->outer_loop_type_temp == ANGLE_LOOP)
        {
            if (setting->angle_feedback_source == IMU_FEED)
                pid_measure = *motor->other_angle_feedback_ptr;
            else
                pid_measure = measure->total_angle;
            pid_ref = PIDCalculate(&motor->angle_PID, pid_measure, pid_ref);
            if (setting->feedforward_flag & SPEED_FEEDFORWARD)
                pid_ref += *motor->speed_feedforward_ptr;
        }

        if ((setting->close_loop_type & SPEED_LOOP) && setting->outer_loop_type_temp & (ANGLE_LOOP | SPEED_LOOP))
        {
            if (setting->speed_feedback_source == IMU_FEED)
                pid_measure = *motor->other_speed_feedback_ptr;
            else
                pid_measure = measure->speed_rpm;
            pid_ref = PIDCalculate(&motor->speed_PID, pid_measure, pid_ref);
        }

        pid_ref += motor->gravity;
        set = (int16_t)pid_ref;

        LKMotorFillMailBox(LK_MOTOR_TORQUE, set);

        if (motor->current_flag == MOTOR_STOP)
        { // 若该电机处于停止状态,直接将发送buff置零
            LKMotorFillMailBox(LK_MOTOR_TORQUE, 0);
        }
    }

    if (idx) // 如果有电机注册了
    {
        CANTransmit(sender_instance, 0.2f);
    }
}

void LKMotorStop(LKMotorInstance *motor)
{
    motor->last_flag = motor->current_flag;
    motor->current_flag = MOTOR_STOP;
}

void LKMotorEnable(LKMotorInstance *motor)
{
    motor->last_flag = motor->current_flag;
    motor->current_flag = MOTOR_ENALBED;
}

void LKMotorSetRef(LKMotorInstance *motor, float ref)
{
    motor->pid_ref = ref;
}

uint8_t LKMotorIsOnline(LKMotorInstance *motor)
{
    return DaemonIsOnline(motor->daemon);
}
