// 该文件由701Enti整合，包含一些sevetest30的总线通讯配置
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体针对sevetest30的硬件设计,TPL0401B数字电位器DC音量控制支持
// 邮箱：   3044963040@qq.com
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.6.28

#ifndef _SEVETEST30_BUSCONF_H_
#define _SEVETEST30_BUSCONF_H_
#endif 

//以下设置会联动到工程中所有上级函数中关于I2C总线的读写配置

#define I2C_MASTER_SCL_IO           (GPIO_NUM_47)// I2C时钟线（SCL)对应的GPIO号     
#define I2C_MASTER_SDA_IO           (GPIO_NUM_48)// I2C数据线（SDA)对应的GPIO号      
#define I2C_MASTER_NUM              0                          //I2C端口号, 这个端口号是否有效将取决于芯片
#define I2C_MASTER_FREQ_HZ          100000                     //I2C通讯频率,单位HZ
#define I2C_MASTER_TIMEOUT_MS       1000                       //设定超时时间,单位ms
#define I2C_TX_BUF   0         //设定不需要发送缓冲
#define I2C_RX_BUF   0         //设定不需要接收缓冲
#define I2C_INTR     0         //设定不需要I2C中断

void I2C_init();//全局总线初始化





