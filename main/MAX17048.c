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

// 对电池电量计量传感器MAX17048的支持
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含I2C通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行

#include "MAX17048.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

static i2c_master_dev_handle_t MAX17048_dev_handle = NULL;

/// @brief 初始化MAX17048
/// @return [ESP_OK 成功]
/// @return [ESP_ERR_INVALID_STATE]
/// 设备状态异常，请检查设备通讯地址是否正确，设备是否正常连接
/// @return [ESP_ERR_NOT_ALLOWED] 已初始化，不能重复初始化 /
/// 获取i2c总线句柄失败，总线未初始化
/// @return [ESP_ERR_NO_MEM 内存不足]
esp_err_t MAX17048_init() {
  const static char *TAG = "MAX17048_init";
  if (MAX17048_dev_handle != NULL) {
    ESP_LOGE(TAG, "MAX17048已初始化,不能重复初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_master_bus_handle_t shared_bus;
  esp_err_t ret = i2c_master_get_bus_handle(MAX17048_I2C_PORT, &shared_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "获取i2c总线句柄失败,总线未初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = MAX17048_DEVICE_ADD,
      .scl_speed_hz = MAX17048_I2C_FREQ_HZ,
  };
  ret = i2c_master_bus_add_device(shared_bus, &dev_cfg, &MAX17048_dev_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "添加i2c设备失败,描述:%s", esp_err_to_name(ret));
    return ret;
  } else {
    ESP_LOGI(TAG, "MAX17048初始化成功");
  }
  return ret;
}

void MAX17048_result_fetch(MAX17048_result_t *dest) {
  const static char *TAG = "MAX17048_result_fetch";

  if (dest == NULL) {
    ESP_LOGE(TAG, "dest为NULL");
    return;
  }

  dest->battery_voltage = -1.0f;
  dest->battery_soc = -1.0f;

  if (MAX17048_dev_handle == NULL) {
    ESP_LOGE(TAG, "MAX17048未初始化,不能读取电池测量结果");
    return;
  }

  uint8_t voltage_reg_addr = 0x02;
  uint8_t soc_reg_addr = 0x04;
  uint8_t rx_buf[2] = {0};
  esp_err_t ret;

  ret = i2c_master_transmit_receive(MAX17048_dev_handle, &voltage_reg_addr,
                                    sizeof(voltage_reg_addr), rx_buf,
                                    sizeof(rx_buf), MAX17048_I2C_TIMEOUT_MS);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "读取电池电压寄存器失败,描述:%s", esp_err_to_name(ret));
    return;
  }
  else{
    dest->battery_voltage = (rx_buf[0] << 8 | rx_buf[1]) * 0.078125f;
    ESP_LOGI(TAG, "电池电压:%.2fmV", dest->battery_voltage);
  }

  ret = i2c_master_transmit_receive(MAX17048_dev_handle, &soc_reg_addr,
                                    sizeof(soc_reg_addr), rx_buf,
                                    sizeof(rx_buf), MAX17048_I2C_TIMEOUT_MS);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "读取电池SOC寄存器失败,描述:%s", esp_err_to_name(ret));
    return;
  }
  else{
    dest->battery_soc = (rx_buf[0] << 8 | rx_buf[1]) / 256.0f;
    ESP_LOGI(TAG, "电池SOC:%.2f%%", dest->battery_soc);
  }
}
