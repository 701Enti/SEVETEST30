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
#define IMU_LA_SO_FS_2G  0.061
#define IMU_LA_SO_FS_4G  0.122
#define IMU_LA_SO_FS_8G  0.244
#define IMU_LA_SO_FS_16G 0.488
//陀螺仪(角速率测量)  mdps/LSB (1 x 10^-3 dps/LSB)
#define IMU_G_SO_FS125DPS  4.375
#define IMU_G_SO_FS250DPS  8.75
#define IMU_G_SO_FS500DPS  17.50
#define IMU_G_SO_FS1000DPS 35
#define IMU_G_SO_FS2000DPS 70


// 寄存器地址,用于数据库条目的构建,如下面的默认配置数据库
enum
{
  // 综合的
  REG_ADD_CTRL1_XL = 0x10, // ODR_XL 26Hz+ ||| 设置大于或等于正负4g
  REG_ADD_CTRL2_G = 0x11,
  REG_ADD_CTRL3_C = 0x12,   // BDU->1  IF_INC->1
  REG_ADD_CTRL4_C = 0x13,   // DRDY_MASK
  REG_ADD_CTRL6_C = 0x15,   // XL_HM_MODE
  REG_ADD_CTRL8_XL = 0x17,  // LOW_PASS_ON_6D
  REG_ADD_CTRL10_C = 0x19,  // FUNC_EN->1 PEDO_EN->1 PEDO_RST_STEP从0跳1再置0清除计步器步数
  REG_ADD_INT1_CTRL = 0x0D, // FIFO_FULL

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

typedef struct
{
  uint8_t reg_address;
  uint8_t reg_value;
} IMU_reg_mapping_t;

// 如果调试时需要根据情况改变条目,我们非常建议使用枚举进行ID_MAP_BASE中id数据的设置,指定条目的写入顺序
// 而相对固定的计划好的数据库可以不使用ID_MAP_BASE,使用MAP_BASE,这将不会造成较大的影响

// 映射表条目标准构建单元,条目写入顺序是在数据库由上到下的顺序,靠近开头的先写
#define MAP_BASE(address, value) \
  {                              \
    address, value               \
  }
// 映射表条目ID标识方式的构建单元,条目写入顺序是在ID数字大小数据,ID取值为 0 到 条目数量-1 ,ID小的先写入
#define ID_MAP_BASE(id, address, value) [id] = {address, value}

// 默认寄存器值配置，用于初始化IMU_reg_mapping_t数组的值，在初始化传感器时使用

#define IMU_INIT_DEFAULT_MAPPING_DATABASE_MAP_NUM 20 // 以下数据库的条目数量
#define IMU_INIT_DEFAULT_MAPPING_DATABASE    { \
  MAP_BASE(REG_ADD_CTRL3_C, 0x01),             \    
  MAP_BASE(REG_ADD_CTRL3_C, 0x44),             \    
  MAP_BASE(REG_ADD_CTRL1_XL, 0x48),            \  
  MAP_BASE(REG_ADD_CTRL2_G, 0x48),             \  
  MAP_BASE(REG_ADD_CTRL4_C, 0x08),             \    
  MAP_BASE(REG_ADD_CTRL6_C, 0x00),             \
  MAP_BASE(REG_ADD_CTRL8_XL, 0x01),            \
  MAP_BASE(REG_ADD_CTRL10_C, 0x1F),            \
  MAP_BASE(REG_ADD_CTRL10_C, 0x1D),            \
  MAP_BASE(REG_ADD_INT1_CTRL, 0x20),           \
  MAP_BASE(REG_ADD_FIFO_CTRL3, 0x09),          \
  MAP_BASE(REG_ADD_FIFO_CTRL5, 0x26),          \
  MAP_BASE(REG_ADD_WAKE_UP_SRC, 0x20),         \
  MAP_BASE(REG_ADD_WAKE_UP_DUR, 0x00),         \
  MAP_BASE(REG_ADD_FREE_FALL, 0x33),           \
  MAP_BASE(REG_ADD_D6D_SRC, 0x40),             \
  MAP_BASE(REG_ADD_MD1_CFG, 0x14),             \
  MAP_BASE(REG_ADD_TAP_THS_6D, 0x80),          \
  MAP_BASE(REG_ADD_TAP_CFG, 0x81),             \
  MAP_BASE(REG_ADD_CONFIG_PEDO_THS_MIN, 0x90), \    
}

// 用户配置自定义类型
typedef enum
{
  IMU_LA_FS_2G,       // ±2g
  IMU_LA_FS_16G,      // ±16g  
  IMU_LA_FS_4G,       // ±4g
  IMU_LA_FS_8G,       // ±8g
} IMU_LA_FS_select_t; // 加速度计(加速度测量)量程选择 g:重力加速度,约为g=9.780米/秒^2 CTRL1_XL低第三位 低第四位 移两位或

typedef enum
{
  IMU_G_FS_125DPS = 0x02,   // ±125dps
  IMU_G_FS_250DPS = 0x00,   // ±250dps
  IMU_G_FS_500DPS = 0x04,   // ±500dps
  IMU_G_FS_1000DPS = 0x08,  // ±1000dps
  IMU_G_FS_2000DPS = 0x0C,  // ±2000dps
} IMU_G_FS_select_t; // 陀螺仪(角速率测量)量程选择 dps:角速率 表示 度/秒 CTRL2_G 低四位 直接或

// 数据自定义类型
typedef struct{
  int8_t x;
  int8_t y;
  int8_t z;
}IMU_acceleration_value_t//加速度值 单位为g (重力加速度,约为g=9.780米/秒^2)

typedef struct{
  int16_t x;
  int16_t y;
  int16_t z;
}IMU_angular_rate_value_t//角速度值 单位为dps (度/秒)

void lsm6ds3trc_database_map_set(IMU_reg_mapping_t *reg_database, int map_num);

void lsm6ds3trc_database_map_read(IMU_reg_mapping_t *reg_database, int map_num);

void lsm6ds3trc_init_or_reset();

uint16_t lsm6ds3trc_data_get_step_counter();
