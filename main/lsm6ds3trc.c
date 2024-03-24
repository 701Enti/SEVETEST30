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
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#include "lsm6ds3trc.h"
#include "esp_log.h"
#include "board_def.h"
#include "driver/i2c.h"
#include <string.h>
#include <math.h>

// I2C相关配置宏定义在board_def.h下
#define LSM6DS3TRC_DEVICE_ADD 0x6A

/// @brief 映射数据库中的配置到LSM6DS3TRC硬件寄存器,即通过数据库写入设定的寄存器
/// @param reg_database 导入数据库，这是一个以IMU_reg_mapping_t的数组，其中的条目即单个IMU_reg_mapping_t数据的个数没有规定，
///                     并且条目的角标与寄存器地址和数据不需要按任何顺序规律进行对应，每个条目只要包含需要写入的寄存器地址和数据，可以参考lsm6ds3trc.h默认配置宏的数据库构建方式
/// @param map_data_num 需要映射的数据条目数量即要写入的寄存器的个数，从第0个条目按数字大小顺序写入map_num个条目为止,map_num需要小于等于条目总数量
void lsm6ds3trc_database_map_set(IMU_reg_mapping_t* reg_database, int map_num)
{
  const char* TAG = "lsm6ds3trc_database_map_set";
  if (!reg_database)
  {
    ESP_LOGE(TAG, "导入了为空的数据库");
    return;
  }
  // 用于偏移到指定数据库内存区域获取数据
  IMU_reg_mapping_t* mapping_buf = reg_database;

  for (int idx = 0; idx < map_num; idx++)
  {
    mapping_buf = &reg_database[idx];
    uint8_t write_buf[2] = { mapping_buf->reg_address, mapping_buf->reg_value };
    esp_err_t err = i2c_master_write_to_device(DEVICE_I2C_PORT, LSM6DS3TRC_DEVICE_ADD, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "与姿态传感器LSM6DS3TRC通讯时发现问题 描述： %s", esp_err_to_name(err));
      return;
    }
  }
  ESP_LOGI(TAG, "成功映射配置到姿态传感器LSM6DS3TRC");
}

/// @brief 映射LSM6DS3TRC硬件寄存器的数据到数据库,即读取设定的寄存器到数据库
/// @param reg_database 导入数据库，这是一个以IMU_reg_mapping_t的数组，其中的条目即单个IMU_reg_mapping_t数据的个数没有规定，
///                     并且条目的角标与寄存器地址和数据不需要按任何顺序规律进行对应，每个条目只要包含需要写入的寄存器地址和数据，可以参考lsm6ds3trc.h默认配置宏的数据库构建方式
//                      完成读取后,数据库中寄存器的值将更新,因此在初始化数据库设定寄存器时的值任意如0x00
/// @param map_data_num 需要映射的数据条目数量即要写入的寄存器的个数，从第0个条目按数字大小顺序写入map_num个条目为止,map_num需要小于等于条目总数量
void lsm6ds3trc_database_map_read(IMU_reg_mapping_t* reg_database, int map_num)
{
  const char* TAG = "lsm6ds3trc_database_map_read";
  if (!reg_database)
  {
    ESP_LOGE(TAG, "导入了为空的数据库");
    return;
  }
  // 用于偏移到指定数据库内存区域写入数据库
  IMU_reg_mapping_t* mapping_buf = reg_database;

  for (int idx = 0; idx < map_num; idx++)
  {
    mapping_buf = &reg_database[idx];

    esp_err_t err = i2c_master_write_read_device(DEVICE_I2C_PORT, LSM6DS3TRC_DEVICE_ADD, &(mapping_buf->reg_address), sizeof(uint8_t), &(mapping_buf->reg_value), sizeof(uint8_t), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "与姿态传感器LSM6DS3TRC通讯时发现问题 描述： %s", esp_err_to_name(err));
      return;
    }
  }
}

/// @brief 以默认配置初始化LSM6DS3TRC姿态传感器，或者重置其到默认配置
void lsm6ds3trc_init_or_reset()
{
  IMU_reg_mapping_t reg_database[IMU_INIT_DEFAULT_MAPPING_DATABASE_MAP_NUM] = IMU_INIT_DEFAULT_MAPPING_DATABASE; // 寄存器映射数据库
  lsm6ds3trc_database_map_set(reg_database, IMU_INIT_DEFAULT_MAPPING_DATABASE_MAP_NUM);
}

/// @brief 获取步数计数值
/// @return 步数计数值
uint16_t lsm6ds3trc_get_step_counter()
{
  IMU_reg_mapping_t reg_database[2] = {
      MAP_BASE(REG_ADD_STEP_COUNTER_L, 0x00),
      MAP_BASE(REG_ADD_STEP_COUNTER_H, 0x00),
  };
  lsm6ds3trc_database_map_read(reg_database, 2);

  uint16_t ret = reg_database[0].reg_value | (reg_database[1].reg_value << 8);
  return ret;
}

/// @brief 获取实时的加速度偏移
/// @return 实时的加速度值偏移
IMU_acceleration_value_t lsm6ds3trc_gat_now_acceleration()
{
  const char* TAG = "lsm6ds3trc_gat_now_acceleration";

  static IMU_acceleration_value_t value_buf = { 0,0,0 };//存储上次的数据以计算偏移量
  IMU_acceleration_value_t acceleration_value = { 0,0,0 };//存储本次数据
  IMU_acceleration_value_t ret = { 0,0,0 };//存储返回值

  //构建映射数据库设置读取的目标
  IMU_reg_mapping_t reg_database[8] = {
      MAP_BASE(REG_ADD_CTRL1_XL, 0x00),

      MAP_BASE(REG_ADD_OUTX_L_XL, 0x00),
      MAP_BASE(REG_ADD_OUTX_H_XL, 0x00),

      MAP_BASE(REG_ADD_OUTY_L_XL, 0x00),
      MAP_BASE(REG_ADD_OUTY_H_XL, 0x00),

      MAP_BASE(REG_ADD_OUTZ_L_XL, 0x00),
      MAP_BASE(REG_ADD_OUTZ_H_XL, 0x00),

      MAP_BASE(REG_ADD_STATUS_REG, 0x00),
  };

  //等待加速度数据成功获取
  while (!reg_database[7].reg_value & 0x01) {
    lsm6ds3trc_database_map_read(reg_database, 8);//执行映射读取操作
  }

  float LA_SO_buf = 0;//通过设置的量程获取对应缩放因数
  uint32_t max_buf = 0;//量程内允许最大绝对值
  switch (reg_database[0].reg_value >> 2 & 0x03)
  {
  case IMU_FS_XL_2G:
    LA_SO_buf = IMU_LA_SO_FS_2G;
    max_buf = 2*1000;
    break;
  case IMU_FS_XL_4G:
    LA_SO_buf = IMU_LA_SO_FS_4G;
    max_buf = 4*1000;
    break;
  case IMU_FS_XL_8G:
    LA_SO_buf = IMU_LA_SO_FS_8G;
    max_buf = 8*1000;
    break;
  case IMU_FS_XL_16G:
    LA_SO_buf = IMU_LA_SO_FS_16G;
    max_buf = 16*1000;
    break;

  default:
    ESP_LOGE(TAG, "未知的加速度量程配置");
    return ret;
    break;
  }

  //计算实际加速度
  acceleration_value.x = (reg_database[1].reg_value | reg_database[2].reg_value << 8) * LA_SO_buf;
  acceleration_value.y = (reg_database[3].reg_value | reg_database[4].reg_value << 8) * LA_SO_buf;
  acceleration_value.z = (reg_database[5].reg_value | reg_database[6].reg_value << 8) * LA_SO_buf;

  //如果在量程范围内,处理相关数据
  if (abs(acceleration_value.x) <= max_buf) {
    ret.x = acceleration_value.x - value_buf.x;//计算偏移量作为返回值
    value_buf.x = acceleration_value.x;//存储本次的数据以准备为下次读取计算偏移量
  }

  if (abs(acceleration_value.y) <= max_buf) {
    ret.y = acceleration_value.y - value_buf.y;
    value_buf.y = acceleration_value.y;
  }

  if (abs(acceleration_value.z) <= max_buf) {
    ret.z = acceleration_value.z - value_buf.z;
    value_buf.z = acceleration_value.z;
  }

  return ret;
}

/// @brief 获取实时的角速率偏移
/// @return 实时的角速率偏移值
IMU_angular_rate_value_t lsm6ds3trc_gat_now_angular_rate()
{
  const char* TAG = "lsm6ds3trc_gat_now_angular_rate";

  static IMU_angular_rate_value_t value_buf = { 0,0,0 };//存储上次的数据以计算偏移量
  IMU_angular_rate_value_t angular_rate_value = { 0,0,0 };//存储本次数据
  IMU_angular_rate_value_t ret = { 0,0,0 };//存储返回值

  //构建映射数据库设置读取的目标
  IMU_reg_mapping_t reg_database[8] = {
      MAP_BASE(REG_ADD_CTRL2_G, 0x00),

      MAP_BASE(REG_ADD_OUTX_L_G, 0x00),
      MAP_BASE(REG_ADD_OUTX_H_G, 0x00),

      MAP_BASE(REG_ADD_OUTY_L_G, 0x00),
      MAP_BASE(REG_ADD_OUTY_H_G, 0x00),

      MAP_BASE(REG_ADD_OUTZ_L_G, 0x00),
      MAP_BASE(REG_ADD_OUTZ_H_G, 0x00),

      MAP_BASE(REG_ADD_STATUS_REG, 0x00),
  };

  //等待数据成功获取
  while (!reg_database[7].reg_value & 0x02) {
    lsm6ds3trc_database_map_read(reg_database, 8);//执行映射读取操作
  }

  //通过设置的量程获取对应缩放因数
  float G_SO_buf = 0;
  uint32_t max_buf = 0;//量程内允许最大绝对值
  switch (reg_database[0].reg_value >> 1 & 0x07)
  {
  case IMU_FS_G_125DPS:
    G_SO_buf = IMU_G_SO_FS_125DPS;
    max_buf = 125*1000;
    break;
  case IMU_FS_G_250DPS:
    G_SO_buf = IMU_G_SO_FS_250DPS;
    max_buf = 250*1000;
    break;
  case IMU_FS_G_500DPS:
    G_SO_buf = IMU_G_SO_FS_500DPS;
    max_buf = 500*1000;
    break;
  case IMU_FS_G_1000DPS:
    G_SO_buf = IMU_G_SO_FS_1000DPS;
    max_buf = 1000*1000;
    break;
  case IMU_FS_G_2000DPS:
    G_SO_buf = IMU_G_SO_FS_2000DPS;
    max_buf = 2000*1000;
    break;

  default:
    ESP_LOGE(TAG, "未知的陀螺仪量程配置");
    return ret;
    break;
  }

  //计算测量得到的角速率
  angular_rate_value.x = (reg_database[1].reg_value | reg_database[2].reg_value << 8) * G_SO_buf;
  angular_rate_value.y = (reg_database[3].reg_value | reg_database[4].reg_value << 8) * G_SO_buf;
  angular_rate_value.z = (reg_database[5].reg_value | reg_database[6].reg_value << 8) * G_SO_buf;

  //如果在量程范围内,处理相关数据
  if (abs(angular_rate_value.x) <= max_buf){
  ret.x = angular_rate_value.x - value_buf.x;//计算偏移量作为返回值
  value_buf.x = angular_rate_value.x;//存储本次的数据以准备为下次读取计算偏移量    
  }

  if (abs(angular_rate_value.y) <= max_buf){
  ret.y = angular_rate_value.y - value_buf.y;
  value_buf.y = angular_rate_value.y;
  }

  if (abs(angular_rate_value.z) <= max_buf){
  ret.z = angular_rate_value.z - value_buf.z;
  value_buf.z = angular_rate_value.z;
  }

  return ret;
}




/// @brief 合成寄存器配置值 CTRL1_XL : ODR_XL [3:0] | FS_XL [1:0] | LPF1_BW_SEL | BW0_XL
/// @param ODR_XL 加速度计数据输出速率(枚举)
/// @param FS_XL  加速度计量程(枚举)
/// @param LPF1_BW_SEL 应用加速度计数字滤波器1带宽选择 false/true
/// @param BW0_XL 加速度计模拟链带宽选择（仅适用于数据输出速率≥1.67 kHz) false=1.5kHz / true=400Hz
/// @return 实际寄存器值
uint8_t value_compound_CTRL1_XL(IMU_ORD_XL_t ODR_XL, IMU_FS_XL_t FS_XL, bool LPF1_BW_SEL, bool BW0_XL) {
  return (ODR_XL << 4) | (FS_XL << 2) | (LPF1_BW_SEL << 1) | BW0_XL;
}


/// @brief 合成寄存器配置值 CTRL2_G : ODR_G [3:0] | FS_G [1:0] | FS_125 | [0]      (FS_G [1:0]和FS_125的组合在IMU_FS_G_t已经处理)
/// @param ODR_G 陀螺仪数据输出速率(枚举)
/// @param FS_G  陀螺仪量程(枚举)
/// @return 实际寄存器值
uint8_t value_compound_CTRL2_G(IMU_ORD_G_t ODR_G, IMU_FS_G_t FS_G) {
  return  (ODR_G << 4) | (FS_G << 1);
}

/// @brief 合成寄存器配置值 CTRL3_C : BOOT | BDU | H_LACTIVE | PP_OD | SIM | IF_INC | BLE | SW_RESET
/// @param BOOT 重置所有寄存器到硬件默认值
/// @param BDU  在读取输出寄存器的MSB和LSB之前，禁止输出寄存器更新
/// @param H_LACTIVE 反转中断引脚输出电平为低电平
/// @param PP_OD 在中断引脚上使用开漏输出
/// @param SIM  SPI通讯模式选择, false = 4线SPI(4-wire interface)  / true = 3线SPI(3-wire interface)
/// @param IF_INC 多字节访问寄存器时地址自动递增
/// @param BLE   反转多字节数据值MSB LSB的寄存器存储位置
/// @param SW_RESET 软复位,同时会重置所有寄存器到硬件默认值
/// @return 实际寄存器值
uint8_t value_compound_CTRL3_C(bool BOOT,bool BDU,bool H_LACTIVE,bool PP_OD,bool SIM,bool IF_INC,bool BLE,bool SW_RESET){
  return (BOOT<<7) | (BDU<<6) | (H_LACTIVE<<5) | (PP_OD<<4) | (SIM<<3) | (IF_INC<<2 )| (BLE<<1 )| SW_RESET;
}

/// @brief 合成寄存器配置值 CTRL4_C : DEN_XL_EN | SLEEP | INT2_on_INT1 | DEN_DRDY_INT1 | DRDY_MASK | I2C_disable | LPF1_SEL_G | [0]
/// @param DEN_XL_EN 将DEN功能扩展到加速度计
/// @param SLEEP 启用陀螺仪的睡眠模式
/// @param INT2_on_INT1 把所有中断信号仅路由到INT1
/// @param DEN_DRDY_INT1 把DEN_DRDY信号路由到INT1
/// @param DRDY_MASK 启用DRDY_MASK功能
/// @param I2C_disable 禁用I2C通讯
/// @param LPF1_SEL_G 启用陀螺仪数字低通滤波器(LPF1)
/// @return 实际寄存器值
uint8_t value_compound_CTRL4_C(bool DEN_XL_EN,bool SLEEP,bool INT2_on_INT1,bool DEN_DRDY_INT1,bool DRDY_MASK,bool I2C_disable,bool LPF1_SEL_G){
  return (DEN_XL_EN << 7) | (SLEEP << 6) | (INT2_on_INT1 << 5) | (DEN_DRDY_INT1 << 4) | (DRDY_MASK << 3) | (I2C_disable << 2) | (LPF1_SEL_G << 1); 
}

/// @brief 合成寄存器配置值 CTRL6_C: TRIG_EN | LVL_EN | LVL2_EN | XL_HM_MODE | USR_OFF_W |  [0] | FTYPE[1:0]
/// @param TRIG_EN 启用DEN数据边缘触发器
/// @param LVL_EN  启用DEN数据水平触发器
/// @param LVL2_EN 启用DEN数据水平触发器的锁存功能
/// @param XL_HM_MODE 禁用加速度计的高性能工作模式
/// @param USR_OFF_W 加速度计自定义偏移位的权重 false = 2^-10 g/LSB  | true = 2^-6 g/LSB
/// @param FTYPE 陀螺仪低通滤波器（LPF1）带宽选择位
/// @return 实际寄存器值
uint8_t value_compound_CTRL6_C(bool TRIG_EN,bool LVL_EN,bool LVL2_EN,bool XL_HM_MODE,bool USR_OFF_W,IMU_FTYPE_t FTYPE){
  return (TRIG_EN << 7) | (LVL_EN << 6) | (LVL2_EN << 5) | (XL_HM_MODE << 4) | (USR_OFF_W << 3) | FTYPE;
}


/// @brief 合成寄存器配置值 CTRL7_G : G_HM_MODE | HP_EN_G | HPM_G[1:0] | [0] | ROUNDING_STATUS | [0] | [0]
/// @param G_HM_MODE 禁用陀螺仪的高性能工作模式
/// @param HP_EN_G 为陀螺仪启用数字高通滤波器。只有当陀螺仪处于HP模式时，它才会生效
/// @param HPM_G 高通滤波器截止频率
/// @param ROUNDING_STATUS 启用舍入函数,舍入函数使得在一次连续读取中锁定地连续读取某些寄存器而不常规递增地址,包括 WAKE_UP_SRC TAP_SRC D6D_SRC STATUS_REG FUNC_SRC
/// @return 实际寄存器值
uint8_t value_compound_CTRL7_G(bool G_HM_MODE,bool HP_EN_G,IMU_HPM_G_t HPM_G,bool ROUNDING_STATUS){
  return  (G_HM_MODE << 7) | (HP_EN_G << 6) | (HPM_G << 4) | (ROUNDING_STATUS << 2);
}


/// @brief 合成寄存器配置值 CTRL8_XL : LPF2_XL_EN | HPCF_XL[1:0] | HP_REF_MODE | INPUT_COMPOSITE |  HP_SLOPE_XL_EN | [0] | LOW_PASS_ON_6D
/// @param LPF2_XL_EN 启用滤波器LPF2
/// @param HPCF_XL 加速度计滤波器截止设置(部分生效的)
/// @param HP_REF_MODE 启用高通滤波器的参考模式
/// @param INPUT_COMPOSITE 复合滤波器输入选择 false = ODR/2低通滤波 | true = ODR/4低通滤波
/// @param HP_SLOPE_XL_EN 选择使用的是高通滤波器
/// @param LOW_PASS_ON_6D 在6D检测功能使用滤波器LPF2
/// @return 实际寄存器值
uint8_t value_compound_CTRL8_XL(bool LPF2_XL_EN, IMU_HPCF_XL_t HPCF_XL, bool HP_REF_MODE, bool INPUT_COMPOSITE, bool HP_SLOPE_XL_EN, bool LOW_PASS_ON_6D) {
  return (LPF2_XL_EN << 7) | (HPCF_XL << 5) | (HP_REF_MODE << 4) | (INPUT_COMPOSITE << 3) | (HP_SLOPE_XL_EN << 2) | LOW_PASS_ON_6D;
}






