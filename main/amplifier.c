// 该文件由701Enti编写，包含一些NS4268基本辅助配置与DC音量控制(数字电位器方式)的通讯，此处用TPL0401B作为辅助数字电位器
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.10

#include "driver/i2c.h"
#include "esp_log.h"
#include "amplifier.h"
#include "sevetest30_gpio.h"


//音频功放设置，音量 取值为 0 -（常量VOL_MAX的值，原程序中为24）,等于 0 时将使得功放进入低功耗关断状态
void amplifier_set(uint8_t volume){
 if (volume > VOL_MAX){
    ESP_LOGI("amplifier_set","输入了一个超过范围的音量值 - %d",volume);
    volume = VOL_MAX;
 }
 if (volume == 0)
 ext_io_value_data.ns4268_SD = 0;
 else
 ext_io_value_data.ns4268_SD = 1;

 uint8_t buf [2] = {DP_COMMAND,(VOL_MAX-volume)*STEP_VOL};//设置值越高，实际音量越低
 esp_err_t err = ESP_OK;
 err = i2c_master_write_to_device(I2C_NUM_0,DP_ADD,buf,sizeof(buf), 1000 / portTICK_PERIOD_MS);
 if(err!=ESP_OK)ESP_LOGI("amplifier_set","与音量控制器通讯时发现问题 描述： %s",esp_err_to_name(err));
 else
 ESP_LOGI("amplifier_set","扬声器配置已更新");
}