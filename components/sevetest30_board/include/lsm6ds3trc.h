/*
 * 701Enti MIT License
 *
 * Copyright © 2024 <701Enti organization>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the “Software”),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

 // 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，包含各种SE30对姿态传感器LSM6DS3TR-C的操作
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
 //       对于功能设计需求，并不是所有寄存器都被考虑，都要求配置，实际上可以遵从默认设置
 // 您可以在ST官网获取LSM6DS3TR-C的相关手册包含程序实现思路 https://www.st.com/zh/mems-and-sensors/lsm6ds3tr-c.html
 // github: https://github.com/701Enti
 // bilibili: 701Enti

 // 基本初始化任务是 设置加速度计和陀螺仪的ORD（为了获得中断输出不得设置加速度计到掉电模式)，以及XL_HM_MODE位的设置
 //             启用BDU和DRDY_MASK 配置INT1 配置自动记录相关模块
 //             设置FIFO抽取系数 设置FIFO_ORD   设置 加速度 角速率 数据源到FIFO   配置为FIFO Continuous mode模式 不使用中断
 // 读取步骤是  读出FIFO存储的数据 / 读出自动记录的数据

 //   快捷监测 6D检测 自由落体检测 计步器(必须 ORD 26Hz+)

#pragma once

#include "esp_types.h"

//LSB对应实际的值
//加速度计 mg/LSB (1 x 10^-3 g/LSB)
#define IMU_LA_SO_FS_2G  0.061f
#define IMU_LA_SO_FS_4G  0.122f
#define IMU_LA_SO_FS_8G  0.244f
#define IMU_LA_SO_FS_16G 0.488f
//陀螺仪(角速率测量)  mdps/LSB (1 x 10^-3 dps/LSB)
#define IMU_G_SO_FS_125DPS  4.375f
#define IMU_G_SO_FS_250DPS  8.75f
#define IMU_G_SO_FS_500DPS  17.50f
#define IMU_G_SO_FS_1000DPS 35.0f
#define IMU_G_SO_FS_2000DPS 70.0f

//寄存器地址库
enum
{
  FUNC_CFG_ACCESS = 0x01,
  SENSOR_SYNC_TIME_FRAME = 0x04,
  SENSOR_SYNC_RES_RATIO,
  FIFO_CTRL1,
  FIFO_CTRL2,
  FIFO_CTRL3,
  FIFO_CTRL4,
  FIFO_CTRL5,
  DRDY_PULSE_CFG_G,
  INT1_CTRL = 0x0D,
  INT2_CTRL,
  WHO_AM_I,
  CTRL1_XL,
  CTRL2_G,
  CTRL3_C,
  CTRL4_C,
  CTRL5_C,
  CTRL6_C,
  CTRL7_G,
  CTRL8_XL,
  CTRL9_XL,
  CTRL10_C,

  MASTER_CONFIG,
  WAKE_UP_SRC,
  TAP_SRC,
  D6D_SRC,
  STATUS_REG,
  OUT_TEMP_L = 0x20,
  OUT_TEMP_H,
  OUTX_L_G,
  OUTX_H_G,
  OUTY_L_G,
  OUTY_H_G,
  OUTZ_L_G,
  OUTZ_H_G,
  OUTX_L_XL,
  OUTX_H_XL,
  OUTY_L_XL,
  OUTY_H_XL,
  OUTZ_L_XL,
  OUTZ_H_XL,
  SENSORHUB1_REG,
  SENSORHUB2_REG,
  SENSORHUB3_REG,
  SENSORHUB4_REG,
  SENSORHUB5_REG,
  SENSORHUB6_REG,
  SENSORHUB7_REG,
  SENSORHUB8_REG,
  SENSORHUB9_REG,
  SENSORHUB10_REG,
  SENSORHUB11_REG,
  SENSORHUB12_REG,

  FIFO_STATUS1,
  FIFO_STATUS2,
  FIFO_STATUS3,
  FIFO_STATUS4,
  FIFO_DATA_OUT_L,
  FIFO_DATA_OUT_H,
  TIMESTAMP0_REG,
  TIMESTAMP1_REG,
  TIMESTAMP2_REG,
  STEP_TIMESTAMP_L = 0x49,
  STEP_TIMESTAMP_H,
  STEP_COUNTER_L,
  STEP_COUNTER_H,
  SENSORHUB13_REG,
  SENSORHUB14_REG,
  SENSORHUB15_REG,
  SENSORHUB16_REG,
  SENSORHUB17_REG,
  SENSORHUB18_REG,
  FUNC_SRC1,
  FUNC_SRC2,
  WRIST_TILT_IA,
  TAP_CFG = 0x58,
  TAP_THS_6D,
  INT_DUR2,
  WAKE_UP_THS,
  WAKE_UP_DUR,
  FREE_FALL,
  MD1_CFG,
  MD2_CFG,
  MASTER_CMD_CODE,

  SENS_SYNC_SPI_ERROR_CODE,
  OUT_MAG_RAW_X_L = 0x66,
  OUT_MAG_RAW_X_H,
  OUT_MAG_RAW_Y_L,
  OUT_MAG_RAW_Y_H,
  OUT_MAG_RAW_Z_L,
  OUT_MAG_RAW_Z_H,
  X_OFS_USR = 0x73,
  Y_OFS_USR,
  Z_OFS_USR,
};

//部分嵌入式功能寄存器地址库
//对嵌入式功能寄存器内容的所有修改都必须在断电模式下执行
enum{
  CONFIG_PEDO_THS_MIN = 0x0F,
};

//*****************************传感数据相关********************************/
typedef struct {
  int x;
  int y;
  int z;
}IMU_acceleration_value_t;//加速度值 单位mg 即 1 x 10^-3 g

typedef struct {
  int x;
  int y;
  int z;
}IMU_angular_rate_value_t;//角速度值 单位为mdps 即 1 x 10^-3 dps

typedef struct {
  bool XL;//X轴偏低
  bool XH;//X轴偏高
  bool YL;//Y轴偏低
  bool YH;//Y轴偏高
  bool ZL;//Z轴偏低
  bool ZH;//Z轴偏高
}IMU_D6D_data_value_t;//D6D方向监测 方向偏移标识
//***********************************用户配置相关***********************************/
typedef enum
{
  IMU_FS_XL_2G,       // ±2g
  IMU_FS_XL_16G,      // ±16g  
  IMU_FS_XL_4G,       // ±4g
  IMU_FS_XL_8G,       // ±8g
} IMU_FS_XL_t; // 加速度计(加速度测量)量程选择 g:重力加速度,约为g=9.780米/秒^2

typedef enum
{
  IMU_FS_G_125DPS = 0x01,   // ±125dps
  IMU_FS_G_250DPS = 0x00,   // ±250dps
  IMU_FS_G_500DPS = 0x02,   // ±500dps
  IMU_FS_G_1000DPS = 0x04,  // ±1000dps
  IMU_FS_G_2000DPS = 0x06,  // ±2000dps
} IMU_FS_G_t; // 陀螺仪(角速率测量)量程选择 dps:角速率 表示 度/秒

typedef enum {
  IMU_ORD_XL_POWER_DOWN = 0x00,
  //             当 XL_HM_MODE = true          | 当 XL_HM_MODE = false
  IMU_ORD_XL_1,//12.5 Hz (low power)           |  12.5 Hz (high performance)
  IMU_ORD_XL_2,//26 Hz (low power)             |  26 Hz (high performance)
  IMU_ORD_XL_3,//52 Hz (low power)             |  52 Hz (high performance) 
  IMU_ORD_XL_4,//104 Hz (normal mode)          |  104 Hz (high performance)
  IMU_ORD_XL_5,//208 Hz (normal mode)          |  208 Hz (high performance) 
  IMU_ORD_XL_6,//416 Hz (high performance)     |  416 Hz (high performance)
  IMU_ORD_XL_7,//833 Hz (high performance)     |  833 Hz (high performance)
  IMU_ORD_XL_8,//1.66 kHz (high performance)   |  1.66 kHz (high performance)
  IMU_ORD_XL_9,//3.33 kHz (high performance)   |  3.33 kHz (high performance)
  IMU_ORD_XL_MAX,//6.66 kHz (high performance) |  6.66 kHz (high performance)
  IMU_ORD_XL_MIN,//1.6 Hz (low power only)     |  12.5 Hz (high performance)
}IMU_ORD_XL_t;//加速度计的数据输出速率

typedef enum {
  IMU_ORD_G_POWER_DOWN = 0x00,
  //             当 XL_HM_MODE = true         | 当 XL_HM_MODE = false
  IMU_ORD_G_MIN,//12.5 Hz (low power)         | 12.5 Hz (high performance)
  IMU_ORD_G_1,  //26 Hz (low power)           | 26 Hz (high performance)
  IMU_ORD_G_2,  //52 Hz (low power)           | 52 Hz (high performance) 
  IMU_ORD_G_3,  //104 Hz (normal mode)        | 104 Hz (high performance)
  IMU_ORD_G_4,  //208 Hz (normal mode)        | 208 Hz (high performance) 
  IMU_ORD_G_5,  //416 Hz (high performance)   | 416 Hz (high performance)
  IMU_ORD_G_6,  //833 Hz (high performance)   | 833 Hz (high performance)
  IMU_ORD_G_7,  //1.66 kHz (high performance) | 1.66 kHz (high performance)
  IMU_ORD_G_8,  //3.33 kHz (high performance  | 3.33 kHz (high performance)
  IMU_ORD_G_MAX,//6.66 kHz (high performance  | 6.66 kHz (high performance)
}IMU_ORD_G_t;//陀螺仪的数据输出速率

typedef enum {
  //  不满足任何一个条件以下对应枚举设置不生效
  //  当HP_SLOPE_XL_EN = false  |  true
  //  并且  LPF2_XL_EN = true   |  任意 
  //并且INPUT_COMPOSITE= 任意    | false
  IMU_HPCF_XL1 = 0x00,//ODR/50  | ODR/4
  IMU_HPCF_XL2,       //ODR/100 | ODR/100
  IMU_HPCF_XL3,       //ODR/9   | ODR/9
  IMU_HPCF_XL4,       //ODR/400 | ODR/400
}IMU_HPCF_XL_t;//加速度计滤波器截止设置(部分生效的)

typedef enum {
  IMU_HPM_G1 = 0x00,//16 mHz
  IMU_HPM_G2,       //65 mHz
  IMU_HPM_G3,       //260 mHz
  IMU_HPM_G4,       //1.04 Hz
}IMU_HPM_G_t;//陀螺仪高通滤波器截止频率


typedef enum {
  /////////////ODR = 800 Hz | ODR = 1.6 kHz | ODR = 3.3 kHz | ODR = 6.6 kHz
  IMU_FTYPE1,//    245 Hz   |     315 Hz    |      343Hz    |     351 Hz
  IMU_FTYPE2,//    195 Hz   |     224 Hz    |      234 Hz   |     237 Hz
  IMU_FTYPE3,//    155 Hz   |     168 Hz    |      172 Hz   |     173 Hz
  IMU_FTYPE4,//    293 Hz   |     505 Hz    |      925 Hz   |     937 Hz
}IMU_FTYPE_t;//陀螺仪低通滤波器（LPF1）带宽选择

typedef enum {
IMU_FF_THS_156MG,
IMU_FF_THS_219MG,
IMU_FF_THS_250MG,
IMU_FF_THS_312MG,
IMU_FF_THS_344MG,
IMU_FF_THS_406MG,
IMU_FF_THS_469MG,
IMU_FF_THS_500MG,
}IMU_FF_THS_t;//自由落体检测触发阈值,单位mg 即 1 x 10^-3 g


typedef enum {
 IMU_INACT_EN_DISABLE,//失能
 IMU_INACT_EN_MODE1,//如果静默事件触发,将加速度计ODR设置为12.5Hz（低功率模式）陀螺仪 不改变
 IMU_INACT_EN_MODE2,//如果静默事件触发,将加速度计ODR设置为12.5Hz（低功率模式）陀螺仪设置为 睡眠模式
 IMU_INACT_EN_MODE3,//如果静默事件触发,将加速度计ODR设置为12.5Hz（低功率模式）陀螺仪设置为 断电模式
}IMU_INACT_EN_t;//静默功能设置

typedef enum {
 IMU_DEC_FIFO_GYRO_NONE,//不抽取任何数据
 IMU_DEC_FIFO_GYRO_ALL,//完整数据(系数1抽取)
 IMU_DEC_FIFO_GYRO_2,//以系数2抽取
 IMU_DEC_FIFO_GYRO_3,//以系数3抽取
 IMU_DEC_FIFO_GYRO_4,//以系数4抽取
 IMU_DEC_FIFO_GYRO_8,//以系数8抽取
 IMU_DEC_FIFO_GYRO_16,//以系数16抽取
 IMU_DEC_FIFO_GYRO_32,//以系数32抽取
}IMU_DEC_FIFO_GYRO_t;//陀螺仪FIFO数据集抽取设置

typedef enum {
 IMU_DEC_FIFO_XL_NONE,//不抽取任何数据
 IMU_DEC_FIFO_XL_ALL,//完整数据(系数1抽取)
 IMU_DEC_FIFO_XL_2,//以系数2抽取
 IMU_DEC_FIFO_XL_3,//以系数3抽取
 IMU_DEC_FIFO_XL_4,//以系数4抽取
 IMU_DEC_FIFO_XL_8,//以系数8抽取
 IMU_DEC_FIFO_XL_16,//以系数16抽取
 IMU_DEC_FIFO_XL_32,//以系数32抽取
}IMU_DEC_FIFO_XL_t;//加速度计FIFO数据集抽取设置

typedef enum {
 IMU_ODR_FIFO_DISABLE,//FIFO关闭
 IMU_ODR_FIFO_MIN,//12.5 Hz
 IMU_ODR_FIFO_1,//26 Hz
 IMU_ODR_FIFO_2,//52 Hz
 IMU_ODR_FIFO_3,//104 Hz
 IMU_ODR_FIFO_4,//208 Hz
 IMU_ODR_FIFO_5,//416 Hz
 IMU_ODR_FIFO_6,//833 Hz
 IMU_ODR_FIFO_7,//1.66 kHz
 IMU_ODR_FIFO_8,//3.33 kHz
 IMU_ODR_FIFO_MAX,//6.66 kHz
}IMU_ODR_FIFO_t;//FIFO ORD选择

typedef enum{
IMU_FIFO_MODE_BYPASS            = 0,//旁路模式,FIFO关闭
IMU_FIFO_MODE_FIFO              = 1,//FIFO模式,数据全部填充状态不再抽取
IMU_FIFO_MODE_CONTINUOUS        = 6, //连续模式,如果FIFO已满，新数据将覆盖旧的数据
IMU_FIFO_MODE_CONTINUOUS_FIFO   = 3,//先是连续模式，触发器被取消断言后进行FIFO模式
IMU_FIFO_MODE_BYPASS_CONTINUOUS = 4,//先是旁路模式，触发器被取消断言后进行连续模式
}IMU_FIFO_MODE_t;//FIFO运行模式

typedef enum{
IMU_SIXD_THS_80_DEGREES,//80度
IMU_SIXD_THS_70_DEGREES,//70度
IMU_SIXD_THS_60_DEGREES,//60度
IMU_SIXD_THS_50_DEGREES,//50度
}IMU_SIXD_THS_t;// 4D/6D检测功能的阈值
/*******************************公共API***************************************/
void lsm6ds3trc_database_map_set(IMU_reg_mapping_t* reg_database, int map_num);
void lsm6ds3trc_database_map_read(IMU_reg_mapping_t* reg_database, int map_num);

void lsm6ds3trc_init_or_reset();

bool lsm6ds3trc_get_free_fall_status();

uint16_t lsm6ds3trc_get_step_counter();

int lsm6ds3trc_get_now_temperature();

IMU_D6D_data_value_t lsm6ds3trc_get_D6D_data_value();

IMU_acceleration_value_t lsm6ds3trc_gat_now_acceleration();

IMU_angular_rate_value_t lsm6ds3trc_gat_now_angular_rate();

/***************************寄存器配置值合成API************************************/
//将分散的配置数据合并成对应寄存器的值,用于数据库条目个性化构建
//使用例子:MAP_BASE(REG_ADD_CTRL1_XL,value_transform_CTRL1_XL(这里填写各种配置数据)),

uint8_t value_compound_CTRL1_XL(IMU_ORD_XL_t ODR_XL, IMU_FS_XL_t FS_XL, bool LPF1_BW_SEL, bool BW0_XL);
uint8_t value_compound_CTRL2_G(IMU_ORD_G_t ODR_G, IMU_FS_G_t FS_G);
uint8_t value_compound_CTRL3_C(bool BOOT,bool BDU,bool H_LACTIVE,bool PP_OD,bool SIM,bool IF_INC,bool BLE,bool SW_RESET);
uint8_t value_compound_CTRL4_C(bool DEN_XL_EN, bool SLEEP, bool INT2_on_INT1, bool DEN_DRDY_INT1, bool DRDY_MASK, bool I2C_disable, bool LPF1_SEL_G);
uint8_t value_compound_CTRL6_C(bool TRIG_EN, bool LVL_EN, bool LVL2_EN, bool XL_HM_MODE, bool USR_OFF_W, IMU_FTYPE_t FTYPE);
uint8_t value_compound_CTRL7_G(bool G_HM_MODE, bool HP_EN_G, IMU_HPM_G_t HPM_G, bool ROUNDING_STATUS);
uint8_t value_compound_CTRL8_XL(bool LPF2_XL_EN, IMU_HPCF_XL_t HPCF_XL, bool HP_REF_MODE, bool INPUT_COMPOSITE, bool HP_SLOPE_XL_EN, bool LOW_PASS_ON_6D);
uint8_t value_compound_CTRL10_C(bool WRIST_TILT_EN,bool TIMER_EN,bool PEDO_EN,bool TILT_EN,bool FUNC_EN,bool PEDO_RST_STEP,bool SIGN_MOTION_EN);
uint8_t value_compound_INT1_CTRL(bool INT1_STEP_DETECTOR,bool INT1_SIGN_MOT,bool INT1_FULL_FLAG,bool INT1_FIFO_OVR,bool INT1_FTH,bool INT1_BOOT,bool INT1_DRDY_G,bool INT1_DRDY_XL);
uint8_t value_compound_INT2_CTRL(bool INT2_STEP_DELTA,bool INT2_STEP_COUNT_OV,bool INT2_FULL_FLAG,bool INT2_FIFO_OVR,bool INT2_FTH,bool INT2_DRDY_TEMP,bool INT2_DRDY_G,bool INT2_DRDY_XL);
uint8_t value_compound_MD1_CFG(bool INT1_INACT_STATE,bool INT1_SINGLE_TAP,bool INT1_WU,bool INT1_FF,bool INT1_DOUBLE_TAP,bool INT1_6D,bool INT1_TILT,bool INT1_TIMER);
uint8_t value_compound_TAP_CFG(bool INTERRUPTS_ENABLE,IMU_INACT_EN_t INACT_EN,bool SLOPE_FDS,bool TAP_X_EN,bool TAP_Y_EN,bool TAP_Z_EN,bool LIR);
uint8_t value_compound_FIFO_CTRL3(IMU_DEC_FIFO_GYRO_t DEC_FIFO_GYRO,IMU_DEC_FIFO_XL_t DEC_FIFO_XL);
uint8_t value_compound_FIFO_CTRL5(IMU_ODR_FIFO_t ODR_FIFO,IMU_FIFO_MODE_t FIFO_MODE);
uint8_t value_compound_partly_TAP_THS_6D(bool D4D_EN,IMU_SIXD_THS_t SIXD_THS);
uint8_t value_compound_partly_FREE_FALL(IMU_FF_THS_t FF_THS);

/******************************数据库构建****************************************/
//关于写入的顺序:
//不使用下面的USE_MAP_ID(),即只有MAP_BASE()
//那么写入顺序是在数据库由上到下的顺序,靠近开头的先写
//否则写入顺序是ID数字大小顺序,ID取值为 0 到 条目数量-1 ,ID小的先写入

//映射数据库存储单元,映射数据库是IMU_reg_mapping_t数组
typedef struct
{
  uint8_t reg_address;
  uint8_t reg_value;
} IMU_reg_mapping_t;

//构建单元,任何映射数据库都由若干个MAP_BASE构建单元组成,address可枚举,是需要写入寄存器的名字,实际指代的是需要写入寄存器的地址
#define MAP_BASE(address, value) \
  {                              \
    address, value               \
  }

//为 构建单元 应用ID标识 进行自定义顺序化读写的映射
//使用例子:USE_MAP_ID(0)MAP_BASE(REG_ADD_CTRL3_C, 0x01),//这个条目将第一个写入,因为它是数组中角标为0的元素
#define USE_MAP_ID(id) [id]= 

//寄存器值配置数据库是包含需要读写的寄存器地址以及一个uint8_t的值为单元的键值对数据集
//如果用户未通过USE_MAP_ID索引单元的优先级,那么这个单元下的目标寄存器何时读取或写入value由它在数组的前后位置决定
//同时它不需要各种配置函数的连续性调用,不需要反复的参数传递,可能提高系统以及总线的相同时间内的利用率
//不同于常规,读取目标先进行了整合,这使得长数据读取,连续读取寄存器时可以方便地引入RTOS支持
//但通过这种方式,外部组件对该部分的配置使用将比较复杂,或许需要编写库内的进一步封装的个性化配置函数

/****************************默认寄存器值配置数据库****************************************/
//用于初始化IMU_reg_mapping_t数组即数据库的值，在初始化函数中被使用
#define IMU_INIT_DEFAULT_MAPPING_DATABASE_MAP_NUM 20 //默认寄存器值配置数据库的最大条目数量
#define IMU_INIT_DEFAULT_MAPPING_DATABASE    { \
MAP_BASE(CTRL3_C,value_compound_CTRL3_C(false,true,false,false,false,true,false,true)),\ 
MAP_BASE(CTRL3_C,value_compound_CTRL3_C(false,true,false,false,false,true,false,false)), \
MAP_BASE(CTRL1_XL, value_compound_CTRL1_XL(IMU_ORD_XL_6, IMU_FS_XL_16G, false, false)), \
MAP_BASE(TAP_CFG, value_compound_TAP_CFG(true,IMU_INACT_EN_MODE1,false,false,false,false,true)), \
MAP_BASE(WAKE_UP_DUR, 0x00),MAP_BASE(FREE_FALL,0x30|value_compound_partly_FREE_FALL(IMU_FF_THS_312MG)), \
MAP_BASE(MD1_CFG,value_compound_MD1_CFG(false,false,false,true,false,false,false,false)), \
MAP_BASE(CTRL2_G, value_compound_CTRL2_G(IMU_ORD_G_5, IMU_FS_G_125DPS)), \
MAP_BASE(CTRL4_C, value_compound_CTRL4_C(true,false,false,false,true,false,true)), \
MAP_BASE(CTRL7_G,value_compound_CTRL7_G(false,true,IMU_HPM_G2,true)),\
MAP_BASE(CTRL8_XL,value_compound_CTRL8_XL(true, IMU_HPCF_XL4, false, true, false, true)), \
MAP_BASE(CTRL10_C,value_compound_CTRL10_C(false,false,true,true,true,true,true)), \
MAP_BASE(CTRL10_C,value_compound_CTRL10_C(false,false,true,true,true,false,true)), \
MAP_BASE(FIFO_CTRL3, value_compound_FIFO_CTRL3(IMU_DEC_FIFO_GYRO_32,IMU_DEC_FIFO_XL_32)), \
MAP_BASE(FIFO_CTRL5, value_compound_FIFO_CTRL5(IMU_ODR_FIFO_5,IMU_FIFO_MODE_CONTINUOUS)), \
MAP_BASE(TAP_THS_6D, value_compound_partly_TAP_THS_6D(true,IMU_SIXD_THS_80_DEGREES)), \
MAP_BASE(CONFIG_PEDO_THS_MIN, 0x90), \    
}

//FIFO是姿态传感器中一种常用的缓存策略,FIFO具有抽取功能,可以采集大量的传感数据的一小部分用于参考
//同时也压缩了数据,因为数据实时性需求大,一帧FIFO数据往往包含多个传感器的数据
//配置抽取的方式不同,FIFO中各种数据的顺序会发生一定变化,通过配置映射数据库来告知读取函数这些数据的顺序含义
//FIFO映射数据库使用寄存器值配置数据库的完全相同逻辑,但是不同单元的读写顺序显然相比更加重要
//键值对的 address 表示姿态传感器寄存器读取目标,显然它们是FIFO数据寄存器的地址
//键值对的 value 表示该寄存器值读取之后,存储到uint8_t的数组的首地址,不同数据显然需要保存在不同uint8_t的数组,取决于外部函数的数据处理方式

/****************************默认FIFO映射数据库(加速度计与陀螺仪ORD一致,抽取系数一致)****************************************/
#define IMU_DEFAULT_FIFO_MAPPING_DATABASE_MAP_NUM 4 //默认FIFO映射数据库的最大条目数量
#define IMU_DEFAULT_FIFO_MAPPING_DATABASE    { \
MAP_BASE(FIFO_DATA_OUT_L,),\ 
MAP_BASE(FIFO_DATA_OUT_H,),\
MAP_BASE(FIFO_DATA_OUT_L,),\ 
MAP_BASE(FIFO_DATA_OUT_H,),\
}
