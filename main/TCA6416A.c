
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

// 包含一些ESP32_S3通过硬件外设与TCA6416建立配置与扩展IO数据的通讯
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 本库特性：1 由于IO控制时，有随时需要调用TCA6416A写入函数的需求，本库不会出现调用一次函数归定只能改一个IO或读一个IO还要传一系列参数的尴尬问题，而是一齐读写,同时还会保存实时IO数据，因此没有用到电平反转寄存器
//          2 使用时直接修改公共变量以在项目非常方便使用，加之，可以像sevetest30_gpio.c封装后使用FreeRTOS支持，并添加中断支持，一但IO电平变化就读取，没有变就不读，客观上可以大大提高资源利用率
// 读写原理：   运用结构体地址一般为结构体中第一个成员变量地址，并且本例中，成员类型均为bool,地址递加从而可以方便地扫描所有成员，
// 敬告： 0 为更加方便后续开发或移植，本库不包含关于FreeRTOS支持的封装，公共变量修改方式的服务封装，以及中断服务的封装，如果需要参考，请参照sevetest30_gpio.c
//       1 文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
//       2 请注意外部引脚模式设置，错误的配置可能导致您的设备损坏，我们不建议修改这些默认配置
//       3 对于设计现实的不同，您可以更改结构体成员变量名，但是必须确保对应的IO次序不变如 P00 P01 P02 P03 以此类推
//         同时成员变量名是上级程序识别操作引脚的关键，如果需要使用其上级程序而不仅仅是TCA6416A库函数，结构体成员变量名不应该随意修改，对当前硬件的更新必须修改上层代码
// github: https://github.com/701Enti
// bilibili: 701Enti

#include "TCA6416A.h"

#include "driver/i2c.h"
#include "esp_log.h"

#include "board_def.h"

uint8_t TCA6416A_data_buf[] = {0x00, 0x00}; // 缓存寄存器地址与数据 {reg + data}

/// @brief GPIO输入输出模式设置，同时保存模式数据，初始化用
/// @param pTCA6416Amode TCA6416A_mode_t模式配置存储位置
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_ARG 参数错误]
/// @return [ESP_FAIL 发送命令时发现问题, TCA6416A未应答]
/// @return [ESP_ERR_INVALID_STATE I2C driver 未安装或没有运行在主机模式]
/// @return [ESP_ERR_TIMEOUT 操作超时因为总线忙]
esp_err_t TCA6416A_gpio_mode_set(TCA6416A_mode_t *pTCA6416Amode)
{
  const char *TAG = "TCA6416A_gpio_mode_set";
  esp_err_t ret = ESP_OK;

  if (pTCA6416Amode == NULL)
  {
    ESP_LOGE(TAG, "无法处理的空指针");
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t data1 = NULL, data2 = NULL; // 临时数据缓存
  bool *p;                            // 定义指针变量，指向成员变量地址

  // 确定设备地址
  uint8_t i2c_add = 0x20;
  if (TCA6416A_ADDR_LEVEL)
    i2c_add = 0x21;

  // 依据pTCA6416Amode结构体递增地址对应的bool数值按8位缓冲变量对应的bit位置装载
  int i = 0;
  for (i = 0; i < 16; i++)
  {
    p = (bool *)pTCA6416Amode + i; // 强制转换地址的类型，指向成员变量地址
    if (i < 8)
      data1 = *p << i | data1; // 取出值进行运算
    if (i >= 8)
      data2 = *p << (i - 8) | data2; // 取出值进行运算
  }

  ESP_LOGW(TAG, "准备进行TCA6416A引脚模式设置,准备写入:\nP00-P07: [P00 %d] [P01 %d] [P02 %d] [P03 %d] [P04 %d] [P05 %d] [P06 %d] [P07 %d]\nP10-P17: [P10 %d] [P11 %d] [P12 %d] [P13 %d] [P14 %d] [P15 %d] [P16 %d] [P17 %d]",
           data1 & 0x01, (data1 >> 1) & 0x01, (data1 >> 2) & 0x01, (data1 >> 3) & 0x01, (data1 >> 4) & 0x01, (data1 >> 5) & 0x01, (data1 >> 6) & 0x01, (data1 >> 7) & 0x01,
           data2 & 0x01, (data2 >> 1) & 0x01, (data2 >> 2) & 0x01, (data2 >> 3) & 0x01, (data2 >> 4) & 0x01, (data2 >> 5) & 0x01, (data2 >> 6) & 0x01, (data2 >> 7) & 0x01);

  // 装载并写入
  TCA6416A_data_buf[0] = TCA6416A_MODE1, TCA6416A_data_buf[1] = data1;
  ret = i2c_master_write_to_device(DEVICE_I2C_PORT, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }
  TCA6416A_data_buf[0] = TCA6416A_MODE2, TCA6416A_data_buf[1] = data2;
  ret = i2c_master_write_to_device(DEVICE_I2C_PORT, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }

  // 回读
  TCA6416A_data_buf[0] = TCA6416A_MODE1;
  ret = i2c_master_write_read_device(DEVICE_I2C_PORT, i2c_add, &TCA6416A_data_buf[0], sizeof(TCA6416A_data_buf[0]), &data1, sizeof(data1), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }
  TCA6416A_data_buf[0] = TCA6416A_MODE2;
  ret = i2c_master_write_read_device(DEVICE_I2C_PORT, i2c_add, &TCA6416A_data_buf[0], sizeof(TCA6416A_data_buf[0]), &data2, sizeof(data2), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGW(TAG, "TCA6416A引脚模式设置完成,当前寄存器回读:\nP00-P07: [P00 %d] [P01 %d] [P02 %d] [P03 %d] [P04 %d] [P05 %d] [P06 %d] [P07 %d]\nP10-P17: [P10 %d] [P11 %d] [P12 %d] [P13 %d] [P14 %d] [P15 %d] [P16 %d] [P17 %d]",
           data1 & 0x01, (data1 >> 1) & 0x01, (data1 >> 2) & 0x01, (data1 >> 3) & 0x01, (data1 >> 4) & 0x01, (data1 >> 5) & 0x01, (data1 >> 6) & 0x01, (data1 >> 7) & 0x01,
           data2 & 0x01, (data2 >> 1) & 0x01, (data2 >> 2) & 0x01, (data2 >> 3) & 0x01, (data2 >> 4) & 0x01, (data2 >> 5) & 0x01, (data2 >> 6) & 0x01, (data2 >> 7) & 0x01);

  return ESP_OK;
}

/// @brief GPIO引脚电平数据交互服务，一次性全更新、写入，根据引脚模式配置以读写操作，以下是我一些肤浅的思路
// 写：直接写入输出寄存器，即使输入引脚也一同写入，因为对于TCA6416,这样的数据无效，没有任何影响
// 读：先读出两个输入寄存器的值，全部映射到level结构体中，再读取两个输出寄存器的值，仅对输出引脚数据进行映射到level结构体中，最终返回给上层程序
// 读数据由传入的结构体地址对应的结构体中按成员直接回读
/// @param  pTCA6416Alevel TCA6416A_level_t电平数据存储位置
/// @param pTCA6416Amode TCA6416A_mode_t模式配置存储位置
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_ARG 参数错误]
/// @return [ESP_FAIL 发送命令时发现问题, TCA6416A未应答]
/// @return [ESP_ERR_INVALID_STATE I2C driver 未安装或没有运行在主机模式]
/// @return [ESP_ERR_TIMEOUT 操作超时因为总线忙]
esp_err_t TCA6416A_gpio_level_service(TCA6416A_level_t *pTCA6416Alevel, TCA6416A_mode_t *pTCA6416Amode)
{
  const char *TAG = "TCA6416A_gpio_level_service";
  esp_err_t ret = ESP_OK;

  if (pTCA6416Alevel == NULL || pTCA6416Amode == NULL)
  {
    ESP_LOGE(TAG, "无法处理的空指针");
    return ESP_ERR_INVALID_ARG;
  }

  bool *p, *m;                        // 定义指针变量，指向成员变量地址
  uint8_t data1 = NULL, data2 = NULL; // 临时数据缓存

  // 确定设备地址
  uint8_t i2c_add = 0x20;
  if (TCA6416A_ADDR_LEVEL)
    i2c_add = 0x21;

  // 准备好两份8bit数据
  // 依据pTCA6416Avalue结构体递增地址对应的bool数值按8位缓冲变量对应的bit位置装载
  int i = 0;
  for (i = 0; i < 16; i++)
  {
    p = (bool *)pTCA6416Alevel + i; // 强制转换地址的类型，指向成员变量地址
    if (i < 8)
      data1 = *p << i | data1; // 取出值进行运算
    if (i >= 8)
      data2 = *p << (i - 8) | data2; // 取出值进行运算
  }

  ESP_LOGW(TAG, "准备进行TCA6416A输出引脚电平设置,准备写入:\nP00-P07: [QC_TOUCH_L %d][EN_LED_BOARD %d][HP_DETECT %d][BAT_QSTRT %d][BAT_ALRT %d][EMF_DRDY %d][amplifier_MUTE %d][amplifier_SD %d]\nP10-P17: [IMU_INT2 %d][IMU_INT1 %d][ALS_INT %d][thumbwheel_CCW %d][thumbwheel_CW %d][OTG_EN %d][charge_SIGN %d][QC_TOUCH_R %d]",
           data1 & 0x01, (data1 >> 1) & 0x01, (data1 >> 2) & 0x01, (data1 >> 3) & 0x01, (data1 >> 4) & 0x01, (data1 >> 5) & 0x01, (data1 >> 6) & 0x01, (data1 >> 7) & 0x01,
           data2 & 0x01, (data2 >> 1) & 0x01, (data2 >> 2) & 0x01, (data2 >> 3) & 0x01, (data2 >> 4) & 0x01, (data2 >> 5) & 0x01, (data2 >> 6) & 0x01, (data2 >> 7) & 0x01);

  // 装载并写入输出寄存器
  TCA6416A_data_buf[0] = TCA6416A_OUT1, TCA6416A_data_buf[1] = data1;
  ret = i2c_master_write_to_device(DEVICE_I2C_PORT, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }
  TCA6416A_data_buf[0] = TCA6416A_OUT2, TCA6416A_data_buf[1] = data2;
  ret = i2c_master_write_to_device(DEVICE_I2C_PORT, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }

  // 读取输入寄存器
  TCA6416A_data_buf[0] = TCA6416A_IN1;
  ret = i2c_master_write_read_device(DEVICE_I2C_PORT, i2c_add, &TCA6416A_data_buf[0], sizeof(TCA6416A_data_buf[0]), &data1, sizeof(data1), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }
  TCA6416A_data_buf[0] = TCA6416A_IN2;
  ret = i2c_master_write_read_device(DEVICE_I2C_PORT, i2c_add, &TCA6416A_data_buf[0], sizeof(TCA6416A_data_buf[0]), &data2, sizeof(data2), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }

  // 依据data1,data2数值移位映射
  for (i = 0; i < 16; i++)
  {
    p = (bool *)pTCA6416Alevel + i; // 强制转换地址的类型，指向（存储）成员变量地址
    if (i < 8)
      *p = (data1 >> i) & 0x01; // 不断取出位移后data1最低位
    if (i >= 8)
      *p = data2 >> (i - 8) & 0x01; // 不断取出位移后data1最低位
  }

  // 读取输出寄存器
  TCA6416A_data_buf[0] = TCA6416A_OUT1;
  ret = i2c_master_write_read_device(DEVICE_I2C_PORT, i2c_add, &TCA6416A_data_buf[0], sizeof(TCA6416A_data_buf[0]), &data1, sizeof(data1), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }
  TCA6416A_data_buf[0] = TCA6416A_OUT2;
  ret = i2c_master_write_read_device(DEVICE_I2C_PORT, i2c_add, &TCA6416A_data_buf[0], sizeof(TCA6416A_data_buf[0]), &data2, sizeof(data2), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }

  // 依据data1,data2数值移位映射
  for (i = 0; i < 16; i++)
  {
    // 强制转换地址的类型，指向（存储）成员变量地址
    p = (bool *)pTCA6416Alevel + i;
    m = (bool *)pTCA6416Amode + i;
    if (*m == 0) // 如果是输出引脚才进行映射
    {
      if (i < 8)
        *p = (data1 >> i) & 0x01; // 不断取出位移后data1最低位
      if (i >= 8)
        *p = data2 >> (i - 8) & 0x01; // 不断取出位移后data2最低位
    }
  }

  ESP_LOGW(TAG, "TCA6416A引脚电平读写完成,当前寄存器回读:\nP00-P07: [QC_TOUCH_L %d][EN_LED_BOARD %d][HP_DETECT %d][BAT_QSTRT %d][BAT_ALRT %d][EMF_DRDY %d][amplifier_MUTE %d][amplifier_SD %d]\nP10-P17: [IMU_INT2 %d][IMU_INT1 %d][ALS_INT %d][thumbwheel_CCW %d][thumbwheel_CW %d][OTG_EN %d][charge_SIGN %d][QC_TOUCH_R %d]",
           pTCA6416Alevel->QC_TOUCH_L, pTCA6416Alevel->EN_LED_BOARD, pTCA6416Alevel->HP_DETECT, pTCA6416Alevel->BAT_QSTRT, pTCA6416Alevel->BAT_ALRT, pTCA6416Alevel->EMF_DRDY, pTCA6416Alevel->amplifier_MUTE, pTCA6416Alevel->amplifier_SD,
           pTCA6416Alevel->IMU_INT2, pTCA6416Alevel->IMU_INT1, pTCA6416Alevel->ALS_INT, pTCA6416Alevel->thumbwheel_CCW, pTCA6416Alevel->thumbwheel_CW, pTCA6416Alevel->OTG_EN, pTCA6416Alevel->charge_SIGN, pTCA6416Alevel->QC_TOUCH_R);

  return ESP_OK;
}

/// @brief 设置全局引脚极性反转
/// @param inversion true=反转/false=正常
/// @return [ESP_OK 成功]
/// @return [ESP_FAIL 发送命令时发现问题, TCA6416A未应答]
/// @return [ESP_ERR_INVALID_STATE I2C driver 未安装或没有运行在主机模式]
/// @return [ESP_ERR_TIMEOUT 操作超时因为总线忙]
esp_err_t TCA6416A_gpio_global_inversion_set(bool inversion)
{
  const char *TAG = "TCA6416A_gpio_global_inversion_set";
  esp_err_t ret = ESP_OK;

  // 确定设备地址
  uint8_t i2c_add = 0x20;
  if (TCA6416A_ADDR_LEVEL)
    i2c_add = 0x21;

  uint8_t data1 = NULL, data2 = NULL; // 临时数据缓存

  // 装载并写入
  TCA6416A_data_buf[0] = TCA6416A_PI1;
  if (inversion)
  {
    data1 = 0xFF, data2 = 0xFF;
  }
  else
  {
    data1 = 0x00, data2 = 0x00;
  }

  TCA6416A_data_buf[0] = TCA6416A_PI1, TCA6416A_data_buf[1] = data1;
  ret = i2c_master_write_to_device(DEVICE_I2C_PORT, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }
  TCA6416A_data_buf[0] = TCA6416A_PI2, TCA6416A_data_buf[1] = data2;
  ret = i2c_master_write_to_device(DEVICE_I2C_PORT, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), 1000 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "与TCA6416A通讯时发现问题 描述： %s", esp_err_to_name(ret));
    return ret;
  }

  if (inversion)
    ESP_LOGW(TAG, "TCA6416A引脚极性设置完成 - 反转");
  else
    ESP_LOGW(TAG, "TCA6416A引脚极性设置完成 - 正常");

  return ESP_OK;
}
