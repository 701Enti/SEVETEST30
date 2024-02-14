// 该文件由701Enti编写，包含各种SE30针对性硬件控制
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.11.25

#include "board_ctrl.h"
#include "board.h"
#include "fonts_chip.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "sevetest30_touch.h"

esp_periph_set_handle_t se30_periph_set_handle;
board_ctrl_t *board_ctrl_buf = NULL;

void amplifier_set(board_ctrl_t *board_ctrl);
void boost_voltage_set(board_ctrl_t *board_ctrl);
void codechip_mode_and_status_set(board_ctrl_t *board_ctrl);
void codechip_volume_set(board_ctrl_t *board_ctrl);
void board_ctrl_buf_map(board_ctrl_t *board_ctrl, board_ctrl_select_t ctrl_select);

// 全局初始化
void sevetest30_all_board_init(board_ctrl_t *board_ctrl, board_device_handle_t *board_device_handle)
{
    // 初始化
    audio_board_init();
    sevetest30_gpio_init(board_ctrl->p_ext_io_mode, board_ctrl->p_ext_io_value);
    fonts_chip_init(board_device_handle);
    vibra_motor_init(get_vibra_motor_IN1_gpio(), get_vibra_motor_IN2_gpio());

    // 配置
    sevetest30_board_ctrl(board_ctrl, BOARD_CTRL_ALL);
}

// 全局设备控制
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

    board_ctrl_buf_map(board_ctrl,ctrl_select);
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
    err = i2c_master_write_to_device(I2C_PORT, AMP_DP_ADD, buf, sizeof(buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
        ESP_LOGI("amplifier_set", "与音量控制器通讯时发现问题 描述： %s", esp_err_to_name(err));
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
    err = i2c_master_write_to_device(I2C_PORT, BV_DP_ADD, buf, sizeof(buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK)
        ESP_LOGE("boost_voltage_set", "与电压控制器通讯时发现问题 描述： %s", esp_err_to_name(err));
    else
        ESP_LOGI("boost_voltage_set", "辅助电压5V已调整");
}

board_ctrl_t *board_status_get()
{
    return board_ctrl_buf;
}