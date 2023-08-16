// 该文件由701Enti整合，包含一些sevetest30的总线通讯配置
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体只针对sevetest30的硬件设计
// 邮箱：   3044963040@qq.com
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.6.28

#include "sevetest30_BusConf.h"

#include "driver/i2c.h"
#include "esp_log.h"

void I2C_init(){

  const char *TAG = "I2C_init";

     int i2c_master_port = I2C_MASTER_NUM;
      i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;//主机模式 
        conf.sda_io_num = I2C_MASTER_SDA_IO;
        conf.scl_io_num = I2C_MASTER_SCL_IO;
        conf.sda_pullup_en = GPIO_PULLUP_DISABLE; //sevetest30板载上拉电阻
        conf.scl_pullup_en = GPIO_PULLUP_DISABLE; //sevetest30板载上拉电阻
        conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    if(i2c_param_config(i2c_master_port, &conf) == ESP_ERR_INVALID_ARG) {
      ESP_LOGI(TAG,"初始化I2C总线时发现错误的配置参数");
      return;
    }
    else{
      i2c_driver_install(i2c_master_port,conf.mode,0,0,NULL);
      ESP_LOGI(TAG,"初始化I2C总线完成");
    }
      
}

