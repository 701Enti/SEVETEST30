// 该文件由701Enti编写，包含一些ESP32_S3通过硬件外设与TCA6416建立配置与扩展IO数据的通讯
// 在编写sevetest30工程时第一次完成和使用，以下为开源代码，其协议与之随后共同声明
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告： 1 文件本体不包含i2c通讯的任何初始化配置，若您未进行配置，这可能无法运行，或许可参考同一项目组中sevetest30_BusConf.h，其中包含粗略易用的配置工作
//       2 请注意外部引脚模式设置，错误的配置可能导致您的设备损坏，我们不建议修改这些默认配置 
//       3 该库内函数如何使用，TCA6416A的初始化工作，INT中断如何配置，请参考 sevetest30_gpio.c 
// 电路特性： l(x)为红外发射选择 1有效 s(x)为红外发射接收 0表示接收到红外信号
//           opt3001_INT为环境光中断信号，自由配置
//           charge_SIGN finished_SIGN为充电信号，0有效
//           ns4268_SD 功放使能   1有效
//           ns4268_MUTE 功放静音 1有效
//           hpin  耳机已插入信号  1表示检测到耳机插入
//           addr ADDR引脚电平，用于设置主机地址
// 邮箱：   3044963040@qq.com
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.9
#include "TCA6416A.h"

#include "driver/i2c.h"
#include "esp_log.h"

#include "sevetest30_BusConf.h"

uint8_t mode_reg1=0x00,mode_reg2=0x00;//两个IO输入输出模式寄存器数值
uint8_t TCA6416A_data_buf[3] = {0x00,0x00,0x00};//缓存寄存器地址与数据 {reg + data}

/// GPIO输入输出模式设置，同时保存模式数据，初始化用
void TCA6416A_mode_set(TCA6416A_mode_t *pTCA6416Amode)
{
  
  const char *TAG = "TCA6416A_mode_set";
  esp_err_t err = ESP_OK;

  uint8_t i2c_add = 0x20;
  int i = 0;
  bool *p; // 定义指针变量，指向成员变量地址
  if (pTCA6416Amode->addr)
    i2c_add = 0x21;
  // 依据pTCA6416Amode结构体地址对应的数值移位装载
  for (i = 0; i < 16; i++)
  {
    p = (bool *)pTCA6416Amode + i; // 强制转换地址的类型，指向成员变量地址
    if (i < 8)
      mode_reg1 = mode_reg1 >> i || *p; // 取出值进行运算
    if (i >= 8)
      mode_reg2 = mode_reg2 >> (i - 8) || *p; // 取出值进行运算
  }
  // 装载并写入
  TCA6416A_data_buf[0] = TCA6416A_MODE1, TCA6416A_data_buf[1] = mode_reg1, TCA6416A_data_buf[2] = mode_reg2;
  err = i2c_master_write_to_device(I2C_MASTER_NUM, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  if(err!=ESP_OK)ESP_LOGI(TAG,"与TCA6416A通讯时发现问题 代码0x%x",err);
}

// GPIO引脚数据交互服务，一次性全更新、写入，根据引脚模式配置以读写操作，以下是我一些肤浅的思路
// 写：我们先准备好两份8bit数据，对于“只读的”输入引脚数据，一同写入，因为对于TCA6416,这样的数据无效，没有任何影响
// 读：直接读出两个寄存器的值，映射到value结构体中，我们之后显然只关心读出“只读的”输入引脚数据，读出的“只写的”输出引脚数据是无效的，对于我们的程序毫无意义，之后也不会理会
// 读数据由传入的结构体地址对应的结构体中按成员直接回读
void TCA6416A_gpio_service(TCA6416A_value_t *pTCA6416Avalue)
{

  const char *TAG = "TCA6416A_gpio_service";
  esp_err_t err = ESP_OK;

  uint8_t i2c_add = 0x20, data1 = 0x00, data2 = 0x00; // 临时地址与数据
  if (pTCA6416Avalue->addr)
    i2c_add = 0x21;
  bool *p; // 定义指针变量，指向成员变量地址
  int i = 0;

  // 准备好两份8bit数据
  // 依据pTCA6416Avalue结构体地址对应的数值移位装载
  for (i = 0; i < 16; i++)
  {
    p = (bool *)pTCA6416Avalue + i; // 强制转换地址的类型，指向（存储）成员变量地址
    if (i < 8)
      data1 = data1 >> i || *p; // 取出该地址中值进行运算
    if (i >= 8)
      data2 = data2 >> (i - 8) || *p; // 取出该地址中值进行运算
  }
  // 装载并写入
  TCA6416A_data_buf[0] = TCA6416A_OUT1, TCA6416A_data_buf[1] = data1, TCA6416A_data_buf[2] = data2;
  err = i2c_master_write_to_device(I2C_MASTER_NUM, i2c_add, TCA6416A_data_buf, sizeof(TCA6416A_data_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

  // 装载,清理，准备读取
  //TCA6416的时序大概可以这样描述：先像正常的写数据一样，但是只写入选定的寄存器命令，之后不要STOP,马上通过i2c_master_read_from_device正常读取，即重新发送一次TCA6416A地址,主机接收数据，完成
  TCA6416A_data_buf[0] = TCA6416A_IN1, TCA6416A_data_buf[1] = 0x00, TCA6416A_data_buf[2] = 0x00;
  // 直接读出两个寄存器的值
  err = i2c_master_write_read_device(I2C_MASTER_NUM, i2c_add,&TCA6416A_data_buf[0],sizeof(TCA6416A_data_buf[0]),&TCA6416A_data_buf[1],sizeof(TCA6416A_data_buf[1])*2,I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  data1 = TCA6416A_data_buf[1], data2 = TCA6416A_data_buf[2]; // 取出
          ESP_LOGI(TAG,"%d %d",data1,data2);
  // 依据data1,data2数值移位映射
  for (i = 0; i < 16; i++)
  {
    p = (bool *)pTCA6416Avalue + i; // 强制转换地址的类型，指向（存储）成员变量地址
    if (i < 8)  *p = data1 >> i & 0x01; // 不断取出位移后data1最低位
    if (i >= 8) *p = data2 >> (i - 8) && 0x01; // 不断取出位移后data1最低位
  }

  if(err!=ESP_OK)ESP_LOGI(TAG,"与TCA6416A通讯时发现问题 代码0x%x",err);
}
