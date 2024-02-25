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
//初始化任务是 设置加速度计和陀螺仪的ORD（为了获得中断输出不得设置加速度计到掉电模式，以及XL_HM_MODE位的设置
//            启用BDU和DRDY_MASK 配置INT1
//           设置FIFO深度   设置 加速度 角速率 数据源到FIFO   配置为FIFO模式 
//读取步骤是 监测INT1状态： 一但外部IO(TCA6416A)中断触发，检测LSM6DS3TRC接入INT1中断触发 
//          运行读取：     读出FIFO存储的数据
//          复位FIFO缓冲区：将模式转到旁路模式又切换回FIFO模式，进入下一个循环

//使用到的寄存器列表
enum{
  IMU_REG_CTRL1_XL,//ODR_XL
  IMU_REG_CTRL3_C,//BDU
  IMU_REG_CTRL4_C, // DRDY_MASK
  IMU_REG_CTRL6_C,//XL_HM_MODE
  IMU_REG_INT1_CTRL,//

}



