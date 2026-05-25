#include "shoot.h"
#include "robot_def.h"

#include "dji_motor.h"
#include "message_center.h"
#include "bsp_dwt.h"
#include "general_def.h"

/* 对于双发射机构的机器人,将下面的数据封装成结构体即可,生成两份shoot应用实例 */
static DJIMotorInstance *friction_l, *friction_r, *loader; // 拨盘电机
// static servo_instance *lid; 需要增加弹舱盖

static Publisher_t *shoot_pub;
static Shoot_Ctrl_Cmd_s shoot_cmd_recv; // 来自cmd的发射控制信息
static Subscriber_t *shoot_sub;
static Shoot_Upload_Data_s shoot_feedback_data; // 来自cmd的发射控制信息
static float f_speed_r = 0.f;
static float f_speed_l = 0.f;

// dwt定时,计算冷却用
// static float hibernate_time = 0, dead_time = 0;

void ShootInit()
{
    // 左摩擦轮
    Motor_Init_Config_s friction_config = {
        .can_init_config = {
            .can_handle = &hcan2,
        },
        .controller_param_init_config = {
            .speed_PID = {
                .Kp = 28.0f, // 20
                .Ki = 2.0f, // 1
                .Kd = 0.001f,
                .Improve = PID_Integral_Limit | PID_OutputFilter,
                .Output_LPF_RC = 0.001f,
                .IntegralLimit = 10000,
                .MaxOut = 14000,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,

            .outer_loop_type_temp = SPEED_LOOP,
            .outer_loop_type_last = SPEED_LOOP,
            .close_loop_type = SPEED_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_REVERSE,
            .speed_unit_type = MOTOR_FEEDBACK_RPM
        },
        .motor_type = M3508};
    friction_config.can_init_config.tx_id = 1,
    friction_l = DJIMotorInit(&friction_config);

    friction_config.controller_param_init_config.speed_PID.Kp = 30.0f;
    friction_config.controller_param_init_config.speed_PID.Ki = 1.0f;
    friction_config.controller_param_init_config.speed_PID.Improve = PID_Integral_Limit;
    friction_config.can_init_config.tx_id = 2; // 右摩擦轮,改txid和方向就行
    friction_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_NORMAL;
    friction_r = DJIMotorInit(&friction_config);

    // 拨盘电机
    Motor_Init_Config_s loader_config = {
        .can_init_config = {
            .can_handle = &hcan2,
            .tx_id = 4,
        },
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 15.f, // 10
                .Ki = 0.f,
                .Kd = 0.5f,
                .IntegralLimit = 3000,
                .MaxOut = 5000,
            },
            .speed_PID = {
                .Kp = 20, // 10
                .Ki = 1, // 1
                .Kd = 0,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = 4000,
                .MaxOut = 8000,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED, .speed_feedback_source = MOTOR_FEED,
            .outer_loop_type_temp = SPEED_LOOP, // 初始化成SPEED_LOOP,让拨盘停在原地,防止拨盘上电时乱转
            .outer_loop_type_last = SPEED_LOOP,
            .close_loop_type = SPEED_LOOP | ANGLE_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL, // 注意方向设置为拨盘的拨出的击发方向
            .speed_unit_type = MOTOR_FEEDBACK_RPM
        },
        .motor_type = M2006
    };
    loader = DJIMotorInit(&loader_config);

    /* 发射链路改为最新值语义，避免旧控制命令在任务周期不匹配时继续排队生效。 */
    shoot_pub = PubRegister("shoot_feed", sizeof(Shoot_Upload_Data_s));
    shoot_sub = SubRegister("shoot_cmd", sizeof(Shoot_Ctrl_Cmd_s));
}

/* 机器人发射机构控制核心任务 */
void ShootTask()
{
    // 从cmd获取控制数据
    SubGetMessage(shoot_sub, &shoot_cmd_recv);

    f_speed_l = friction_l->measure.speed_rpm;
    f_speed_r = -friction_r->measure.speed_rpm;

    // 摩擦轮以及软件缓启动，防止烧毁降压板
    static float friction_soft_start = 0.f;
    static float fric_target_speed = 0.f;
    if (shoot_cmd_recv.friction_mode == FRICTION_OFF)
    {
        if (friction_soft_start > 0.f)
        {
            DJIMotorEnable(friction_l);
            DJIMotorEnable(friction_r);
            friction_soft_start -= 20.0f;
            if (friction_soft_start < 0.0f)
                friction_soft_start = FRIC_STOP;
            DJIMotorSetRef(friction_l, friction_soft_start);
            DJIMotorSetRef(friction_r, friction_soft_start);
        }
        else
        {
            DJIMotorStop(friction_l);
            DJIMotorStop(friction_r);
            friction_soft_start = 0.f;
        }
    }
    else
    {
        DJIMotorEnable(friction_l);
        DJIMotorEnable(friction_r);
        switch (shoot_cmd_recv.bullet_speed)
        {
            case BULLET_SPEED_NONE:
                fric_target_speed = FRIC_SPEED_RPM_COEF * BULLET_SPEED_NONE;
                break;
            case SMALL_AMU_18:
               fric_target_speed = FRIC_SPEED_RPM_COEF * SMALL_AMU_18;
                break;
            case SMALL_AMU_17:
                fric_target_speed = FRIC_SPEED_RPM_COEF * SMALL_AMU_17;
                break;
            case SMALL_AMU_16:
                fric_target_speed = FRIC_SPEED_RPM_COEF * SMALL_AMU_16;
                break;
        }
        if (friction_soft_start < fric_target_speed)
        {
            friction_soft_start += 25.f;
            if (friction_soft_start > fric_target_speed)
                friction_soft_start = fric_target_speed;
        }
        else if (friction_soft_start > fric_target_speed)
        {
            friction_soft_start -= 25.f;
            if (friction_soft_start < fric_target_speed)
                friction_soft_start = fric_target_speed;
        }
        DJIMotorSetRef(friction_l, friction_soft_start + LEFT_OFFSET_SPEED_RPM);
        DJIMotorSetRef(friction_r, friction_soft_start + RIGHT_OFFSET_SPEED_RPM);
    }

    // 拨弹
    static uint8_t is_single_shooting = 0;   // 单次点击鼠标左键只会触发一次单发的标志位
    static float single_target_angle = 0.f;
    static uint8_t is_tribe_shooting = 0;    // 单次点击鼠标左键只会触发一次三发的标志位
    static float tribe_target_angle = 0.f;
    if (shoot_cmd_recv.load_mode == LOAD_STOP)
    {
        is_single_shooting = 0;
        is_tribe_shooting = 0;
        DJIMotorEnable(loader);
        DJIMotorOuterLoop(loader, SPEED_LOOP);
        DJIMotorSetRef(loader, 0);
    }
    else
    {
        DJIMotorEnable(loader);
        switch (shoot_cmd_recv.load_mode)
        {
            case LOAD_1_BULLET:
            {
                if (is_single_shooting == 0)
                {
                    is_single_shooting = 1;
                    is_tribe_shooting = 0;
                    // 逻辑上的正转是编码器的反转，这里加减根据逻辑与编码器是否同向，定颗数的发射都可以这样写
                    single_target_angle = (float)loader->measure.total_angle - ONE_BULLET_DELTA_ANGLE * REDUCTION_RATIO_LOADER;
                }
                DJIMotorOuterLoop(loader, ANGLE_LOOP);
                DJIMotorSetRef(loader, single_target_angle);
                break;
            }
            case LOAD_3_BULLET:
            {
                if (is_tribe_shooting == 0)
                {
                    is_tribe_shooting = 1;
                    is_single_shooting = 0;
                    tribe_target_angle = (float)loader->measure.total_angle - THREE_BULLET_DELTA_ANGLE * REDUCTION_RATIO_LOADER;
                }
                DJIMotorOuterLoop(loader, ANGLE_LOOP);
                DJIMotorSetRef(loader, tribe_target_angle);
                break;
            }
            case LOAD_BURSTFIRE:
            {
                is_single_shooting = 0;
                is_tribe_shooting = 0;
                DJIMotorOuterLoop(loader, SPEED_LOOP);
                DJIMotorSetRef(loader, (float)-shoot_cmd_recv.shoot_freq * 60 * REDUCTION_RATIO_LOADER / 12);
                break;
            }
            case LOAD_REVERSE:
            {
                is_single_shooting = 0;
                is_tribe_shooting = 0;
                DJIMotorOuterLoop(loader, SPEED_LOOP);
                DJIMotorSetRef(loader, LOAD_REVERSR_SPEED);
                break;
            }
            default:
            {
                is_single_shooting = 0;
                is_tribe_shooting = 0;
                DJIMotorOuterLoop(loader, SPEED_LOOP);
                DJIMotorSetRef(loader, 0);
                break;
            }
        }
    }

    // 单发模式主要提供给能量机关激活使用(以及英雄的射击大部分处于单发)
    // if (hibernate_time + dead_time > DWT_GetTimeline_ms())
    //     return;

    shoot_feedback_data.loader_data = loader->measure;
    // 反馈数据,目前暂时没有要设定的反馈数据,后续可能增加应用离线监测以及卡弹反馈
    PubPushMessage(shoot_pub, (void *)&shoot_feedback_data);
}
