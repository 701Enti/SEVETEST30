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

// 该文件归属701Enti组织，主要由SEVETEST30开发团队维护，包含各种SE30对姿态传感器LSM6DS3TR-C的操作
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体不包含i2c通讯的任何初始化配置，若您单独使用而未进行配置，这可能无法运行
//       对于功能设计需求，并不是所有寄存器都被考虑，都要求配置，实际上可以遵从默认设置
// 您可以在ST官网获取LSM6DS3TR-C的相关手册包含程序实现思路 https://www.st.com/zh/mems-and-sensors/lsm6ds3tr-c.html
// github: https://github.com/701Enti
// bilibili: 701Enti

//我们这里使用了普通FIFO模式，因为我们只是用于操作UI而无需完整的姿态数据以及严格的实时性，低误差性
//初始化任务是 设置加速度计和陀螺仪的ORD（为了获得中断输出不得设置加速度计到掉电模式)，以及XL_HM_MODE位的设置
//            启用BDU和DRDY_MASK 配置INT1
//            设置FIFO抽取系数 设置FIFO_ORD   设置 加速度 角速率 数据源到FIFO   配置为FIFO Continuous mode模式 不使用中断
//读取步骤是  读出FIFO存储的数据 / 读出自动记录的数据     

//   快捷监测 6D检测 自由落体检测 计步器(必须 ORD 26Hz+)

//使用到的寄存器列表
enum{
  //综合的
  IMU_REG_CTRL1_XL = 0x10,//ODR_XL 26Hz+ ||| 设置大于或等于正负4g
  IMU_REG_CTRL3_C = 0x12,//BDU->1  IF_INC->1
  IMU_REG_CTRL4_C = 0x13, // DRDY_MASK
  IMU_REG_CTRL6_C = 0x15,//XL_HM_MODE
  IMU_REG_CTRL8_XL = 0x17,// LOW_PASS_ON_6D
  IMU_REG_CTRL10_C = 0x19,//FUNC_EN->1 PEDO_EN->1 PEDO_RST_STEP从0跳1再置0清除计步器步数
  IMU_REG_INT1_CTRL = 0x0D,//

  //FIFO
  IMU_REG_FIFO_CTRL3 = 0x08,//抽取系数 DEC_FIFO_G DEC_FIFO_XL
  IMU_REG_FIFO_CTRL4 = 0x09,//抽取系数
  IMU_REG_FIFO_CTRL5 = 0x0A,//ODR_FIFO FIFO_MODE->110b(更改ORD需要读取出有用数据再置Bypass mode,修改ORD后改为原来模式)
  IMU_REG_FIFO_STATUS1 = 0x3A,//DIFF_FIFO查看数据个数 
  IMU_REG_FIFO_STATUS2 = 0x3B,//DIFF_FIFO查看数据个数 全部读完FIFO_EMPTY位被设置为高
  IMU_REG_FIFO_STATUS3 = 0x3C,//FIFO_PATTERN正在读取的传感器和字节
  IMU_REG_FIFO_STATUS4 = 0x3D,//FIFO_PATTERN正在读取的传感器和字节
  
  //自由落体检测
  IMU_REG_WAKE_UP_SRC = 0x1B,//FF_IA
  IMU_REG_WAKE_UP_DUR = 0x5C,//FF_DUR5
  IMU_REG_FREE_FALL = 0x5D,// FF_THS FF_DUR

  //6D检测
  IMU_REG_D6D_SRC = 0x1D//all
  IMU_REG_MD1_CFG = 0x5E,//INT1_6D  ||| INT1_FF
  IMU_REG_TAP_THS_6D = 0x59,//SIXD_THS
  IMU_REG_TAP_CFG = 0x58,//INTERRUPTS_ENABLE(6D 自由落体检测)  LIR->1(全局有效) 
  
  //计步器(必须 ORD 26Hz+)
  IMU_REG_STEP_COUNTER_L = 0x4B,//read
  IMU_REG_STEP_COUNTER_H = 0x4C,//read
  IMU_REG_CONFIG_PEDO_THS_MIN,//PEDO_FS->1 

  //温度监测
  IMU_REG_OUT_TEMP_L = 0x20,//read
  IMU_REG_OUT_TEMP_H = 0x21,//read

}



