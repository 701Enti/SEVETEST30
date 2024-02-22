// 该文件归属701Enti组织，由SEVETEST30开发团队维护，包含一些ESP32_S3通过硬件外设与TCA6416建立配置与扩展IO数据的通讯
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 本库特性：1 由于IO控制时，有随时需要调用TCA6416A写入函数的需求，本库不会出现调用一次函数归定只能改一个IO或读一个IO还要传一系列参数的尴尬问题，而是一齐读写,同时还会保存实时IO数据，因此没有用到电平反转寄存器
//          2 使用时直接修改公共变量以在项目非常方便使用，加之，可以像sevetest30_gpio.c封装后使用FreeRTOS支持，并添加中断支持，一但IO电平变化就读取，没有变就不读，客观上可以大大提高资源利用率
// 读写原理：   运用结构体地址一般为结构体中第一个成员变量地址，并且本例中，成员类型均为bool,地址递加从而可以方便地扫描所有成员，
// 敬告： 0 为更加方便后续开发或移植，本库不包含关于FreeRTOS支持的封装，公共变量修改方式的服务封装，以及中断服务的封装，如果需要参考，请参照sevetest30_gpio.c
//       1 本库会保存实时IO数据，因此没有用到电平反转寄存器            
//       2 文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
//       3 请注意外部引脚模式设置，错误的配置可能导致您的设备损坏，我们不建议修改这些默认配置 
//       4 对于设计现实的不同，您可以更改结构体成员变量名，但是必须确保对应的IO次序不变如 P00 P01 P02 P03 以此类推
//         同时成员变量名是上级程序识别操作引脚的关键，如果需要使用其上级程序而不仅仅是TCA6416A库函数，结构体成员变量名不应该随意修改，对当前硬件的更新必须修改上层代码
// github: https://github.com/701Enti
// bilibili: 701Enti


#include "TCA6416A.h"

#include "driver/i2c.h"
#include "esp_log.h"

uint8_t TCA6416A_data_buf[3] = {0x00,0x00,0x00};//缓存寄存器地址与数据 {reg + data}

/// GPIO输入输出模式设置，同时保存模式数据，初始化用
void TCA6416A_mode_set(TCA6416A_mode_t *pTCA6416Amode)
{  
  const char *TAG = "TCA6416A_mode_set";
  esp_err_t err = ESP_OK;

  if (pTCA6416Amode == NULL){
    ESP_LOGE(TAG,"无法处理的空指针");
    return;
  } 

  uint8_t data1 = NULL, data2 = NULL; // 临时数据缓存
  bool *p; // 定义指针变量，指向成员变量地址 

  //确定设备地址
  uint8_t i2c_add = 0x20;
  if (pTCA6416Amode->addr)
    i2c_add = 0x21;

  // 依据pTCA6416Amode结构体递增地址对应的bool数值按8位缓冲变量对应的bit位置装载  
  int i = 0;
  for (i = 0; i < 16; i++)
  {
    p = (bool *)pTCA6416Amode + i; // 强制转换地址的类型，指向成员变量地址
    if (i < 8)
      data1 = *p << i | data1; // 取出值进行运算
    if (i >= 8)
      data2 = *p << (i-8) | data2; // 取出值进行运算
  }

  // 装载并写入
  TCA6416A_data_buf[0] = TCA6416A_MODE1, TCA6416A_data_buf[1] = data1, TCA6416A_data_buf[2] = data2;
  err = i2c_master_write_to_device(I2C_NUM_0, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), 1000 / portTICK_PERIOD_MS);
  if(err!=ESP_OK)ESP_LOGE(TAG,"与TCA6416A通讯时发现问题 描述： %s",esp_err_to_name(err));
}

// GPIO引脚数据交互服务，一次性全更新、写入，根据引脚模式配置以读写操作，以下是我一些肤浅的思路
// 写：我们先准备好两份8bit数据，对于“只读的”输入引脚数据，一同写入，因为对于TCA6416,这样的数据无效，没有任何影响
// 读：直接读出两个寄存器的值，映射到value结构体中，我们之后显然只关心读出“只读的”输入引脚数据，读出的“只写的”输出引脚数据是无效的，对于我们的程序毫无意义，之后也不会理会
// 读数据由传入的结构体地址对应的结构体中按成员直接回读
void TCA6416A_gpio_service(TCA6416A_value_t *pTCA6416Avalue)
{  
  const char *TAG = "TCA6416A_gpio_service";
  esp_err_t err = ESP_OK;

  if (pTCA6416Avalue == NULL){
    ESP_LOGE(TAG,"无法处理的空指针");
    return;
  } 

  bool *p; // 定义指针变量，指向成员变量地址
  uint8_t data1 = NULL, data2 = NULL; // 临时数据缓存

  //确定设备地址 
  uint8_t i2c_add = 0x20;
  if (pTCA6416Avalue->addr)
    i2c_add = 0x21;

  // 准备好两份8bit数据
  // 依据pTCA6416Avalue结构体递增地址对应的bool数值按8位缓冲变量对应的bit位置装载  
  int i = 0;
  for (i = 0; i < 16; i++)
  {
    p = (bool *)pTCA6416Avalue + i; // 强制转换地址的类型，指向成员变量地址
    if (i < 8)
      data1 = *p << i | data1; // 取出值进行运算
    if (i >= 8)
      data2 = *p << (i-8) | data2; // 取出值进行运算
  }

  // 装载并写入
  TCA6416A_data_buf[0] = TCA6416A_OUT1, TCA6416A_data_buf[1] = data1, TCA6416A_data_buf[2] = data2;
  err = i2c_master_write_to_device(I2C_NUM_0, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), 1000 / portTICK_PERIOD_MS);

  // 装载,清理，准备读取
  //TCA6416的时序大概可以这样描述：先像正常的写数据一样，但是只写入选定的寄存器命令，之后不要STOP,马上通过i2c_master_read_from_device正常读取，即重新发送一次TCA6416A地址,主机接收数据，完成
  TCA6416A_data_buf[0] = TCA6416A_IN1, TCA6416A_data_buf[1] = 0x00, TCA6416A_data_buf[2] = 0x00;

  // 直接读出两个寄存器的值
  err = i2c_master_write_read_device(I2C_NUM_0, i2c_add,&TCA6416A_data_buf[0],sizeof(TCA6416A_data_buf[0]),&TCA6416A_data_buf[1],sizeof(TCA6416A_data_buf[1])*2,1000 / portTICK_PERIOD_MS);
  data1 = TCA6416A_data_buf[1], data2 = TCA6416A_data_buf[2]; // 取出

  // 依据data1,data2数值移位映射
  for (i = 0; i < 16; i++)
  {
    p = (bool *)pTCA6416Avalue + i; // 强制转换地址的类型，指向（存储）成员变量地址
    if (i < 8)  *p = (data1 >> i) & 0x01; // 不断取出位移后data1最低位
    if (i >= 8) *p = data2 >> (i - 8) & 0x01; // 不断取出位移后data1最低位
  }

  if(err!=ESP_OK)ESP_LOGE(TAG,"与TCA6416A通讯时发现问题 描述： %s",esp_err_to_name(err));
}
