
/*
 * 701Enti MIT License
 *
 * Copyright © 2024 <701Enti organization>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the “Software”),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// 包含各种SE30对温湿度传感器 AHT20的支持
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// AHT20的CRC校验计算支持,来自奥松电子官方的实例程序,非常感谢
// github: https://github.com/701Enti



#include "AHT20.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <stdint.h>
#include <string.h>

static i2c_master_dev_handle_t AHT20_dev_handle = NULL;

esp_err_t AHT20_begin();

/// @brief 初始化AHT20
/// @return [ESP_OK 成功] 
/// @return [ESP_ERR_INVALID_STATE]
/// 设备状态异常，请检查设备通讯地址是否正确，设备是否正常连接
/// @return [ESP_ERR_NOT_ALLOWED] 已初始化，不能重复初始化 /
/// 获取i2c总线句柄失败，总线未初始化
/// @return [ESP_ERR_NO_MEM 内存不足]
esp_err_t AHT20_init() {
  const static char *TAG = "AHT20_init";
  if (AHT20_dev_handle != NULL) {
    ESP_LOGE(TAG, "AHT20已初始化,不能重复初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_master_bus_handle_t shared_bus;
  esp_err_t ret = i2c_master_get_bus_handle(AHT20_I2C_PORT, &shared_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "获取i2c总线句柄失败,总线未初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = AHT20_DEVICE_ADD,
      .scl_speed_hz = AHT20_I2C_FREQ_HZ,
  };
  ret = i2c_master_bus_add_device(shared_bus, &dev_cfg, &AHT20_dev_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "添加i2c设备失败,描述：%s", esp_err_to_name(ret));
    return ret;
  }
  ret = AHT20_begin();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "尝试启动AHT20失败,描述：%s", esp_err_to_name(ret));
    return ret;
  } else {
    ESP_LOGI(TAG, "AHT20初始化成功");
  }
  return ret;
}

/// @brief  尝试启动AHT20
/// @return [ESP_OK] 传感器可以正常运行
/// @return [ESP_ERR_INVALID_STATE] 设备状态异常，请检查设备通讯地址是否正确，设备是否正常连接
/// @return [ESP_ERR_NOT_ALLOWED] 设备I2C句柄为空,未初始化,不能启动
/// @return [ESP_FAIL] 传感器启动失败
esp_err_t AHT20_begin() {
  const char *TAG = "AHT20_begin";

  if (AHT20_dev_handle == NULL) {
    ESP_LOGE(TAG, "设备I2C句柄为空,未初始化,不能启动");
    return ESP_ERR_NOT_ALLOWED;
  }

  if ((AHT20_get_status() & 0x08) >> 3) {
    ESP_LOGI(TAG, "温湿度传感器AHT20可以正常运行");
    return ESP_OK;
  } else {
    uint8_t buf[3] = {0xBE, 0x08, 0x00};
    esp_err_t ret = i2c_master_transmit(AHT20_dev_handle, buf, sizeof(buf),
                                        AHT20_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "与温湿度传感器AHT20通讯时发现问题 描述： %s",
               esp_err_to_name(ret));
      return ret;
    }
    
    if ((AHT20_get_status() & 0x08) >> 3) {
      ESP_LOGI(TAG, "温湿度传感器AHT20启动成功");
      return ESP_OK;
    } else {
      ESP_LOGE(TAG, "温湿度传感器AHT20启动失败");
      return ESP_FAIL;
    }
  }
}

/// @brief 获取运行状态字
/// @return 一个字节的状态字
uint8_t AHT20_get_status() {
  const char *TAG = "AHT20_get_status";
  uint8_t read_buf = 0x00; // 读取缓存
  uint8_t write_buf = 0x71;
  esp_err_t ret = i2c_master_transmit_receive(
      AHT20_dev_handle, &write_buf, sizeof(write_buf), &read_buf,
      sizeof(read_buf), AHT20_I2C_TIMEOUT_MS);
  if (ret != ESP_OK) {
    read_buf = 0x00;
    ESP_LOGE(TAG, "与温湿度传感器AHT20通讯时发现问题 描述： %s",
             esp_err_to_name(ret));
  }
  return read_buf;
}

/// @brief AHT20的CRC校验计算,来自奥松电子官方的实例程序
/// @param pDat 数据位置
/// @param Lenth 数据长度
/// @return CRC校验码
uint8_t CheckCrc8(uint8_t *pDat, uint8_t Lenth) {
  uint8_t crc = 0xff, i, j;

  if (!pDat) {
    ESP_LOGE("AHT20.c - CheckCrc8", "导入了为空的数据地址");
    return crc;
  }

  for (i = 0; i < Lenth; i++) {
    crc = crc ^ *pDat;
    for (j = 0; j < 8; j++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x31;
      else
        crc <<= 1;
    }
    pDat++;
  }
  return crc;
}

/// @brief 触发测量
void AHT20_trigger() {
  const char *TAG = "AHT20_trigger";
  uint8_t write_buf[3] = {0};
  write_buf[0] = 0xAC;
  write_buf[1] = 0x33;
  write_buf[2] = 0x00;
  esp_err_t ret = i2c_master_transmit(AHT20_dev_handle, write_buf,
                                      sizeof(write_buf), AHT20_I2C_TIMEOUT_MS);

  if (ret != ESP_OK)
    ESP_LOGE(TAG, "与温湿度传感器AHT20通讯时发现问题 描述： %s",
             esp_err_to_name(ret));
}

/// @brief 获取测量结果
/// @param dest 目标存储区域,同时会读取其中配置
void AHT20_get_result(AHT20_result_t *dest) {
  const char *TAG = "AHT20_get_result";

  if (!dest) {
    ESP_LOGE(TAG, "导入了为空的数据地址");
    return;
  }

  memset(dest, 0, sizeof(AHT20_result_t));

  if ((AHT20_get_status() & 0x80)) {
    ESP_LOGE(TAG, "温湿度传感器AHT20正在测量中,无法读取");
    return;
  } else {
    uint8_t read_buf[6] = {0}; // 读取缓存
    uint8_t write_buf = 0x71;

    esp_err_t ret = i2c_master_transmit_receive(
        AHT20_dev_handle, &write_buf, sizeof(write_buf), read_buf,
        sizeof(read_buf), AHT20_I2C_TIMEOUT_MS);

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "与温湿度传感器AHT20通讯时发现问题 描述： %s",
               esp_err_to_name(ret));
      return;
    } else {
      // 计算最终数据
      dest->humidity = ((double)((read_buf[1] << 12) | (read_buf[2] << 4) |
                                 ((read_buf[3] >> 4) & 0x0F)) /
                        (1048576)) *
                       100;
      dest->temperature = ((double)(((read_buf[3] & 0x0F) << 16) |
                                    (read_buf[4] << 8) | (read_buf[5])) /
                           (1048576) * 200) -
                          50;

      ESP_LOGI(TAG, "温度 %lf℃  湿度 %lf%%RH", dest->temperature,
               dest->humidity);
    }
  }
  return;
}
