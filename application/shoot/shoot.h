#ifndef SHOOT_H
#define SHOOT_H

#define MAX_FRIC_SPEED_RPM 7000
#define FRIC_SPEED_RPM_COEF 320.20741f      // 60 / (PI * 摩擦轮电机直径(m)本例0.061m) * (1 - (过多的计算(0-0.1)) / 摩擦轮效率 (0.8 - 0.9))
#define LEFT_OFFSET_SPEED_RPM 3.0f     // 换用不同摩擦轮电机需要修改此参数，将左右摩擦轮电机曲线尽量重合
#define RIGHT_OFFSET_SPEED_RPM 0.0f    // 换用不同摩擦轮电机需要修改此参数，将左右摩擦轮电机曲线尽量重合
#define FRIC_STOP 0.0f                 // 摩擦轮停转速度
#define LOAD_REVERSR_SPEED 4000        // 卡弹反转速度
#define ONE_BULLET_DELTA_ANGLE 30.f    // 拨出一颗弹丸所需要的角度
#define THREE_BULLET_DELTA_ANGLE 90.f    // 拨出3颗弹丸所需要的角度
#define REDUCTION_RATIO_LOADER 36.0f   // 拨弹盘电机减速比

/**
 * @brief 发射初始化,会被RobotInit()调用
 * 
 */
void ShootInit();

/**
 * @brief 发射任务
 * 
 */
void ShootTask();

#endif // SHOOT_H