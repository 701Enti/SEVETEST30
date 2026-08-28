
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

// 包含各种SE30针对性硬件控制
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件包含 DEVICE_I2C_PORT
// i2c通讯的初始化配置,需要调用device_i2c_init(),此处音频和其他设备共用端口，在audio_board_init()初始化，不需初始化
//     API规范: sevetest30_board_ctrl
//     外部函数应该调用board_status_get获取控制缓存变量,修改值后导入
///----注意：控制数据只有在完成sevetest30_board_ctrl工作之后，才会保存到board_ctrl_buf缓存中，如果果您只是外部定义了一个board_ctrl_t类型变量存储您的更改，
//     但是没有调用sevetest30_board_ctrl,board_ctrl_buf缓存数据将不会更新,而系统缓存的位置是board_ctrl_buf，而不是您自己定义的外部缓存，
///    意味着系统比如蓝牙读取，读到的数据将不是更新的数据，所以如果您要进行控制数据更改，务必保证sevetest30_board_ctrl工作进行了
///    如果只是单纯希望修改控制数据可以调用board_status_get直接获取系统缓存结构体指针进行修改，这样其他API读到数据将是更新的数据,但是只有sevetest30_board_ctrl被调用，硬件才会与控制数据同步
// github: https://github.com/701Enti


#include "board_ctrl.h"
#include "AGS10.h"
#include "AHT20.h"
#include "board.h"
#include "gt32l32s0140.h"
#include "OPT3001.h"
#include "MAX17048.h"

#include "HSCDTD008A.h"
#include "LSM6DS3TRC.h"
#include "driver/i2c_master.h"
#include "es8388.h"
#include "esp_log.h"
#include "sevetest30_BWEDA.h"
#include "sevetest30_LedArray.h"
#include "sevetest30_gpio.h"
#include "sevetest30_touch.h"

static i2c_master_dev_handle_t amplifier_dev_handle = NULL;

esp_err_t board_ctrl_init_report[INIT_STEP_NUMBER] = {ESP_FAIL};

esp_periph_set_handle_t se30_periph_set_handle;

///----注意：控制数据只有在完成sevetest30_board_ctrl工作之后，才会保存到board_ctrl_buf缓存中，如果果您只是外部定义了一个board_ctrl_t类型变量存储您的更改，
//     但是没有调用sevetest30_board_ctrl,board_ctrl_buf缓存数据将不会更新,而系统缓存的位置是board_ctrl_buf，而不是您自己定义的外部缓存，
///    意味着系统比如蓝牙读取，读到的数据将不是更新的数据，所以如果您要进行控制数据更改，务必保证sevetest30_board_ctrl工作进行了
///    如果只是单纯希望修改控制数据可以调用board_status_get直接获取系统缓存结构体指针进行修改，这样其他API读到数据将是更新的数据,但是只有sevetest30_board_ctrl被调用，硬件才会与控制数据同步
board_ctrl_t *board_ctrl_buf = NULL;

esp_err_t amplifier_vol_dp_init();
esp_err_t amplifier_vol_dp_set(board_ctrl_t *board_ctrl);

void codechip_mode_and_status_set(board_ctrl_t *board_ctrl);
void codechip_volume_set(board_ctrl_t *board_ctrl);
void board_ctrl_buf_map(board_ctrl_t *board_ctrl,
                        board_ctrl_select_t ctrl_select);
void codec_set_mic_gain(board_ctrl_t *board_ctrl);
void codec_config_adc_input(board_ctrl_t *board_ctrl);
void codec_config_dac_output(board_ctrl_t *board_ctrl);

esp_err_t get_spi_pins_font_chip(
    spi_bus_config_t *spi_config,
    spi_device_interface_config_t *spi_device_interface_config);

esp_err_t get_spi_pins_ledarray(
    spi_bus_config_t *spi_config,
    spi_device_interface_config_t *spi_device_interface_config);

// API规范: sevetest30_board_ctrl :
// 外部函数应该调用board_status_get获取控制缓存变量,修改值后导入
/// @brief
/// 获取系统控制缓存结构体指针,应该使用获取的结构体指针，修改参数并以此调用sevetest30_board_ctrl()
/// @return 控制缓存结构体指针

/// @brief
/// 获取系统控制缓存结构体指针,应该使用获取的结构体指针，修改参数并以此调用sevetest30_board_ctrl()
/// @return 控制缓存结构体指针
board_ctrl_t *board_status_get() { return board_ctrl_buf; }

// /// @brief 初始化设备I2C总线
// /// @return ESP_OK: 成功
// /// @return ESP_ERR_NO_MEM: 内存不足
// /// @return ESP_ERR_NOT_FOUND: 未找到空闲总线
// esp_err_t device_i2c_init() {
//   const static char *TAG = "device_i2c_init";

//   i2c_master_bus_config_t bus_cfg = {
//       .i2c_port = DEVICE_I2C_PORT,
//       .sda_io_num = DEVICE_I2C_SDA_IO,
//       .scl_io_num = DEVICE_I2C_SCL_IO,
//       .clk_source = DEVICE_I2C_CLK_SRC,
//   };

//   i2c_master_bus_handle_t bus_handle;
//   esp_err_t ret = i2c_new_master_bus(&bus_cfg, &bus_handle);
//   if (ret != ESP_OK) {
//     ESP_LOGE(TAG, "I2C总线创建失败");
//     return ret;
//   }

//   ESP_LOGI(TAG, "初始化I2C总线完成");
//   return ESP_OK;
// }

/// @brief 全局设备初始化
/// @param board_ctrl 定义board_ctrl_t非指针类型全局变量，进行所有值设置后导入
/// @param board_device_handle
/// 设备句柄，定义board_device_handle_t非指针类型全局变量不进行任何更改，在进行一些活动时将使用其中句柄
/// @return 初始化操作的所有esp_err_t返回记录
esp_err_t *sevetest30_all_device_init(board_ctrl_t *board_ctrl) {
  // // 初始化设备I2C总线(SEVETEST30的设备与音频共用I2C总线，所以无需执行)
  // device_i2c_init();

  // 初始化音频面板(包括了I2C的初始化和注册)
  audio_board_init();

  // LED阵列
  board_ctrl_init_report[LEDARRAY_INIT] = ledarray_init();

  // 字库芯片
  board_ctrl_init_report[FONTS_CHIP_INIT] = fonts_chip_init();

  // 初始化TCA6416A扩展IO芯片
  board_ctrl_init_report[TCA6416A_INIT] = TCA6416A_init();

  // 初始化GPIO服务(包括扩展GPIO)
  board_ctrl_init_report[SEVETEST30_GPIO_INIT] = sevetest30_gpio_init(
      board_ctrl->p_ext_io_mode, board_ctrl->p_ext_io_value);

  // 初始化音频功放音量数字电位器
  board_ctrl_init_report[AMP_VOL_DP_INIT] = amplifier_vol_dp_init();

  // AHT20(温湿度传感器)
  board_ctrl_init_report[AHT20_INIT] = AHT20_init();

  // AGS10(空气质量传感器)
  board_ctrl_init_report[AGS10_INIT] = AGS10_init();

  // OPT3001(环境光传感器)
  board_ctrl_init_report[OPT3001_INIT] = OPT3001_init();

  // MAX17048(电池状态传感器)
  board_ctrl_init_report[MAX17048_INIT] = MAX17048_init();

  // LSM6DS3TRC(姿态传感器)
  board_ctrl_init_report[LSM6DS3TRC_INIT] = LSM6DS3TRC_init();

  // HSCDTD008A(地磁场传感器)
  board_ctrl_init_report[HSCDTD008A_INIT] = HSCDTD008A_init();

  // 震动马达
  board_ctrl_init_report[VIBRA_MOTOR_INIT] =
      vibra_motor_init(VIBRA_IN1_IO, VIBRA_IN2_IO);

  // 全部初始化之后配置所有设备到指定模式
  board_ctrl_init_report[BOARD_CTRL_CONFIG] =
      sevetest30_board_ctrl(board_ctrl, BOARD_CTRL_ALL);

  return board_ctrl_init_report;
}

/// @brief
/// 全局设备控制,外部函数应该调用board_status_get获取控制缓存变量,修改值后导入
/// @param board_ctrl
/// 调用board_status_get()获取系统控制缓存结构体，进行需要修改后放入
/// @param ctrl_select 选择控制的对象，这是一个枚举类型

/// @brief
/// 全局设备控制,外部函数应该调用board_status_get获取控制缓存变量,修改值后导入
/// @param board_ctrl
/// 调用board_status_get()获取系统控制缓存结构体，进行需要修改后放入
/// @param ctrl_select 选择控制的对象，这是一个枚举类型
/// @return ESP_OK: 成功
/// @return 其他： 失败
esp_err_t sevetest30_board_ctrl(board_ctrl_t *board_ctrl,
                                board_ctrl_select_t ctrl_select) {
  if (!board_ctrl) {
    ESP_LOGE("sevetest30_board_ctrl", "无法处理的空指针");
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = ESP_OK;

  switch (ctrl_select) {
  case BOARD_CTRL_ALL:
    ret |= amplifier_vol_dp_set(board_ctrl);
    codechip_mode_and_status_set(board_ctrl);
    codec_config_dac_output(board_ctrl);
    codechip_volume_set(board_ctrl);
    codec_config_adc_input(board_ctrl);
    codec_set_mic_gain(board_ctrl);
    ret |= ext_io_mode_service();
    ret |= ext_io_level_service();
    break;

  case BOARD_CTRL_AMPLIFIER:
    ret |= amplifier_vol_dp_set(board_ctrl);
    break;

  case BOARD_CTRL_CODEC_MODE_AND_STATUS:
    codechip_mode_and_status_set(board_ctrl);
    break;

  case BOARD_CTRL_CODEC_DAC_PIN:
    codec_config_dac_output(board_ctrl);
    break;

  case BOARD_CTRL_CODEC_DAC_VOL:
    codechip_volume_set(board_ctrl);
    break;

  case BOARD_CTRL_CODEC_ADC_PIN:
    codec_config_adc_input(board_ctrl);
    break;

  case BOARD_CTRL_CODEC_ADC_GAIN:
    codec_set_mic_gain(board_ctrl);
    break;

  case BOARD_CTRL_EXT_IO:
    ret |= ext_io_mode_service();
    ret |= ext_io_level_service();
    break;

  default:
    ret |= ext_io_mode_service();
    ret |= ext_io_level_service();
    break;
  }

  // 添加控制操作需要同时完善board_ctrl_buf_map的缓存映射
  board_ctrl_buf_map(board_ctrl, ctrl_select);
  sevetest30_ble_attr_value_push(); // 向蓝牙客户端推送更改

  return ret;
}

// 设备控制参数缓存映射
void board_ctrl_buf_map(board_ctrl_t *board_ctrl,
                        board_ctrl_select_t ctrl_select) {
  if (!board_ctrl_buf) {
    board_ctrl_buf = malloc(sizeof(board_ctrl_t));
    while (!board_ctrl_buf) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP_LOGE("board_ctrl_buf_map", "申请board_ctrl_buf资源发现问题 正在重试");
      board_ctrl_buf = malloc(sizeof(board_ctrl_t));
    }
    memset(board_ctrl_buf, 0, sizeof(board_ctrl_t));
  }
  switch (ctrl_select) {
  case BOARD_CTRL_ALL:
    board_ctrl_buf->p_ext_io_mode = board_ctrl->p_ext_io_mode;
    board_ctrl_buf->p_ext_io_value = board_ctrl->p_ext_io_value;
    board_ctrl_buf->amplifier_volume = board_ctrl->amplifier_volume;
    board_ctrl_buf->amplifier_mute = board_ctrl->amplifier_mute;
    board_ctrl_buf->amplifier_sd = board_ctrl->amplifier_sd;
    board_ctrl_buf->codec_mode = board_ctrl->codec_mode;
    board_ctrl_buf->codec_audio_hal_ctrl = board_ctrl->codec_audio_hal_ctrl;
    board_ctrl_buf->codec_dac_pin = board_ctrl->codec_dac_pin;
    board_ctrl_buf->codec_dac_volume = board_ctrl->codec_dac_volume;
    board_ctrl_buf->codec_adc_pin = board_ctrl->codec_adc_pin;
    board_ctrl_buf->codec_adc_gain = board_ctrl->codec_adc_gain;
    break;

  case BOARD_CTRL_AMPLIFIER:
    board_ctrl_buf->amplifier_volume = board_ctrl->amplifier_volume;
    board_ctrl_buf->amplifier_mute = board_ctrl->amplifier_mute;
    board_ctrl_buf->amplifier_sd = board_ctrl->amplifier_sd;
    break;

  case BOARD_CTRL_CODEC_MODE_AND_STATUS:
    board_ctrl_buf->codec_mode = board_ctrl->codec_mode;
    board_ctrl_buf->codec_audio_hal_ctrl = board_ctrl->codec_audio_hal_ctrl;
    break;

  case BOARD_CTRL_CODEC_DAC_PIN:
    board_ctrl_buf->codec_dac_pin = board_ctrl->codec_dac_pin;
    break;

  case BOARD_CTRL_CODEC_DAC_VOL:
    board_ctrl_buf->codec_dac_volume = board_ctrl->codec_dac_volume;
    break;

  case BOARD_CTRL_CODEC_ADC_PIN:
    board_ctrl_buf->codec_adc_pin = board_ctrl->codec_adc_pin;
    break;

  case BOARD_CTRL_CODEC_ADC_GAIN:
    board_ctrl_buf->codec_adc_gain = board_ctrl->codec_adc_gain;
    break;

  case BOARD_CTRL_EXT_IO:
    board_ctrl_buf->p_ext_io_mode = board_ctrl->p_ext_io_mode;
    board_ctrl_buf->p_ext_io_value = board_ctrl->p_ext_io_value;
    break;

  default:
    board_ctrl_buf->p_ext_io_mode = board_ctrl->p_ext_io_mode;
    board_ctrl_buf->p_ext_io_value = board_ctrl->p_ext_io_value;
    break;
  }
}

// 调整音频编解码器芯片参数
void codechip_mode_and_status_set(board_ctrl_t *board_ctrl) {
  audio_board_handle_t board_handle = audio_board_get_handle();
  if (board_handle != NULL) {
    audio_hal_ctrl_codec(board_handle->audio_hal, board_ctrl->codec_mode,
                         board_ctrl->codec_audio_hal_ctrl);
  } else {
    ESP_LOGE("codechip_mode_and_status_set", "获取board_handle句柄时发现问题");
  }
}

void codechip_volume_set(board_ctrl_t *board_ctrl) {
  audio_board_handle_t board_handle = audio_board_get_handle();
  if (board_handle != NULL) {
    audio_hal_set_volume(board_handle->audio_hal, board_ctrl->codec_dac_volume);
  } else {
    ESP_LOGE("codechip_volume_set", "获取board_handle句柄时发现问题");
  }
}

esp_err_t get_spi_pins_font_chip(
    spi_bus_config_t *spi_config,
    spi_device_interface_config_t *spi_device_interface_config) {
  // 获取为字库芯片提供的SPI通讯IO定义
  if (spi_device_interface_config == NULL)
    return ESP_FAIL;
  spi_device_interface_config->spics_io_num = FONT_CHIP_SPI_CS_IO;

  if (spi_config == NULL)
    return ESP_FAIL;
  spi_config->mosi_io_num = FONT_CHIP_SPI_MOSI_IO;
  spi_config->miso_io_num = FONT_CHIP_SPI_MISO_IO;
  spi_config->sclk_io_num = FONT_CHIP_SPI_SCLK_IO;

  return ESP_OK;
}

esp_err_t get_spi_pins_ledarray(
    spi_bus_config_t *spi_config,
    spi_device_interface_config_t *spi_device_interface_config) {
  // 获取为LED阵列提供的SPI通讯IO定义
  if (spi_device_interface_config == NULL)
    return ESP_FAIL;
  spi_device_interface_config->spics_io_num = -1;

  if (spi_config == NULL)
    return ESP_FAIL;
  spi_config->mosi_io_num = LEDARRAY_SPI_MOSI_IO;
  spi_config->miso_io_num = -1;
  spi_config->sclk_io_num = LEDARRAY_SPI_SCLK_IO;

  return ESP_OK;
}

/// @brief 初始化音频功放音量数字电位器
/// @return - ESP_OK: 成功
/// @return - ESP_ERR_INVALID_STATE:
/// 设备状态异常，请检查设备通讯地址是否正确，设备是否正常连接
/// @return - ESP_ERR_NOT_ALLOWED: 已初始化，不能重复初始化 /
/// 获取i2c总线句柄失败，总线未初始化
/// @return - ESP_ERR_NO_MEM: 内存不足
esp_err_t amplifier_vol_dp_init() {
  const static char *TAG = "amplifier_vol_dp_init";
  if (amplifier_dev_handle != NULL) {
    ESP_LOGE(TAG, "音频功放音量数字电位器已初始化,不能重复初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_master_bus_handle_t shared_bus;
  esp_err_t ret = i2c_master_get_bus_handle(DEVICE_I2C_PORT, &shared_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "获取i2c总线句柄失败,总线未初始化");
    return ESP_ERR_NOT_ALLOWED;
  }
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = AMP_VOL_DP_ADD,
      .scl_speed_hz = AMP_VOL_DP_FREQ_HZ,
  };
  ret = i2c_master_bus_add_device(shared_bus, &dev_cfg, &amplifier_dev_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "添加i2c设备失败,描述:%s", esp_err_to_name(ret));
    return ret;
  }
  else {
    ESP_LOGI(TAG, "音频功放音量数字电位器初始化成功");
  }
  return ret;
}

// 音频功放音量数字电位器设置，音量 取值为 0
// -（board_def.h中常量AMP_VOL_MAX的值，原程序中为24）, 等于
// 0时将使得功放进入静音状态

/// @brief 设置音频功放音量数字电位器
/// @param board_ctrl: 需要的配置
/// @return - ESP_OK 成功
/// @return - ESP_ERR_INVALID_STATE  设备未初始化 /
/// 获取i2c总线句柄失败,总线未初始化
/// @return - ESP_FAIL 设备未响应 / sevetest30_board_ctrl异常
/// @return - ESP_ERR_TIMEOUT 设备响应超时
/// @return - ESP_ERR_INVALID_PARAM 输入了一个超过范围的音量值
esp_err_t amplifier_vol_dp_set(board_ctrl_t *board_ctrl) {
  const static char *TAG = "amplifier_vol_dp_set";

  if (amplifier_dev_handle == NULL) {
    ESP_LOGE(TAG, "音频功放音量数字电位器未初始化,不能设置音量");
    return ESP_ERR_INVALID_STATE;
  }
  if (board_ctrl->amplifier_volume > AMP_VOL_MAX) {
    ESP_LOGE(TAG, "输入了一个超过范围的音量值 - %d",
             board_ctrl->amplifier_volume);
    board_ctrl->amplifier_volume = AMP_VOL_MAX;
  }

  uint8_t buf[2] = {AMP_VOL_DP_COMMAND, 0}; // 设置值越高，实际音量越低

  if (board_ctrl->amplifier_sd == true) {

    if (board_ctrl->amplifier_mute == true ||
        board_ctrl->amplifier_volume == 0) {
      board_ctrl->p_ext_io_value->amplifier_SD = 1;
      board_ctrl->p_ext_io_value->amplifier_MUTE = 1;
      buf[1] = (AMP_VOL_MAX - 0) * AMP_STEP_VOL;
    } else {
      board_ctrl->p_ext_io_value->amplifier_SD = 1;
      board_ctrl->p_ext_io_value->amplifier_MUTE = 0;
      buf[1] = (AMP_VOL_MAX - board_ctrl->amplifier_volume) * AMP_STEP_VOL;
    }

    esp_err_t err = ESP_OK;
    err = i2c_master_transmit(amplifier_dev_handle, buf, sizeof(buf),
                              AMP_VOL_DP_TIMEOUT_MS);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "音频功放音量数字电位器配置时发现问题");
      return err;
    } else {
      if (sevetest30_board_ctrl(board_ctrl, BOARD_CTRL_EXT_IO) != ESP_OK) {
        ESP_LOGE(TAG, "sevetest30_board_ctrl异常");
        return ESP_FAIL;
      }
      ESP_LOGI(TAG,
               "音频功放音量数字电位器配置: [功放音量 %d] [静音 %d] [使能 %d]",
               board_ctrl->amplifier_volume, board_ctrl->amplifier_mute,
               board_ctrl->amplifier_sd);
    }

  } else {
    board_ctrl->p_ext_io_value->amplifier_SD = 0;
    if (sevetest30_board_ctrl(board_ctrl, BOARD_CTRL_EXT_IO) != ESP_OK) {
      ESP_LOGE(TAG, "sevetest30_board_ctrl异常");
      return ESP_FAIL;
    }
    ESP_LOGI(TAG,
             "音频功放音量数字电位器配置: [功放音量 %d] [静音 %d] [使能 %d]",
             board_ctrl->amplifier_volume, board_ctrl->amplifier_mute,
             board_ctrl->amplifier_sd);
  }
  return ESP_OK;
}

void codec_set_mic_gain(board_ctrl_t *board_ctrl) {
  if (es8388_set_mic_gain(board_ctrl->codec_adc_gain) != ESP_OK)
    ESP_LOGE("board", "设置麦克风增益发现问题");
}

void codec_config_adc_input(board_ctrl_t *board_ctrl) {
  if (es8388_config_adc_input(board_ctrl->codec_adc_pin) != ESP_OK)
    ESP_LOGE("board", "设置麦克风端口发现问题");
}

void codec_config_dac_output(board_ctrl_t *board_ctrl) {
  if (es8388_config_dac_output(board_ctrl->codec_dac_pin) != ESP_OK)
    ESP_LOGE("board", "设置音频输出端口发现问题");
}
