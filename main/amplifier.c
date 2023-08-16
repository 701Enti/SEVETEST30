// 该文件由701Enti编写，包含一些NS4268基本辅助配置与DC音量控制(数字电位器方式)的通讯，此处用TPL0401B作为辅助数字电位器
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您未进行配置，这可能无法运行，或许可参考同一项目组中sevetest30_BusConf.h，其中包含粗略易用的配置工作
// 邮箱：   3044963040@qq.com
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.10

#include <driver/i2c.h>
#include "amplifier.h"
#include "sevetest30_BusConf.h"
#include "sevetest30_gpio.h"


//音频功放设置，是否静音(true/false)，是否掉电(true/false)，音量 取值为 0-24
void amplifier_set(bool mute,bool sd,uint8_t volume){
 uint8_t buf [1] = {volume*STEP_VOL};
 ext_io_value_data.ns4268_MUTE = mute ;
 ext_io_value_data.ns4268_SD = sd ;
 i2c_master_write_to_device(I2C_MASTER_NUM,DP_ADD,buf,sizeof(buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}