/*
 * 701Enti MIT License
 *
 * Copyright © 2026 <701Enti organization>
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

// 对环境光传感器OPT3001的支持
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含I2C通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行

#include "OPT3001.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

static i2c_master_dev_handle_t OPT3001_dev_handle = NULL;

esp_err_t OPT3001_set_default_config();

/// @brief 初始化OPT3001
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_STATE]
/// 设备状态异常，请检查设备通讯地址是否正确，设备是否正常连接
/// @return [ESP_ERR_NOT_ALLOWED] 已初始化，不能重复初始化 /
/// 获取i2c总线句柄失败，总线未初始化
/// @return [ESP_ERR_NO_MEM 内存不足]
esp_err_t OPT3001_init() {
  const static char *TAG = "OPT3001_init";
  if (OPT3001_dev_handle != NULL) {
    ESP_LOGE(TAG, "OPT3001已初始化,不能重复初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_master_bus_handle_t shared_bus;
  esp_err_t ret = i2c_master_get_bus_handle(OPT3001_I2C_PORT, &shared_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "获取i2c总线句柄失败,总线未初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = OPT3001_DEVICE_ADD,
      .scl_speed_hz = OPT3001_I2C_FREQ_HZ,
  };
  ret = i2c_master_bus_add_device(shared_bus, &dev_cfg, &OPT3001_dev_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "添加i2c设备失败,描述:%s", esp_err_to_name(ret));
    return ret;
  } else {
    ESP_LOGI(TAG, "OPT3001初始化成功");
  }
  ret = OPT3001_set_default_config();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "尝试设置OPT3001默认配置失败,描述：%s", esp_err_to_name(ret));
    return ret;
  } else {
    ESP_LOGI(TAG, "OPT3001设置默认配置成功");
  }
  return ret;
}

/// @brief  尝试启动OPT3001设置默认配置
/// @return [ESP_OK] 传感器可以正常运行
/// @return [ESP_ERR_INVALID_STATE]
/// 设备状态异常，请检查设备通讯地址是否正确，设备是否正常连接
/// @return [ESP_ERR_NOT_ALLOWED] 设备I2C句柄为空,未初始化,不能启动
/// @return [ESP_FAIL] 传感器启动失败
esp_err_t OPT3001_set_default_config() {
  const static char *TAG = "OPT3001_set_default_config";

  if (OPT3001_dev_handle == NULL) {
    ESP_LOGE(TAG, "OPT3001未初始化,不能设置默认配置");
    return ESP_ERR_NOT_ALLOWED;
  }

  uint8_t write_buf[3] = {0x01, OPT3001_DEFAULT_CONFIG_REG_VALUE >> 8,
                          OPT3001_DEFAULT_CONFIG_REG_VALUE & 0xFF};

  esp_err_t ret = i2c_master_transmit(
      OPT3001_dev_handle, write_buf, sizeof(write_buf), OPT3001_I2C_TIMEOUT_MS);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "与环境光传感器OPT3001通讯时发现问题 描述： %s",
             esp_err_to_name(ret));
    return ret;
  }

  return ESP_OK;
}


/// @brief  读取lux数值
/// @return lux照度数值 / -1 读取失败
float OPT3001_fetch_lux()
{
    const static char *TAG = "OPT3001_fetch_lux";

    if (OPT3001_dev_handle == NULL) {
        ESP_LOGE(TAG, "OPT3001未初始化,不能读取lux数值");
        return -1.0f;
    }

    uint8_t reg_addr = 0x00;
    uint8_t rx_buf[2];

    // 先发送寄存器地址，再读取2字节
    esp_err_t ret = i2c_master_transmit_receive(OPT3001_dev_handle, &reg_addr, sizeof(reg_addr), rx_buf, sizeof(rx_buf), OPT3001_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "与环境光传感器OPT3001通讯时发现问题 描述： %s",
                 esp_err_to_name(ret));
        return -1.0f;
    }

    uint16_t res_reg = ((uint16_t)rx_buf[0] << 8) | rx_buf[1];

    uint8_t  exp  = (res_reg >> 12) & 0x0F;
    uint16_t mant = res_reg & 0x0FFF;

    float lux = 0.01f * (1U << exp) * mant;

    ESP_LOGI(TAG, "lux: %f", lux);

    return lux;
}