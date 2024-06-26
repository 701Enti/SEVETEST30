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

 // 该文件归属701Enti组织，SEVETEST30开发团队应该提供责任性维护，包含各种SE30对姿态传感器LSM6DS3TR-C的操作
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
 //       对于功能设计需求，并不是所有寄存器都被考虑，都要求配置，实际上可以遵从默认设置
 //       FIFO读出数据可能存在无效样本,需要自行过滤,(过滤大于等于0x7FF0样本)
 // 您可以在ST官网获取LSM6DS3TR-C的相关手册包含程序实现思路 https://www.st.com/zh/mems-and-sensors/lsm6ds3tr-c.html
 // github: https://github.com/701Enti
 // bilibili: 701Enti
 // [基本初始化] 设置加速度计和陀螺仪的ORD（为了获得中断输出,不得设置加速度计到掉电模式)，以及XL_HM_MODE位的设置
 //             开启BDU和DRDY_MASK 配置INT1 配置自动记录相关模块
 //             设置FIFO抽取系数 设置FIFO_ORD   设置 加速度 角速率 数据源到FIFO   配置为FIFO Continuous mode模式 不使用中断
 // [读取步骤] 读出FIFO存储的数据 / 读出自动记录的数据
 // [快捷监测] 6D/4D检测 自由落体检测 计步器
 // [默认中断] INT1上路由 - 自由落体事件
 // [中断锁存] 默认启用
 
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
/// @param map_num 需要映射的数据条目数量即要写入的寄存器的个数，从第0个条目按数字大小顺序写入map_num个条目为止,map_num需要小于等于条目总数量
/// @return ESP_OK / ESP_FAIL
esp_err_t lsm6ds3trc_database_map_set(IMU_reg_mapping_t* reg_database, int map_num)
{
  const char* TAG = "lsm6ds3trc_database_map_set";
  if (!reg_database)
  {
    ESP_LOGE(TAG, "导入了为空的数据库");
    return ESP_FAIL;
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
      return ESP_FAIL;
    }
  }

  ESP_LOGI(TAG, "成功映射配置到姿态传感器LSM6DS3TRC");
  return ESP_OK;
}

/// @brief 映射LSM6DS3TRC硬件寄存器的数据到数据库,即读取设定的寄存器到数据库
/// @param reg_database 导入数据库，这是一个以IMU_reg_mapping_t的数组，其中的条目即单个IMU_reg_mapping_t数据的个数没有规定，
///                     并且条目的角标与寄存器地址和数据不需要按任何顺序规律进行对应，每个条目只要包含需要写入的寄存器地址和数据，可以参考lsm6ds3trc.h默认配置宏的数据库构建方式
//                      完成读取后,数据库中寄存器的值将更新,因此在初始化数据库设定寄存器时的值任意如0x00
/// @param map_num 需要映射的数据条目数量即要写入的寄存器的个数，从第0个条目按数字大小顺序写入map_num个条目为止,map_num需要小于等于条目总数量
/// @return ESP_OK / ESP_FAIL
esp_err_t lsm6ds3trc_database_map_read(IMU_reg_mapping_t* reg_database, int map_num)
{
  const char* TAG = "lsm6ds3trc_database_map_read";
  if (!reg_database)
  {
    ESP_LOGE(TAG, "导入了为空的数据库");
    return ESP_FAIL;
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
      return ESP_FAIL;
    }
  }

  return ESP_OK;
}

/// @brief [FIFO数据映射]
/// @brief FIFO是姿态传感器中一种常用的缓存策略,FIFO具有抽取功能,可以采集大量的传感数据的一小部分用于参考
/// @brief 同时也压缩了数据,因为数据实时性需求大,一帧FIFO数据往往包含多个传感器的数据
/// @brief 配置抽取的方式不同,FIFO中各种数据的顺序会发生一定变化,通过配置映射数据库来告知读取函数这些数据的顺序含义
/// @brief FIFO映射数据库使用寄存器值配置数据库的完全相同逻辑,但是不同单元的读写顺序显然相比更加重要
/// @brief 键值对的 address 表示姿态传感器寄存器读取目标,显然它们是FIFO数据寄存器的地址
/// @brief 键值对的 value 表示该寄存器值读取之后,存储到uint8_t的数组的首地址,不同数据显然需要保存在不同uint8_t的数组,取决于外部函数的数据处理方式
/// @brief 每次读取从这些地址开始导入,在data_num控制下偏移地址地存储数据
/// @param FIFO_database FIFO映射数据库,条目键值对的 value 设置为NULL表示舍弃该条目数据读取
/// @param map_num  (必须准确)数据库的条目数量即MAP_BASE个数
/// @param read_num (必须准确)读取的FIFO数据帧个数,一帧FIFO数据往往包含多个传感器的数据
/// @return ESP_OK / ESP_FAIL
esp_err_t lsm6ds3trc_FIFO_map(IMU_reg_mapping_t* FIFO_database,int map_num,int read_num){
  const char* TAG = "lsm6ds3trc_FIFO_map";

  //参数合法性检查
  if(!FIFO_database){
    ESP_LOGE(TAG, "导入了为空的FIFO映射数据库");
    return ESP_FAIL;
  }

  //初始化读取缓存
  IMU_reg_mapping_t* buf_db = NULL;//创建用于缓存的寄存器数据库
  buf_db = (IMU_reg_mapping_t*)malloc(sizeof(IMU_reg_mapping_t)*map_num);
  memset(buf_db,0,sizeof(IMU_reg_mapping_t)*map_num);
  for (int a=0;a<map_num;a++)//映射需要依次读取的寄存器地址
    buf_db[a].reg_address = FIFO_database[a].reg_address;
  
  //读取循环,共映射read_num帧数据
  static int idx=0;
  while (idx<read_num){
    //读取一帧数据,FIFO内部会根据读取数据次数偏移到存储区,如果合理,每次读到的顺序是FIFO_database设置的顺序
    if(lsm6ds3trc_database_map_read(buf_db,map_num) != ESP_OK){
      ESP_LOGW(TAG, "数据读取意外终止,数据被部分映射 第 %d 个无法读取 / 共有 %d 个,等待重试",idx+1,read_num);
      if(lsm6ds3trc_database_map_read(buf_db,map_num) != ESP_OK){
        ESP_LOGW(TAG, "重试读取失败,等待下次调用");
        break;
      }
    }
    //映射这帧数据到所有预设内存区域
    for(int dp=0;dp<map_num;dp++){
      //遍历所有条目,在预设的内存区域写入,用idx偏移到指定数组元素,而FIFO_database[dp].reg_value的值不变
      if((uint8_t*)FIFO_database[dp].reg_value){//条目键值对的 value 设置为NULL表示舍弃该条目数据读取
        *((uint8_t*)(FIFO_database[dp].reg_value + idx)) =  buf_db[dp].reg_value;//FIFO_database[dp].reg_value的值不变
      }
    }

    idx++;
  }

  //释放内存
  free(buf_db);
  buf_db = NULL;

  if(idx < read_num)
   return ESP_FAIL;//idx的值将不会复位,使得下次开始从错误位置再次读取
   
  idx = 0;
  return ESP_OK;
  
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
      MAP_BASE(STEP_COUNTER_L, 0x00),
      MAP_BASE(STEP_COUNTER_H, 0x00),
  };
  lsm6ds3trc_database_map_read(reg_database, 2);

  uint16_t ret = reg_database[0].reg_value | (reg_database[1].reg_value << 8);
  return ret;
}


IMU_D6D_data_value_t lsm6ds3trc_get_D6D_data_value()
{
  IMU_reg_mapping_t reg_database[1] = {
      MAP_BASE(D6D_SRC, 0x00),
  };
  lsm6ds3trc_database_map_read(reg_database, 1);

  IMU_D6D_data_value_t data_value = {
    .XL = reg_database[0].reg_value,
    .XH = reg_database[0].reg_value >> 1,
    .YL = reg_database[0].reg_value >> 2,
    .YH = reg_database[0].reg_value >> 3,
    .ZL = reg_database[0].reg_value >> 4,
    .ZH = reg_database[0].reg_value >> 5,
  };

  return data_value;
}


/// @brief 检测是否处于自由落体状态
/// @return 自由落体状态标志
bool lsm6ds3trc_get_free_fall_status(){
  //构建映射数据库设置读取的目标
  IMU_reg_mapping_t reg_database[1] = {
      MAP_BASE(WAKE_UP_SRC, 0x00),
  };
  lsm6ds3trc_database_map_read(reg_database,1);//执行映射读取操作
  return reg_database[0].reg_value >> 5;
}



/// @brief 获取传感器的实时温度(1000 = 1摄氏温度)
/// @return 实时温度(1000 = 1摄氏温度)
int lsm6ds3trc_get_now_temperature()
{
  //构建映射数据库设置读取的目标
  IMU_reg_mapping_t reg_database[3] = {
      MAP_BASE(OUT_TEMP_H, 0x00),
      MAP_BASE(OUT_TEMP_L, 0x00),
      MAP_BASE(STATUS_REG, 0x00),
  };

  //等待数据成功获取
  while (!(reg_database[2].reg_value & 0x04)) {
    lsm6ds3trc_database_map_read(reg_database,3);//执行映射读取操作
  }

  return ((OUT_TEMP_H << 8) | OUT_TEMP_L)*1000/256 + 25;
}


/// @brief 获取实时的加速度偏移(支持DRDY_MASK下无效样本过滤)
/// @return 实时的加速度值偏移
IMU_acceleration_value_t lsm6ds3trc_gat_now_acceleration()
{
  const char* TAG = "lsm6ds3trc_gat_now_acceleration";

  static IMU_acceleration_value_t value_buf = { 0,0,0 };//存储上次的数据以计算偏移量
  IMU_acceleration_value_t acceleration_value = { 0,0,0 };//存储本次数据
  IMU_acceleration_value_t ret = { 0,0,0 };//存储返回值

  //构建映射数据库设置读取的目标
  IMU_reg_mapping_t reg_database[8] = {
      MAP_BASE(CTRL1_XL, 0x00),

      MAP_BASE(OUTX_L_XL, 0x00),
      MAP_BASE(OUTX_H_XL, 0x00),

      MAP_BASE(OUTY_L_XL, 0x00),
      MAP_BASE(OUTY_H_XL, 0x00),

      MAP_BASE(OUTZ_L_XL, 0x00),
      MAP_BASE(OUTZ_H_XL, 0x00),

      MAP_BASE(STATUS_REG, 0x00),
  };

  //等待加速度数据成功获取
  while (!(reg_database[7].reg_value & 0x01)) {
    lsm6ds3trc_database_map_read(reg_database, 8);//执行映射读取操作
  }

  float LA_SO_buf = 0;//通过设置的量程获取对应缩放因数
  uint32_t max_buf = 0;//量程内允许最大绝对值
  switch (reg_database[0].reg_value >> 2 & 0x03)
  {
  case IMU_FS_XL_2G:
    LA_SO_buf = IMU_LA_SO_FS_2G;
    max_buf = 2 * 1000;
    break;
  case IMU_FS_XL_4G:
    LA_SO_buf = IMU_LA_SO_FS_4G;
    max_buf = 4 * 1000;
    break;
  case IMU_FS_XL_8G:
    LA_SO_buf = IMU_LA_SO_FS_8G;
    max_buf = 8 * 1000;
    break;
  case IMU_FS_XL_16G:
    LA_SO_buf = IMU_LA_SO_FS_16G;
    max_buf = 16 * 1000;
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

/// @brief 获取实时的角速率偏移(支持DRDY_MASK下无效样本过滤)
/// @return 实时的角速率偏移值
IMU_angular_rate_value_t lsm6ds3trc_gat_now_angular_rate()
{
  const char* TAG = "lsm6ds3trc_gat_now_angular_rate";

  static IMU_angular_rate_value_t value_buf = { 0,0,0 };//存储上次的数据以计算偏移量
  IMU_angular_rate_value_t angular_rate_value = { 0,0,0 };//存储本次数据
  IMU_angular_rate_value_t ret = { 0,0,0 };//存储返回值

  //构建映射数据库设置读取的目标
  IMU_reg_mapping_t reg_database[8] = {
      MAP_BASE(CTRL2_G, 0x00),

      MAP_BASE(OUTX_L_G, 0x00),
      MAP_BASE(OUTX_H_G, 0x00),

      MAP_BASE(OUTY_L_G, 0x00),
      MAP_BASE(OUTY_H_G, 0x00),

      MAP_BASE(OUTZ_L_G, 0x00),
      MAP_BASE(OUTZ_H_G, 0x00),

      MAP_BASE(STATUS_REG, 0x00),
  };

  //等待数据成功获取
  while (!(reg_database[7].reg_value & 0x02)) {
    lsm6ds3trc_database_map_read(reg_database, 8);//执行映射读取操作
  }

  //通过设置的量程获取对应缩放因数
  float G_SO_buf = 0;
  uint32_t max_buf = 0;//量程内允许最大绝对值
  switch (reg_database[0].reg_value >> 1 & 0x07)
  {
  case IMU_FS_G_125DPS:
    G_SO_buf = IMU_G_SO_FS_125DPS;
    max_buf = 125 * 1000;
    break;
  case IMU_FS_G_250DPS:
    G_SO_buf = IMU_G_SO_FS_250DPS;
    max_buf = 250 * 1000;
    break;
  case IMU_FS_G_500DPS:
    G_SO_buf = IMU_G_SO_FS_500DPS;
    max_buf = 500 * 1000;
    break;
  case IMU_FS_G_1000DPS:
    G_SO_buf = IMU_G_SO_FS_1000DPS;
    max_buf = 1000 * 1000;
    break;
  case IMU_FS_G_2000DPS:
    G_SO_buf = IMU_G_SO_FS_2000DPS;
    max_buf = 2000 * 1000;
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
  if (abs(angular_rate_value.x) <= max_buf) {
    ret.x = angular_rate_value.x - value_buf.x;//计算偏移量作为返回值
    value_buf.x = angular_rate_value.x;//存储本次的数据以准备为下次读取计算偏移量    
  }

  if (abs(angular_rate_value.y) <= max_buf) {
    ret.y = angular_rate_value.y - value_buf.y;
    value_buf.y = angular_rate_value.y;
  }

  if (abs(angular_rate_value.z) <= max_buf) {
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
uint8_t value_compound_CTRL3_C(bool BOOT, bool BDU, bool H_LACTIVE, bool PP_OD, bool SIM, bool IF_INC, bool BLE, bool SW_RESET) {
  return (BOOT << 7) | (BDU << 6) | (H_LACTIVE << 5) | (PP_OD << 4) | (SIM << 3) | (IF_INC << 2) | (BLE << 1) | SW_RESET;
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
uint8_t value_compound_CTRL4_C(bool DEN_XL_EN, bool SLEEP, bool INT2_on_INT1, bool DEN_DRDY_INT1, bool DRDY_MASK, bool I2C_disable, bool LPF1_SEL_G) {
  return (DEN_XL_EN << 7) | (SLEEP << 6) | (INT2_on_INT1 << 5) | (DEN_DRDY_INT1 << 4) | (DRDY_MASK << 3) | (I2C_disable << 2) | (LPF1_SEL_G << 1);
}

/// @brief 合成寄存器配置值 CTRL6_C: TRIG_EN | LVL_EN | LVL2_EN | XL_HM_MODE | USR_OFF_W |  [0] | FTYPE[1:0] 
/// @brief 这里请格外注意该高级配置的合理性,不合理的滤波配置会对全局的数据处理产生影响,甚至使如自由落体等中断检测过敏感而无法正常工作
/// @param TRIG_EN 启用DEN数据边缘触发器
/// @param LVL_EN  启用DEN数据水平触发器
/// @param LVL2_EN 启用DEN数据水平触发器的锁存功能
/// @param XL_HM_MODE 禁用加速度计的高性能工作模式
/// @param USR_OFF_W 加速度计自定义偏移位的权重 false = 2^-10 g/LSB  | true = 2^-6 g/LSB
/// @param FTYPE 陀螺仪低通滤波器（LPF1）带宽选择位
/// @return 实际寄存器值
uint8_t value_compound_CTRL6_C(bool TRIG_EN, bool LVL_EN, bool LVL2_EN, bool XL_HM_MODE, bool USR_OFF_W, IMU_FTYPE_t FTYPE) {
  return (TRIG_EN << 7) | (LVL_EN << 6) | (LVL2_EN << 5) | (XL_HM_MODE << 4) | (USR_OFF_W << 3) | FTYPE;
}


/// @brief 合成寄存器配置值 CTRL7_G : G_HM_MODE | HP_EN_G | HPM_G[1:0] | [0] | ROUNDING_STATUS | [0] | [0]
/// @param G_HM_MODE 禁用陀螺仪的高性能工作模式
/// @param HP_EN_G 为陀螺仪启用数字高通滤波器。只有当陀螺仪处于HP模式时，它才会生效
/// @param HPM_G 高通滤波器截止频率
/// @param ROUNDING_STATUS 启用舍入函数,舍入函数使得在一次连续读取中锁定地连续读取某些寄存器而不常规递增地址,包括 WAKE_UP_SRC TAP_SRC D6D_SRC STATUS_REG FUNC_SRC
/// @return 实际寄存器值
uint8_t value_compound_CTRL7_G(bool G_HM_MODE, bool HP_EN_G, IMU_HPM_G_t HPM_G, bool ROUNDING_STATUS) {
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


/// @brief 合成寄存器配置值 CTRL10_C : WRIST_TILT_EN | [0] | TIMER_EN | PEDO_EN | TILT_EN | FUNC_EN | PEDO_RST_STEP | SIGN_MOTION_EN
/// @param WRIST_TILT_EN 启用wrist斜率运算程序
/// @param TIMER_EN 启用计数时间戳
/// @param PEDO_EN 启用计步器运算程序
/// @param TILT_EN 使能斜率计算
/// @param FUNC_EN 启用嵌入式功能(包括但不限于计步器等)
/// @param PEDO_RST_STEP 重置计步器的计数值
/// @param SIGN_MOTION_EN 使能显著运动检测功能
/// @return 实际寄存器值
uint8_t value_compound_CTRL10_C(bool WRIST_TILT_EN, bool TIMER_EN, bool PEDO_EN, bool TILT_EN, bool FUNC_EN, bool PEDO_RST_STEP, bool SIGN_MOTION_EN) {
  return(WRIST_TILT_EN << 7) | (TIMER_EN << 5) | (PEDO_EN << 4) | (TILT_EN << 3) | (FUNC_EN << 2) | (PEDO_RST_STEP << 1) | SIGN_MOTION_EN;
}


/// @brief 合成寄存器配置值 INT1_CTRL : INT1_STEP_DETECTOR | INT1_SIGN_MOT | INT1_FULL_FLAG | INT1_FIFO_OVR | INT1_FTH | INT1_BOOT | INT1_DRDY_G | INT1_DRDY_XL
/// @param INT1_STEP_DETECTOR 在INT1启用中断- 计步器计数
/// @param INT1_SIGN_MOT 在INT1启用中断- 显著运动检测
/// @param INT1_FULL_FLAG 在INT1启用中断- FIFO数据填充完成
/// @param INT1_FIFO_OVR 在INT1启用中断- FIFO数据溢出
/// @param INT1_FTH 在INT1启用中断- FIFO阈值中断
/// @param INT1_BOOT 在INT1启用中断- BOOT引导状态
/// @param INT1_DRDY_G 在INT1启用中断- 陀螺仪数据准备完毕
/// @param INT1_DRDY_XL 在INT1启用中断- 加速度计数据准备完毕
/// @return 实际寄存器值
uint8_t value_compound_INT1_CTRL(bool INT1_STEP_DETECTOR, bool INT1_SIGN_MOT, bool INT1_FULL_FLAG, bool INT1_FIFO_OVR, bool INT1_FTH, bool INT1_BOOT, bool INT1_DRDY_G, bool INT1_DRDY_XL) {
  return (INT1_STEP_DETECTOR << 7) | (INT1_SIGN_MOT << 6) | (INT1_FULL_FLAG << 5) | (INT1_FIFO_OVR << 4) | (INT1_FTH << 3) | (INT1_BOOT << 2) | (INT1_DRDY_G << 1) | INT1_DRDY_XL;
}



/// @brief 合成寄存器配置值 INT2_CTRL : INT2_STEP_DELTA | INT2_STEP_COUNT_OV | INT2_FULL_FLAG | INT2_FIFO_OVR | INT2_FTH | INT2_DRDY_TEMP | INT2_DRDY_G | INT2_DRDY_XL
/// @param INT2_STEP_DELTA 在INT2启用中断- 计步器计数
/// @param INT2_STEP_COUNT_OV 在INT2启用中断- 计步器计数溢出中断
/// @param INT2_FULL_FLAG 在INT2启用中断- FIFO数据填充完成
/// @param INT2_FIFO_OVR 在INT2启用中断- FIFO数据溢出
/// @param INT2_FTH 在INT2启用中断- FIFO阈值中断
/// @param INT2_DRDY_TEMP 在INT2启用中断- 温度数据就绪
/// @param INT2_DRDY_G 在INT2启用中断- 陀螺仪数据准备完毕
/// @param INT2_DRDY_XL 在INT2启用中断- 加速度计数据准备完毕
/// @return 实际寄存器值
uint8_t value_compound_INT2_CTRL(bool INT2_STEP_DELTA, bool INT2_STEP_COUNT_OV, bool INT2_FULL_FLAG, bool INT2_FIFO_OVR, bool INT2_FTH, bool INT2_DRDY_TEMP, bool INT2_DRDY_G, bool INT2_DRDY_XL) {
  return (INT2_STEP_DELTA << 7) | (INT2_STEP_COUNT_OV << 6) | (INT2_FULL_FLAG << 5) | (INT2_FIFO_OVR << 4) | (INT2_FTH << 3) | (INT2_DRDY_TEMP << 2) | (INT2_DRDY_G << 1) | INT2_DRDY_XL;
}


/// @brief 合成寄存器配置值 MD1_CFG : INT1_INACT_STATE | INT1_SINGLE_TAP | INT1_WU | INT1_FF | INT1_DOUBLE_TAP | INT1_6D | INT1_TILT | INT1_TIMER
/// @param INT1_INACT_STATE INT1上路由 - 静默事件
/// @param INT1_SINGLE_TAP INT1上路由 - Single-tap识别事件
/// @param INT1_WU INT1上路由 - 唤醒事件
/// @param INT1_FF INT1上路由 - 自由落体事件
/// @param INT1_DOUBLE_TAP INT1上路由 - double-tap识别事件
/// @param INT1_6D INT1上路由 - 6D相关事件
/// @param INT1_TILT INT1上路由 - 倾斜事件
/// @param INT1_TIMER INT1上路由 - 计时器停止事件
/// @return 实际寄存器值
uint8_t value_compound_MD1_CFG(bool INT1_INACT_STATE, bool INT1_SINGLE_TAP, bool INT1_WU, bool INT1_FF, bool INT1_DOUBLE_TAP, bool INT1_6D, bool INT1_TILT, bool INT1_TIMER) {
  return (INT1_INACT_STATE << 7) | (INT1_SINGLE_TAP << 6) | (INT1_WU << 5) | (INT1_FF << 4) | (INT1_DOUBLE_TAP << 3) | (INT1_6D << 2) | (INT1_TILT << 1) | INT1_TIMER;
}

/// @brief 合成寄存器配置值的一部分 FREE_FALL : FF_DUR4(未包含,默认值0)|FF_DUR3(未包含,默认值0)|FF_DUR2(未包含,默认值0)|FF_DUR1(未包含,默认值0)|FF_DUR0(未包含,默认值0) | FF_THS2 | FF_THS1 | FF_THS0
/// @param FF_THS 自由落体检测触发阈值
/// @return 可能需要继续加工的实际寄存器值
uint8_t value_compound_partly_FREE_FALL(IMU_FF_THS_t FF_THS) {
  return FF_THS;
}

/// @brief 合成寄存器配置值的一部分 TAP_THS_6D : D4D_EN | SIXD_THS[1:0] | TAP_THS[4:0](未包含,默认值00000)
/// @param D4D_EN 关闭4D检测,使用6D检测
/// @param SIXD_THS 4D/6D检测功能的阈值
/// @return 可能需要继续加工的实际寄存器值
uint8_t value_compound_partly_TAP_THS_6D(bool D4D_EN, IMU_SIXD_THS_t SIXD_THS) {
  return (D4D_EN << 7) | (SIXD_THS << 5);
}

/// @brief 合成寄存器配置值 TAP_CFG : INTERRUPTS_ENABLE | INACT_EN1 INACT_EN0 | SLOPE_FDS | TAP_X_EN | TAP_Y_EN | TAP_Z_EN | LIR
/// @param INTERRUPTS_ENABLE 使能基本中断(6D/4D 自由落体检测 唤醒事件 静默事件 Single-tap/double-tap)
/// @param INACT_EN 静默功能的设置
/// @param SLOPE_FDS 在唤醒和静默/活动检测上选择高通滤波
/// @param TAP_X_EN 在tap识别中启用X方向
/// @param TAP_Y_EN 在tap识别中启用Y方向
/// @param TAP_Z_EN 在tap识别中启用Z方向
/// @param LIR 中断锁存
/// @return 实际寄存器值
uint8_t value_compound_TAP_CFG(bool INTERRUPTS_ENABLE, IMU_INACT_EN_t INACT_EN, bool SLOPE_FDS, bool TAP_X_EN, bool TAP_Y_EN, bool TAP_Z_EN, bool LIR) {
  return (INTERRUPTS_ENABLE << 7) | (INACT_EN << 5) | (SLOPE_FDS << 4) | (TAP_X_EN << 3) | (TAP_Y_EN << 2) | (TAP_Z_EN << 1) | LIR;
}



/// @brief 合成寄存器配置值 FIFO_CTRL3 : [0] [0] | DEC_FIFO_GYRO[2:0] | DEC_FIFO_XL[2:0]
/// @param DEC_FIFO_GYRO 陀螺仪FIFO数据集抽取设置
/// @param DEC_FIFO_XL   加速度计FIFO数据集抽取设置
/// @return 实际寄存器值
uint8_t value_compound_FIFO_CTRL3(IMU_DEC_FIFO_GYRO_t DEC_FIFO_GYRO, IMU_DEC_FIFO_XL_t DEC_FIFO_XL) {
  return (DEC_FIFO_GYRO << 3) | DEC_FIFO_XL;
}




/// @brief // 合成寄存器配置值 FIFO_CTRL5 : [0] | ODR_FIFO[3:0] | FIFO_MODE[2:0]
/// @param ODR_FIFO FIFO_ORD选择
/// @param FIFO_MODE FIFO运行模式
/// @return 实际寄存器值
uint8_t value_compound_FIFO_CTRL5(IMU_ODR_FIFO_t ODR_FIFO, IMU_FIFO_MODE_t FIFO_MODE) {
  return (ODR_FIFO << 3) | FIFO_MODE;
}