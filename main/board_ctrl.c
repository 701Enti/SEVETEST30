
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

// 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，，包含各种SE30针对性硬件控制
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件包含 DEVICE_I2C_PORT i2c通讯的初始化配置,需要调用device_i2c_init(),此处音频和其他设备共用端口，在audio_board_init()初始化，不需初始化
///----注意：控制数据只有在完成sevetest30_board_ctrl工作之后，才会保存到board_ctrl_buf缓存中，如果果您只是外部定义了一个board_ctrl_t类型变量存储您的更改，
//     但是没有调用sevetest30_board_ctrl,board_ctrl_buf缓存数据将不会更新,而系统缓存的位置是board_ctrl_buf，而不是您自己定义的外部缓存，
///    意味着系统比如蓝牙读取，读到的数据将不是更新的数据，所以如果您要进行控制数据更改，务必保证sevetest30_board_ctrl工作进行了
///    如果只是单纯希望修改控制数据可以调用board_status_get直接获取系统缓存结构体指针进行修改，这样其他API读到数据将是更新的数据,但是只有sevetest30_board_ctrl被调用，硬件才会与控制数据同步
// github: https://github.com/701Enti
// bilibili: 701Enti

#include "board_ctrl.h"
#include "board.h"
#include "fonts_chip.h"
#include "lsm6ds3trc.h"
#include "driver/i2c.h"

#include "esp_log.h"
#include "sevetest30_touch.h"
#include "sevetest30_BWEDA.h"

esp_periph_set_handle_t se30_periph_set_handle;

///----注意：控制数据只有在完成sevetest30_board_ctrl工作之后，才会保存到board_ctrl_buf缓存中，如果果您只是外部定义了一个board_ctrl_t类型变量存储您的更改，
//     但是没有调用sevetest30_board_ctrl,board_ctrl_buf缓存数据将不会更新,而系统缓存的位置是board_ctrl_buf，而不是您自己定义的外部缓存，
///    意味着系统比如蓝牙读取，读到的数据将不是更新的数据，所以如果您要进行控制数据更改，务必保证sevetest30_board_ctrl工作进行了
///    如果只是单纯希望修改控制数据可以调用board_status_get直接获取系统缓存结构体指针进行修改，这样其他API读到数据将是更新的数据,但是只有sevetest30_board_ctrl被调用，硬件才会与控制数据同步
board_ctrl_t *board_ctrl_buf = NULL;

void amplifier_set(board_ctrl_t *board_ctrl);
void boost_voltage_set(board_ctrl_t *board_ctrl);
void codechip_mode_and_status_set(board_ctrl_t *board_ctrl);
void codechip_volume_set(board_ctrl_t *board_ctrl);
void board_ctrl_buf_map(board_ctrl_t *board_ctrl, board_ctrl_select_t ctrl_select);

/// @brief 获取系统控制缓存结构体指针,应该使用获取的结构体指针，修改参数并以此调用sevetest30_board_ctrl()
/// @return 控制缓存结构体指针
board_ctrl_t *board_status_get()
{
    return board_ctrl_buf;
}


//此处音频和其他设备共用端口，在audio_board_init()初始化，不需初始化
//初始化设备I2C接口
esp_err_t device_i2c_init()
{
    i2c_config_t port_cfg;
    port_cfg.mode = I2C_MODE_MASTER;
    port_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    port_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    port_cfg.master.clk_speed = DEVICE_I2C_FREQ_HZ;

    get_i2c_pins(DEVICE_I2C_PORT,&port_cfg);

    i2c_param_config(DEVICE_I2C_PORT, &port_cfg);
    return i2c_driver_install(DEVICE_I2C_PORT, port_cfg.mode, 0, 0, false);
}

/// @brief 全局设备初始化
/// @param board_ctrl 定义board_ctrl_t非指针类型全局变量，进行所有值设置后导入
/// @param board_device_handle 设备句柄，定义board_device_handle_t非指针类型全局变量不进行任何更改，在进行一些活动时将使用其中句柄
/// @return 成功 ESP_OK 失败 ESP_FAIL
esp_err_t sevetest30_all_board_init(board_ctrl_t *board_ctrl, board_device_handle_t *board_device_handle)
{
    // 初始化设备
    audio_board_init();
    sevetest30_gpio_init(board_ctrl->p_ext_io_mode, board_ctrl->p_ext_io_value);
    fonts_chip_init(board_device_handle);
    BL5372_config_init();
    lsm6ds3trc_init_or_reset(); 
    vibra_motor_init(get_vibra_motor_IN1_gpio(), get_vibra_motor_IN2_gpio());

    // 配置
    sevetest30_board_ctrl(board_ctrl, BOARD_CTRL_ALL);

    // while (1)
    // {
    //     // vTaskDelay(pdMS_TO_TICKS(500));
    //     // IMU_acceleration_value_t value = lsm6ds3trc_gat_now_acceleration();
    //     // IMU_angular_rate_value_t value = lsm6ds3trc_gat_now_angular_rate();
    //     // if(value.x != 0 ||value.y != 0 || value.z !=0)
    //     // ESP_LOGE("ME", "%d %d %d",value.x,value.y,value.z);

    //     // IMU_D6D_data_value_t value = lsm6ds3trc_get_D6D_data_value();
    //     // ESP_LOGE("ME", "%d %d %d %d %d %d",value.XL,value.XH,value.YL,value.YH,value.ZL,value.ZH);

        

    //     // if(lsm6ds3trc_get_free_fall_status())ESP_LOGE("ME", "自由落体");
    // } 

    return ESP_OK;
}

/// @brief 全局设备控制
/// @param board_ctrl 调用board_status_get()获取系统控制缓存结构体，进行需要修改后放入
/// @param ctrl_select 选择控制的对象，这是一个枚举类型
void sevetest30_board_ctrl(board_ctrl_t *board_ctrl, board_ctrl_select_t ctrl_select)
{
    switch (ctrl_select)
    {
    case BOARD_CTRL_ALL:
        amplifier_set(board_ctrl);
        boost_voltage_set(board_ctrl);
        codechip_mode_and_status_set(board_ctrl);
        codec_config_dac_output(board_ctrl);
        codechip_volume_set(board_ctrl);
        codec_config_adc_input(board_ctrl);
        codec_set_mic_gain(board_ctrl);
        ext_io_value_service();
        ext_io_mode_service();
        break;

    case BOARD_CTRL_AMPLIFIER:
        amplifier_set(board_ctrl);
        break;

    case BOARD_CTRL_BOOST:
        boost_voltage_set(board_ctrl);
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
        ext_io_value_service();
        ext_io_mode_service();
        break;

    default:
        ext_io_value_service();
        ext_io_mode_service();
        break;
    }

    board_ctrl_buf_map(board_ctrl, ctrl_select);
    sevetest30_ble_attr_value_push(); // 向蓝牙客户端推送更改
}

// 设备控制参数缓存映射
void board_ctrl_buf_map(board_ctrl_t *board_ctrl, board_ctrl_select_t ctrl_select)
{
    if (!board_ctrl_buf)
    {
        board_ctrl_buf = malloc(sizeof(board_ctrl_t));
        while (!board_ctrl_buf)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGE("board_ctrl_buf_map", "申请board_ctrl_buf资源发现问题 正在重试");
            board_ctrl_buf = malloc(sizeof(board_ctrl_t));
        }
        memset(board_ctrl_buf, 0, sizeof(board_ctrl_t));
    }
    switch (ctrl_select)
    {
    case BOARD_CTRL_ALL:
        board_ctrl_buf->amplifier_volume = board_ctrl->amplifier_volume;
        board_ctrl_buf->amplifier_mute = board_ctrl->amplifier_mute;
        board_ctrl_buf->boost_voltage = board_ctrl->boost_voltage;
        board_ctrl_buf->codec_mode = board_ctrl->codec_mode;
        board_ctrl_buf->codec_audio_hal_ctrl = board_ctrl->codec_audio_hal_ctrl;
        board_ctrl_buf->codec_dac_pin = board_ctrl->codec_dac_pin;
        board_ctrl_buf->codec_dac_volume = board_ctrl->codec_dac_volume;
        board_ctrl_buf->codec_adc_pin = board_ctrl->codec_adc_pin;
        board_ctrl_buf->codec_adc_gain = board_ctrl->codec_adc_gain;
        board_ctrl_buf->p_ext_io_mode = board_ctrl->p_ext_io_mode;
        board_ctrl_buf->p_ext_io_value = board_ctrl->p_ext_io_value;
        break;

    case BOARD_CTRL_AMPLIFIER:
        board_ctrl_buf->amplifier_volume = board_ctrl->amplifier_volume;
        board_ctrl_buf->amplifier_mute = board_ctrl->amplifier_mute;
        break;

    case BOARD_CTRL_BOOST:
        board_ctrl_buf->boost_voltage = board_ctrl->boost_voltage;
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
void codechip_mode_and_status_set(board_ctrl_t *board_ctrl)
{
    audio_board_handle_t board_handle = audio_board_get_handle();
    audio_hal_ctrl_codec(board_handle->audio_hal, board_ctrl->codec_mode, board_ctrl->codec_audio_hal_ctrl);
}

void codechip_volume_set(board_ctrl_t *board_ctrl)
{
    audio_board_handle_t board_handle = audio_board_get_handle();
    audio_hal_set_volume(board_handle->audio_hal, board_ctrl->codec_dac_volume);
}

// 小型设备控制

// 音频功放设置，音量 取值为 0 -（board_def.h中常量AMP_VOL_MAX的值，原程序中为24）,等于 0 时将使得功放进入低功耗关断状态
void amplifier_set(board_ctrl_t *board_ctrl)
{
    if (board_ctrl->amplifier_volume > AMP_VOL_MAX)
    {
        ESP_LOGE("amplifier_set", "输入了一个超过范围的音量值 - %d", board_ctrl->amplifier_volume);
        board_ctrl->amplifier_volume = AMP_VOL_MAX;
    }
    uint8_t buf[2] = {AMP_DP_COMMAND, 0}; // 设置值越高，实际音量越低

    if (board_ctrl->amplifier_mute == true || board_ctrl->amplifier_volume == 0)
    {
        board_ctrl->p_ext_io_value->amplifier_SD = 0;
        buf[1] = (AMP_VOL_MAX - 0) * AMP_STEP_VOL;
    }
    else
    {
        board_ctrl->p_ext_io_value->amplifier_SD = 1;
        buf[1] = (AMP_VOL_MAX - board_ctrl->amplifier_volume) * AMP_STEP_VOL;
    }
    esp_err_t err = ESP_OK;
    err = i2c_master_write_to_device(DEVICE_I2C_PORT, AMP_DP_ADD, buf, sizeof(buf), 1000 / portTICK_PERIOD_MS);
    sevetest30_board_ctrl(board_ctrl, BOARD_CTRL_EXT_IO);

    if (err != ESP_OK)
        ESP_LOGE("amplifier_set", "与音量控制器通讯时发现问题 描述： %s", esp_err_to_name(err));
    else
        ESP_LOGI("amplifier_set", "扬声器配置已更新");
}

// 辅助电压设置，电压调整值 取值为 0 - (board_def.h中常量BV_VOL_MAX的值(24))
void boost_voltage_set(board_ctrl_t *board_ctrl)
{
    if (board_ctrl->boost_voltage > BV_VOL_MAX)
    {
        ESP_LOGE("boost_voltage_set", "输入了一个超过范围的调整值 - %d", board_ctrl->boost_voltage);
        board_ctrl->boost_voltage = BV_VOL_MAX;
    }

    uint8_t buf[2] = {BV_DP_COMMAND, (BV_VOL_MAX - board_ctrl->boost_voltage) * BV_STEP_VOL};
    esp_err_t err = ESP_OK;
    err = i2c_master_write_to_device(DEVICE_I2C_PORT, BV_DP_ADD, buf, sizeof(buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
        ESP_LOGE("boost_voltage_set", "与电压控制器通讯时发现问题 请检查电池电源连接 描述： %s", esp_err_to_name(err));
    else
        ESP_LOGI("boost_voltage_set", "辅助电压5V已调整");
}
