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

// 我们这里使用了普通FIFO模式，因为我们只是用于操作UI而无需完整的姿态数据以及严格的实时性，低误差性
// 初始化任务是 设置加速度计和陀螺仪的ORD（为了获得中断输出不得设置加速度计到掉电模式)，以及XL_HM_MODE位的设置
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


// 寄存器地址,用于数据库条目的构建,如下面的默认配置数据库
enum
{
  // 综合的
  REG_ADD_CTRL1_XL = 0x10,//加速度计-> 数据输出速率 量程 滤波算法相关
  REG_ADD_CTRL2_G = 0x11, //陀螺仪-> 数据输出速率 量程
  REG_ADD_CTRL3_C = 0x12,   // BDU->1  IF_INC->1
  REG_ADD_CTRL4_C = 0x13,   
  REG_ADD_CTRL6_C = 0x15,   // XL_HM_MODE
  REG_ADD_CTRL8_XL = 0x17,  // LOW_PASS_ON_6D
  REG_ADD_CTRL10_C = 0x19,  // FUNC_EN->1 PEDO_EN->1 PEDO_RST_STEP从0跳1再置0清除计步器步数
  REG_ADD_INT1_CTRL = 0x0D, // FIFO_FULL
  REG_ADD_STATUS_REG = 0x1E,
  // FIFO
  REG_ADD_FIFO_CTRL3 = 0x08,   // 抽取系数 DEC_FIFO_G DEC_FIFO_XL
  REG_ADD_FIFO_CTRL4 = 0x09,   // 外部扩展传感器抽取系数
  REG_ADD_FIFO_CTRL5 = 0x0A,   // ODR_FIFO FIFO_MODE->110b(更改ORD需要读取出有用数据再置Bypass mode,修改ORD后改为原来模式)
  REG_ADD_FIFO_STATUS1 = 0x3A, // read DIFF_FIFO查看数据个数
  REG_ADD_FIFO_STATUS2 = 0x3B, // read DIFF_FIFO查看数据个数 全部读完FIFO_EMPTY位被设置为高
  REG_ADD_FIFO_STATUS3 = 0x3C, // read FIFO_PATTERN正在读取的传感器和字节
  REG_ADD_FIFO_STATUS4 = 0x3D, // read FIFO_PATTERN正在读取的传感器和字节
  REG_ADD_FIFO_DATA_OUT_L = 0x3E,
  REG_ADD_FIFO_DATA_OUT_H = 0x3F,

  // 自由落体检测
  REG_ADD_WAKE_UP_SRC = 0x1B, // FF_IA
  REG_ADD_WAKE_UP_DUR = 0x5C, // FF_DUR5
  REG_ADD_FREE_FALL = 0x5D,   // FF_THS FF_DUR

  // 6D检测
  REG_ADD_D6D_SRC = 0x1D,    // all
  REG_ADD_MD1_CFG = 0x5E,    // INT1_6D  ||| INT1_FF
  REG_ADD_TAP_THS_6D = 0x59, // SIXD_THS
  REG_ADD_TAP_CFG = 0x58,    // INTERRUPTS_ENABLE(6D 自由落体检测)  LIR->1(全局有效)

  // 计步器(必须 ORD 26Hz+)
  REG_ADD_STEP_COUNTER_L = 0x4B,      // read
  REG_ADD_STEP_COUNTER_H = 0x4C,      // read
  REG_ADD_CONFIG_PEDO_THS_MIN = 0x0F, // PEDO_FS->1

  // 温度监测
  REG_ADD_OUT_TEMP_L = 0x20, // read
  REG_ADD_OUT_TEMP_H = 0x21, // read

  // 读取源数据
  // 陀螺仪
  REG_ADD_OUTX_L_G = 0x22,
  REG_ADD_OUTX_H_G = 0x23,
  REG_ADD_OUTY_L_G = 0x24,
  REG_ADD_OUTY_H_G = 0x25,
  REG_ADD_OUTZ_L_G = 0x26,
  REG_ADD_OUTZ_H_G = 0x27,

  // 加速度计
  REG_ADD_OUTX_L_XL = 0x28,
  REG_ADD_OUTX_H_XL = 0x29,
  REG_ADD_OUTY_L_XL = 0x2A,
  REG_ADD_OUTY_H_XL = 0x2B,
  REG_ADD_OUTZ_L_XL = 0x2C,
  REG_ADD_OUTZ_H_XL = 0x2D,
};

//映射数据库存储单元,映射数据库是IMU_reg_mapping_t数组
typedef struct
{
  uint8_t reg_address;
  uint8_t reg_value;
} IMU_reg_mapping_t;

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

typedef enum{
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

typedef enum{
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

typedef enum{
//  不满足任何一个条件以下对应枚举设置不生效
//  当HP_SLOPE_XL_EN = false  |  true
//  并且  LPF2_XL_EN = true   |  任意 
//并且INPUT_COMPOSITE= 任意    | false
IMU_HPCF_XL1 = 0x00,//ODR/50  | ODR/4
IMU_HPCF_XL2,       //ODR/100 | ODR/100
IMU_HPCF_XL3,       //ODR/9   | ODR/9
IMU_HPCF_XL4,       //ODR/400 | ODR/400
}IMU_HPCF_XL_t;

typedef enum{
IMU_HPM_G1 = 0x00,//16 mHz
IMU_HPM_G2,       //65 mHz
IMU_HPM_G3,       //260 mHz
IMU_HPM_G4,       //1.04 Hz
}IMU_HPM_G_t;


typedef enum{
           //ODR = 800 Hz | ODR = 1.6 kHz | ODR = 3.3 kHz | ODR = 6.6 kHz
IMU_FTYPE1,//    245 Hz   |     315 Hz    |      343Hz    |     351 Hz
IMU_FTYPE2,//    195 Hz   |     224 Hz    |      234 Hz   |     237 Hz
IMU_FTYPE3,//    155 Hz   |     168 Hz    |      172 Hz   |     173 Hz
IMU_FTYPE4,//    293 Hz   |     505 Hz    |      925 Hz   |     937 Hz
}IMU_FTYPE_t;





//*****************************传感数据相关********************************/
typedef struct{
  int x;
  int y;
  int z;
}IMU_acceleration_value_t;//加速度值 单位mg 即 1 x 10^-3 g

typedef struct{
  int x;
  int y;
  int z;
}IMU_angular_rate_value_t;//角速度值 单位为mdps 即 1 x 10^-3 dps


/*******************************公共API***************************************/
void lsm6ds3trc_database_map_set(IMU_reg_mapping_t *reg_database, int map_num);
void lsm6ds3trc_database_map_read(IMU_reg_mapping_t *reg_database, int map_num);

void lsm6ds3trc_init_or_reset();

uint16_t lsm6ds3trc_get_step_counter();

IMU_acceleration_value_t lsm6ds3trc_gat_now_acceleration();

IMU_angular_rate_value_t lsm6ds3trc_gat_now_angular_rate();

/***************************寄存器配置值合成API************************************/
//将分散的配置数据合并成对应寄存器的值,用于数据库条目个性化构建
//使用例子:MAP_BASE(REG_ADD_CTRL1_XL,value_transform_CTRL1_XL(这里填写各种配置数据)),

uint8_t value_compound_CTRL1_XL(IMU_ORD_XL_t ODR_XL,IMU_FS_XL_t FS_XL,bool LPF1_BW_SEL,bool BW0_XL);
uint8_t value_compound_CTRL2_G(IMU_ORD_G_t ODR_G,IMU_FS_G_t FS_G);
uint8_t value_compound_CTRL4_C(bool DEN_XL_EN,bool SLEEP,bool INT2_on_INT1,bool DEN_DRDY_INT1,bool DRDY_MASK,bool I2C_disable,bool LPF1_SEL_G);
uint8_t value_compound_CTRL6_C(bool TRIG_EN,bool LVL_EN,bool LVL2_EN,bool XL_HM_MODE,bool USR_OFF_W,IMU_FTYPE_t FTYPE);
uint8_t value_compound_CTRL7_G(bool G_HM_MODE,bool HP_EN_G,IMU_HPM_G_t HPM_G,bool ROUNDING_STATUS);
uint8_t value_compound_CTRL8_XL(bool LPF2_XL_EN,IMU_HPCF_XL_t HPCF_XL,bool HP_REF_MODE,bool INPUT_COMPOSITE,bool HP_SLOPE_XL_EN,bool LOW_PASS_ON_6D);

/******************************数据库构建****************************************/
//关于写入的顺序:
//不使用下面的USE_MAP_ID(),即只有MAP_BASE()
//那么写入顺序是在数据库由上到下的顺序,靠近开头的先写
//否则写入顺序是ID数字大小顺序,ID取值为 0 到 条目数量-1 ,ID小的先写入

//构建单元,任何映射数据库都由若干个MAP_BASE构建单元组成
#define MAP_BASE(address, value) \
  {                              \
    address, value               \
  }

//为 构建单元 应用ID标识 进行自定义顺序化读写的映射
//使用例子:USE_MAP_ID(0)MAP_BASE(REG_ADD_CTRL3_C, 0x01),//这个条目将第一个写入
#define USE_MAP_ID(id) [id]= 






/****************************默认寄存器值配置数据库****************************************/
//用于初始化IMU_reg_mapping_t数组即数据库的值，在初始化函数中被使用
#define IMU_INIT_DEFAULT_MAPPING_DATABASE_MAP_NUM 20 //默认寄存器值配置数据库的最大条目数量
#define IMU_INIT_DEFAULT_MAPPING_DATABASE    { \
  MAP_BASE(REG_ADD_CTRL3_C, 0x01),             \    
  MAP_BASE(REG_ADD_CTRL3_C, 0x44),             \    
  MAP_BASE(REG_ADD_CTRL1_XL,value_compound_CTRL1_XL(IMU_ORD_XL_6,IMU_FS_XL_16G,false,false)),\  
  MAP_BASE(REG_ADD_CTRL2_G,value_compound_CTRL2_G(IMU_ORD_G_MAX,IMU_FS_G_125DPS)),             \  
  MAP_BASE(REG_ADD_CTRL4_C, 0x08),             \    
  MAP_BASE(REG_ADD_CTRL8_XL,value_compound_CTRL8_XL(true,IMU_HPCF_XL4,false,true,false,true)),\
  MAP_BASE(REG_ADD_CTRL10_C, 0x1F),            \
  MAP_BASE(REG_ADD_CTRL10_C, 0x1D),            \
  MAP_BASE(REG_ADD_INT1_CTRL, 0x20),           \
  MAP_BASE(REG_ADD_FIFO_CTRL3, 0x09),          \
  MAP_BASE(REG_ADD_FIFO_CTRL5, 0x26),          \
  MAP_BASE(REG_ADD_WAKE_UP_SRC, 0x20),         \
  MAP_BASE(REG_ADD_FREE_FALL, 0x33),           \
  MAP_BASE(REG_ADD_D6D_SRC, 0x40),             \
  MAP_BASE(REG_ADD_MD1_CFG, 0x14),             \
  MAP_BASE(REG_ADD_TAP_THS_6D, 0x80),          \
  MAP_BASE(REG_ADD_TAP_CFG, 0x81),             \
  MAP_BASE(REG_ADD_CONFIG_PEDO_THS_MIN, 0x90), \    
}



