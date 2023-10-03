// 该文件由701Enti编写，包含一些NS4268基本辅助配置与DC音量控制(数字电位器方式)的通讯，此处用TPL0401B作为辅助数字电位器
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
// 邮箱：  hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.10

#ifndef _AMPLIFIER_H_
#define _AMPLIFIER_H_
#endif

#include <string.h>
#include <stdbool.h>

#define  DP_ADD    0x3E //使用的数字电位器I2C地址，这里用的是TPL0401B，它性价比足够高，可以尽量使用完全一样型号的数字电位器，因为这可能涉及通讯时是否需要命令的特殊问题，程序可能不兼容
#define  DP_COMMAND 0x00 //操作命令，部分数字电位器操作需要一个固有命令，在寄存器设置的8位数据之前发送，如TPL0401B需要0x00
#define  VOL_MAX 24      //由于可以设置的阻值范围是比较大的，而屏幕大小有限，为了方便用户调节，将DC音量能够识别到的电压范围对应的阻值范围缩小到0-VOL_MAX单位,需要修改则STEP_VOL也要改
#define  STEP_VOL  0x05  //单位步数，由于可以设置的阻值范围是比较大的，而屏幕大小有限，为了方便用户调节，将DC音量能够识别到的电压范围对应的阻值范围缩小到0-VOL_MAX单位，其中一个单位所对应的寄存器设置值为STEP_VOL

void amplifier_set(uint8_t volume);