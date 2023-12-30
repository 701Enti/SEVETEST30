// 该文件由701Enti编写，包含各种SE30针对性硬件控制
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.11.25

#include "board.h"
#include "fonts_chip.h"
#include "board_ctrl.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "sevetest30_touch.h"

esp_periph_set_handle_t se30_periph_set_handle;


//总线和设备,GPIO的初始化
void sevetest30_all_board_init(board_ctrl_t* board_ctrl,board_device_handle_t* board_device_handle){
    audio_board_init();
    sevetest30_gpio_init(board_ctrl->p_ext_io_mode,board_ctrl->p_ext_io_value); 
    //音频解码
    codechip_set(AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    //关键外设
    amplifier_set(board_ctrl);
    
    // boost_voltage_set(board_ctrl);
    // fonts_chip_init(board_device_handle);
    // vibra_motor_init(get_vibra_motor_IN1_gpio(),get_vibra_motor_IN2_gpio());


}

//调整音频编解码器芯片参数
void codechip_set(audio_hal_codec_mode_t mode, audio_hal_ctrl_t audio_hal_ctrl){
    audio_board_handle_t board_handle = audio_board_get_handle();
    audio_hal_ctrl_codec(board_handle->audio_hal,mode,audio_hal_ctrl);
    audio_hal_set_volume(board_handle->audio_hal,100);
}

//小型设备控制

//音频功放设置，音量 取值为 0 -（board_def.h中常量AMP_VOL_MAX的值，原程序中为24）,等于 0 时将使得功放进入低功耗关断状态
void amplifier_set(board_ctrl_t* board_ctrl){
 if (board_ctrl->amplifier_volume > AMP_VOL_MAX){
    ESP_LOGE("amplifier_set","输入了一个超过范围的音量值 - %d",board_ctrl->amplifier_volume);
    board_ctrl->amplifier_volume = AMP_VOL_MAX;
 }
 if (board_ctrl->amplifier_volume == 0)
 board_ctrl->p_ext_io_value->amplifier_SD = 0;
 else
 board_ctrl->p_ext_io_value->amplifier_SD = 1;

 uint8_t buf [2] = {AMP_DP_COMMAND,(AMP_VOL_MAX-board_ctrl->amplifier_volume)*AMP_STEP_VOL};//设置值越高，实际音量越低
 esp_err_t err = ESP_OK;
 err = i2c_master_write_to_device(I2C_NUM_0,AMP_DP_ADD,buf,sizeof(buf), 1000 / portTICK_PERIOD_MS);
 if(err!=ESP_OK)ESP_LOGI("amplifier_set","与音量控制器通讯时发现问题 描述： %s",esp_err_to_name(err));
 else
 ESP_LOGI("amplifier_set","扬声器配置已更新");
}

//辅助电压设置，电压调整值 取值为 0 - (board_def.h中常量BV_VOL_MAX的值(24))
void boost_voltage_set(board_ctrl_t* board_ctrl){
    if (board_ctrl->boost_voltage > BV_VOL_MAX){
    ESP_LOGE("boost_voltage_set","输入了一个超过范围的调整值 - %d",board_ctrl->boost_voltage);
    board_ctrl->boost_voltage = BV_VOL_MAX;
    }

    uint8_t buf [2] = {BV_DP_COMMAND,(BV_VOL_MAX-board_ctrl->boost_voltage)*BV_STEP_VOL};
    esp_err_t err = ESP_OK;
    err = i2c_master_write_to_device(I2C_NUM_0,BV_DP_ADD,buf,sizeof(buf), 1000 / portTICK_PERIOD_MS); 
    if(err!=ESP_OK)ESP_LOGE("boost_voltage_set","与电压控制器通讯时发现问题 描述： %s",esp_err_to_name(err));
    else
    ESP_LOGI("boost_voltage_set","辅助电压5V已调整");
}