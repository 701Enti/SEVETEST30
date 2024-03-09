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

// I2C相关配置宏定义在board_def.h下
#define LSM6DS3TRC_DEVICE_ADD 0x6A

/// @brief 映射数据库中的配置到LSM6DS3TRC硬件寄存器,即通过数据库写入设定的寄存器
/// @param reg_database 导入数据库，这是一个以IMU_reg_mapping_t的数组，其中的条目即单个IMU_reg_mapping_t数据的个数没有规定，
///                     并且条目的角标与寄存器地址和数据不需要按任何顺序规律进行对应，每个条目只要包含需要写入的寄存器地址和数据，可以参考lsm6ds3trc.h默认配置宏的数据库构建方式
/// @param map_data_num 需要映射的数据条目数量即要写入的寄存器的个数，从第0个条目按数字大小顺序写入map_num个条目为止,map_num需要小于等于条目总数量
void lsm6ds3trc_database_map_set(IMU_reg_mapping_t *reg_database, int map_num)
{
  const char *TAG = "lsm6ds3trc_database_map_set";
  if (!reg_database)
  {
    ESP_LOGE(TAG, "导入了为空的数据库");
    return;
  }
  // 用于偏移到指定数据库内存区域获取数据
  IMU_reg_mapping_t *mapping_buf = reg_database;

  for (int idx = 0; idx < map_num; idx++)
  {
    mapping_buf = &reg_database[idx];
    uint8_t write_buf[2] = {mapping_buf->reg_address, mapping_buf->reg_value};
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
void lsm6ds3trc_database_map_read(IMU_reg_mapping_t *reg_database, int map_num)
{
  const char *TAG = "lsm6ds3trc_database_map_read";
  if (!reg_database)
  {
    ESP_LOGE(TAG, "导入了为空的数据库");
    return;
  }
  // 用于偏移到指定数据库内存区域写入数据库
  IMU_reg_mapping_t *mapping_buf = reg_database;

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
uint16_t lsm6ds3trc_data_get_step_counter()
{
  IMU_reg_mapping_t reg_database[2] = {
      MAP_BASE(REG_ADD_STEP_COUNTER_L, 0x00),
      MAP_BASE(REG_ADD_STEP_COUNTER_H, 0x00),
  };
  lsm6ds3trc_database_map_read(reg_database, 2);

  uint16_t ret = reg_database[0].reg_value | (reg_database[1].reg_value << 8);
  return ret;
}

/// @brief 获取实时的加速度值
/// @return 实时的加速度值,一帧XYZ轴的加速度
IMU_acceleration_value_t lsm6ds3trc_data_get_step_counter()
{
  const char *TAG = "lsm6ds3trc_data_get_step_counter";
  IMU_reg_mapping_t reg_database[7] = {
      MAP_BASE(REG_ADD_CTRL1_XL, 0x00),

      MAP_BASE(REG_ADD_OUTX_L_XL, 0x00),
      MAP_BASE(REG_ADD_OUTX_H_XL, 0x00),

      MAP_BASE(REG_ADD_OUTY_L_XL, 0x00),
      MAP_BASE(REG_ADD_OUTY_H_XL, 0x00),

      MAP_BASE(REG_ADD_OUTZ_L_XL, 0x00),
      MAP_BASE(REG_ADD_OUTZ_H_XL, 0x00),
  };
  lsm6ds3trc_database_map_read(reg_database, 7);

  float LA_SO_buf = 0;
  switch (reg_database[0].value >> 2 & 0x03)
  {
  case IMU_LA_FS_2G:
    LA_SO_buf = IMU_LA_SO_FS_2G;
    break;
  case IMU_LA_FS_4G:
    LA_SO_buf = IMU_LA_SO_FS_4G;
    break;
  case IMU_LA_FS_8G:
    LA_SO_buf = IMU_LA_SO_FS_8G;
    break;
  case IMU_LA_FS_16G:
    LA_SO_buf = IMU_LA_SO_FS_16G;
    break;

  default:
    ESP_LOGE(TAG, "未知的加速度量程配置");
    return;
    break;
  }
  IMU_acceleration_value_t value;
  memset(&value, 0, sizeof(value));
  value.x = (reg_database[1] | reg_database[2] << 8) * LA_SO


}
