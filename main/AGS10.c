
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

// 包含各种SE30对空气质量传感器AGS10 的支持
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// AGS10推荐I2C最高通讯频率为15kHz,而大部分器件目前使用100kHz,所以在库函数内提供临时频率变更支持,目前已经在AGS10_TVOC_result_get()应用,之后库中追加的API也应该调用频率变更支持
// github: https://github.com/701Enti

#include "AGS10.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

static i2c_master_dev_handle_t AGS10_dev_handle = NULL;

/// @brief 初始化AGS10
/// @return [ESP_OK 成功] 
/// @return [ESP_ERR_INVALID_STATE]
/// 设备状态异常，请检查设备通讯地址是否正确，设备是否正常连接
/// @return [ESP_ERR_NOT_ALLOWED] 已初始化，不能重复初始化 /
/// 获取i2c总线句柄失败，总线未初始化
/// @return [ESP_ERR_NO_MEM 内存不足]
esp_err_t AGS10_init() {
  const static char *TAG = "AGS10_init";
  if (AGS10_dev_handle != NULL) {
    ESP_LOGE(TAG, "AGS10已初始化,不能重复初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_master_bus_handle_t shared_bus;
  esp_err_t ret = i2c_master_get_bus_handle(AGS10_I2C_PORT, &shared_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "获取i2c总线句柄失败,总线未初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = AGS10_DEVICE_ADD,
      .scl_speed_hz = AGS10_I2C_FREQ_HZ,
  };
  ret = i2c_master_bus_add_device(shared_bus, &dev_cfg, &AGS10_dev_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "添加i2c设备失败,描述:%s", esp_err_to_name(ret));
    return ret;
  } else {
    ESP_LOGI(TAG, "AGS10初始化成功");
  }
  return ret;
}

/// @brief
/// AGS10的CRC校验计算,来自奥松电子官方的AGS10数据手册内(需登录后点击"文件下载"获取手册)
/// https://www.aosong.com/Products/info.aspx?lcid=&proid=49
/// @param dat 数据位置
/// @param Num 数据字节个数
/// @return CRC校验码
uint8_t Calc_CRC8(uint8_t *dat, uint8_t Num) {
  uint8_t i, byte, crc = 0xFF;

  if (!dat) {
    ESP_LOGE("AGS10.c - Calc_CRC8", "导入了为空的数据地址");
    return crc;
  }

  for (byte = 0; byte < Num; byte++) {
    crc ^= (dat[byte]);
    for (i = 0; i < 8; i++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x31;
      else
        crc = (crc << 1);
    }
  }
  return crc;
}

void AGS10_TVOC_result_get(AGS10_result_t *dest) {
  const char *TAG = "AGS10_TVOC_result_get";

  if (!dest) {
    ESP_LOGE(TAG, "导入了为空的数据地址");
    return;
  } else {
    bool crc_flag = dest->flag_crc;
    memset(dest, 0, sizeof(AGS10_result_t));
    dest->flag_crc = crc_flag;
  }

  uint8_t write_buf = 0x00;  // 设置到数据采集寄存器0x00
  uint8_t read_buf[5] = {0}; // 读取缓存 [0]为状态数据 [1][2][3]为TVOC数据
                             // [4]为CRC校验值(选择性读取)

  esp_err_t err = ESP_OK;

  if (dest->flag_crc) {
    err = i2c_master_transmit_receive(AGS10_dev_handle, &write_buf,
                                      sizeof(write_buf), read_buf,
                                      sizeof(read_buf), AGS10_I2C_TIMEOUT_MS);
  } else {
    err = i2c_master_transmit_receive(
        AGS10_dev_handle, &write_buf, sizeof(write_buf), read_buf,
        sizeof(read_buf) - 1, AGS10_I2C_TIMEOUT_MS);
  }

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "与空气质量传感器AGS10通讯时发现问题 描述： %s",
             esp_err_to_name(err));
    return;
  } else {
    // 传感器状态检查
    if (read_buf[0] & 0x01) {
      ESP_LOGW(TAG, "数据未更新或传感器正在预热,无法读取");
      return;
    } else {
      dest->TVOC_value = (read_buf[1] << 16) | (read_buf[2] << 8) | read_buf[3];
    }

    // CRC校验
    if (dest->flag_crc) {
      if (Calc_CRC8(&read_buf[0], 4) != read_buf[4]) {
        ESP_LOGE(TAG, "数据CRC校验后发现问题");
        return;
      }
    }

    ESP_LOGI(TAG, "%d ppb", (int)dest->TVOC_value);

    // 数据有效标记
    dest->valid = true;
    return;
  }
}
