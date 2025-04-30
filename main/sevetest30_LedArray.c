
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

 // 包含WS2812构成的LED阵列的图形与显示处理，使用的WS2812底层驱动程序为官方组件
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // 敬告：文件本体不包含WS2812硬件驱动代码，而是参考ESP-IDF的例程文件,使用官方led_strip组件依赖,同时还使用了源文件中的hsv到rgb的转换函数,非常感谢
 // 绘制函数本身不会刷新屏幕,需要运行屏幕刷新,才会在屏幕上点亮
 // 绘制函数规定使用字模点阵的左上角的点作为其坐标表示点,LED的延伸方向为X轴正方向,沿线方向的灯排对应的gpio号在ledarray_gpio_info数组的先后顺序指代其在Y轴正向出现顺序,对应角标+1为其对应的Y轴坐标
 // 在标准SEVETEST30-LedArray板下,正视"701Enti"标志,此时坐标原点应为最左上角像素点,X轴正方向向右,Y轴正方向向下,原点坐标为(1,1)即填入函数 x = 1,y = 1
 // 本库WS2812使用硬件RMT驱动,占用两个RMT模块,由ESP-IDF内部驱动程序动态分配RMT模块,无需手动指定
 // 如果是12排分为6组(0-5),RMT输出引脚将在这些连接的GPIO之间来回切换,实现阵列的刷新,同时提供局部刷新服务模式提高刷新效率
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#include "sevetest30_LedArray.h"
#include "sevetest30_UI.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdarg.h"
#include "led_strip.h"
#include "led_strip_interface.h"
#include "driver/rmt_tx.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include "hal/rmt_types.h"
#include "gt32l32s0140.h"
#include "esp_check.h"

// 一个图像可看作不同颜色的像素组合，而每个像素颜色可用红绿蓝三元色的深度（亮度）表示
// 因此，我们可以将一个图像分离成三个单色图层，我们就定义三个8bit数组
// 分别表示这三个图层中[12x24=288]像素R G B 三色的明暗程度，合成他们便可表示显示板上的图像
// 但是考虑到硬件设计中，WS2812是一排24灯为最小操作单位写入，所以每个图层最好分割成12排
// 最后，我们选择以排为单位，3个图层，每个图层12排，共3x12=36个数组，这里称为缓冲区
// SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必增加足够变量 red_y(x) green_y(x) blue_y(x)

//请勿移动以下数组定义位置,可能产生内存对齐问题
uint8_t red_y1[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y2[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y3[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y4[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y5[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y6[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y7[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y8[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y9[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y10[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y11[LINE_LED_NUMBER] = { 0x00 };
uint8_t red_y12[LINE_LED_NUMBER] = { 0x00 };
//请勿移动以下数组定义位置,可能产生内存对齐问题
uint8_t green_y1[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y2[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y3[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y4[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y5[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y6[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y7[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y8[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y9[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y10[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y11[LINE_LED_NUMBER] = { 0x00 };
uint8_t green_y12[LINE_LED_NUMBER] = { 0x00 };
//请勿移动以下数组定义位置,可能产生内存对齐问题
uint8_t blue_y1[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y2[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y3[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y4[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y5[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y6[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y7[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y8[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y9[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y10[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y11[LINE_LED_NUMBER] = { 0x00 };
uint8_t blue_y12[LINE_LED_NUMBER] = { 0x00 };

const int ledarray_gpio_num_list[VERTICAL_LED_NUMBER] = { 4, 5, 6, 7, 17, 18, 8, 42, 41, 40, 39, 38 }; // ws2812数据线连接的GPIO号列表 顺序为 第一行 到 最后一行
uint8_t compound_result[LINE_LED_NUMBER * 3] = { 0 }; // 发送给WS2812的格式化数据缓存，GRB格式

//本库WS2812使用硬件RMT驱动,占用两个RMT模块
//如果是12排分为6组(0-5),RMT输出引脚将在这些连接的GPIO之间来回切换,实现阵列的刷新,同时提供局部刷新服务模式提高刷新效率
led_strip_handle_t strip0 = NULL;
led_strip_handle_t strip1 = NULL;
rmt_channel_handle_t strip0_rmt_channel = NULL;
rmt_channel_handle_t strip1_rmt_channel = NULL;

// 数字 0-9
const uint8_t matrix_0[7] = { 0xF0, 0x90, 0x90, 0x90, 0x90, 0x90, 0xF0 };
const uint8_t matrix_1[7] = { 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
const uint8_t matrix_2[7] = { 0xF0, 0x10, 0x10, 0xF0, 0x80, 0x80, 0xF0 };
const uint8_t matrix_3[7] = { 0xF0, 0x10, 0x10, 0xF0, 0x10, 0x10, 0xF0 };
const uint8_t matrix_4[7] = { 0x10, 0x30, 0x50, 0xF0, 0x10, 0x10, 0x10 };
const uint8_t matrix_5[7] = { 0xF0, 0x80, 0x80, 0xF0, 0x10, 0x10, 0xF0 };
const uint8_t matrix_6[7] = { 0xF0, 0x80, 0x80, 0xF0, 0x90, 0x90, 0xF0 };
const uint8_t matrix_7[7] = { 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10 };
const uint8_t matrix_8[7] = { 0xF0, 0x90, 0x90, 0xF0, 0x90, 0x90, 0xF0 };
const uint8_t matrix_9[7] = { 0xF0, 0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0 };



/******************************自动屏幕刷新服务 [绘制函数本身不会刷新屏幕,需要手动或自动运行屏幕刷新,才会在屏幕上点亮] *****************************/
uint8_t draw_line_count[VERTICAL_LED_NUMBER] = { 0 };//每行的绘制计数,在局部刷新模式下,为0次绘制活动的行不刷新屏幕
ledarray_auto_refresh_mode_t refresh_mode_buf = LEDARRAY_AUTO_REFRESH_DISABLE;
SemaphoreHandle_t refresh_Task_Mutex = NULL;

/// @brief [单次全刷任务 - ALL_ONCE]一次性刷新整个屏幕所有行,全屏刷新之后才发生延时
void refresh_ALL_ONCE_Task() {
	while (1) {
		for (int idx = 0;idx <= VERTICAL_LED_NUMBER / 2;idx++) {
			ledarray_set_and_write(idx);
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

/// @brief [多次全刷任务 - ALL_MULTIPLE]分多步进地完成刷新整个屏幕所有行,每步之后发生延时
void refresh_ALL_MULTIPLE_Task() {
	while (1) {
		for (int idx = 0;idx <= VERTICAL_LED_NUMBER / 2;idx++) {
			ledarray_set_and_write(idx);
			vTaskDelay(pdMS_TO_TICKS(2));
		}
	}
}

/// @brief [单次局刷任务 - PART_ONCE]一次性刷新所有发生绘制活动的行,需要部分完全刷新之后才发生延时
void refresh_PART_ONCE_Task() {
	while (1) {
		for (int idx = 0;idx <= VERTICAL_LED_NUMBER / 2;idx++) {
			if (draw_line_count[idx * 2 + 0] || draw_line_count[idx * 2 + 1])
				ledarray_set_and_write(idx);
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

/// @brief [多次局刷任务 - PART_MULTIPLE]分多步进地完成刷新所有发生绘制活动的行,每步之后发生延时
void refresh_PART_MULTIPLE_Task() {
	while (1) {
		for (int idx = 0;idx <= VERTICAL_LED_NUMBER / 2;idx++) {
			if (draw_line_count[idx * 2 + 0] || draw_line_count[idx * 2 + 1])
				ledarray_set_and_write(idx);
			vTaskDelay(pdMS_TO_TICKS(10));
		}
	}
}

/// @brief 设置LED阵列的自动刷新模式,立即启动生效
/// @param mode 刷新模式,这是一个枚举类型
void ledarray_set_auto_refresh_mode(ledarray_auto_refresh_mode_t mode) {
	switch (refresh_mode_buf)
	{
	case LEDARRAY_AUTO_REFRESH_ALL_ONCE:
		vTaskDelete(xTaskGetHandle("ALL_ONCE"));
		break;
	case LEDARRAY_AUTO_REFRESH_ALL_MULTIPLE:
		vTaskDelete(xTaskGetHandle("ALL_MULTIPLE"));
		break;
	case LEDARRAY_AUTO_REFRESH_PART_ONCE:
		vTaskDelete(xTaskGetHandle("PART_ONCE"));
		break;
	case LEDARRAY_AUTO_REFRESH_PART_MULTIPLE:
		vTaskDelete(xTaskGetHandle("PART_MULTIPLE"));
		break;
	default:
		break;
	}
	switch (mode)
	{
	case LEDARRAY_AUTO_REFRESH_ALL_ONCE:
		xTaskCreatePinnedToCore(&refresh_ALL_ONCE_Task, "ALL_ONCE",
			LEDARRAY_REFRESH_TASK_STACK_SIZE, NULL, LEDARRAY_REFRESH_TASK_PRIO, NULL, LEDARRAY_REFRESH_TASK_CORE);
		break;
	case LEDARRAY_AUTO_REFRESH_ALL_MULTIPLE:
		xTaskCreatePinnedToCore(&refresh_ALL_MULTIPLE_Task, "ALL_MULTIPLE",
			LEDARRAY_REFRESH_TASK_STACK_SIZE, NULL, LEDARRAY_REFRESH_TASK_PRIO, NULL, LEDARRAY_REFRESH_TASK_CORE);
		break;
	case LEDARRAY_AUTO_REFRESH_PART_ONCE:
		xTaskCreatePinnedToCore(&refresh_PART_ONCE_Task, "PART_ONCE",
			LEDARRAY_REFRESH_TASK_STACK_SIZE, NULL, LEDARRAY_REFRESH_TASK_PRIO, NULL, LEDARRAY_REFRESH_TASK_CORE);
		break;
	case LEDARRAY_AUTO_REFRESH_PART_MULTIPLE:
		xTaskCreatePinnedToCore(&refresh_PART_MULTIPLE_Task, "PART_MULTIPLE",
			LEDARRAY_REFRESH_TASK_STACK_SIZE, NULL, LEDARRAY_REFRESH_TASK_PRIO, NULL, LEDARRAY_REFRESH_TASK_CORE);
		break;
	default:
		break;
	}

	refresh_mode_buf = mode;
}

/*******************************************************软件图像生成函数**********************************************************/
/// @brief 生成一个矩形字模(需要释放)
/// @param breadth 矩形横向长度(1-LINE_LED_NUMBER)
/// @param length  矩形纵向长度(1-VERTICAL_LED_NUMBER)
/// @return 返回值 rectangle_data 为矩形数据地址 *rectangle_data 为 总数据大小(uint64_t)(单位:Byte) RECTANGLE_MATRIX(rectangle_data) 为 矩形字模
/// @return 例 返回值为p separation_draw(x,y,b,RECTANGLE_MATRIX(p),*p,color,change); free(p);
uint8_t* rectangle(int32_t breadth, int32_t length)
{
	if (breadth < 0 || length < 0)
		return NULL;

	uint64_t x_byte_num = 1, entire_byte_num = 1; // 横向字节个数，总数据有效字节个数（不包含entire_byte_num段）
	uint64_t Dx = 0;// 临时存储一下横向偏移长度，这只是用于计算。纵向偏移长度由绘制函数获取，不需要,
	bool flag = 0;// 即将写入的位数据值

	static uint8_t* pT1 = NULL;
	static uint8_t* p = NULL;

	// 进一法，最后不足8个点就补满8位。
	// 因为ceil传入的是浮点数，全部提前转换，防止整型相除而向下取整，否则ceil在这里就没意义了
	x_byte_num = ceil(breadth * 1.0 / 8.0);
	entire_byte_num = sizeof(uint8_t) * x_byte_num * length;

	uint8_t* rectangle_data = (uint8_t*)malloc(RECTANGLE_SIZE_MAX * sizeof(uint8_t) + 8);//8个字节(uint64_t)用于存储字模数据大小
	memset(rectangle_data, 0, RECTANGLE_SIZE_MAX * sizeof(uint8_t) + 8);

	*rectangle_data = entire_byte_num; // 装载entire_byte_num
	p = rectangle_data;
	pT1 = rectangle_data + 0x08; // 获取到数据的起始地址

	// 先进行全图填充
	flag = 1;
	for (uint8_t i = 0; i < entire_byte_num; i++)
	{
		if (pT1 - p > entire_byte_num)
		{
			ESP_LOGE("rectangle", "计算总数据大小出错");
			return NULL; // 越界退出
		}
		for (uint8_t j = 0; j < 8; j++)
		{
			*pT1 |= flag << (7 - j); // 写入
			if (Dx == breadth - 1)
			{
				Dx = 0;
				j = 8; // 一行写完强制退出，写下一个，实际就是回车，因为下一个字节就是下一行的了
			}
			else
				Dx++;
		}
		pT1++; // 地址偏移
	}
	return rectangle_data;
}


/*******************************************************基本绘制函数**********************************************************/
/// @brief 三色分离方式 取模适配PCtoLCD2002
/// @brief 取模说明：从第一行开始向右每取8个点作为一个字节，如果最后不足8个点就补满8位。
/// @brief 取模顺序是从高到低，即第一个点作为最高位。如*-------取为10000000
/// @brief RGB三色分离方式绘制,只支持单色绘制,之后ledarray_set_and_write需要被调用才可显示
/// @brief 取模方式请参考头文件
/// @param x 图案横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 图案纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param breadth 图案宽度（已定义的：FIGURE-数字 LETTER-字母 CHINESE-汉字）
/// @param p       导入字模指针
/// @param byte_number 总数据长度(Byte)
/// @param in_color 注入颜色 （RGB）
/// @param change   亮度调整（1-100）警告:过高的调整幅度可能导致色彩失真
void separation_draw(int x, int y, uint64_t breadth, const uint8_t* p, uint8_t byte_number, uint8_t in_color[3], uint8_t change)
{
	if (p == NULL)
	{
		ESP_LOGE("separation_draw", "输入了无法处理的空指针");
		return;
	}

	if (xSemaphoreTake(refresh_Task_Mutex, portMAX_DELAY)) {
		uint64_t Dx = 0, Dy = 0; // xy的增加量
		uint8_t data = 0x00;		// 临时数据存储
		uint8_t i = 0;			// 临时变量i
		int sx = 0;			// 临时存储选定的横坐标
		bool flag = 0;			// 该像素是否需要点亮
		uint8_t black[3] = { 0 };
		uint8_t color[3] = { in_color[0], in_color[1], in_color[2] };			// 因为数组本质也是指针，所以下级改动，上级数据也会破坏，所以需要隔离
		ledarray_intensity_change(&color[0], &color[1], &color[2], change); // 亮度调制

		p--; // 地址初始补偿
		while (byte_number)
		{
			p++; // 地址被动偏移
			for (i = 0; i <= 7; i++)
			{
				// 数据解析
				data = *p;				  // 读取数据
				flag = (data << i) & 0x80; // 位移取出一个bit数据，flag显示了选定的像素要不要点亮

				// 存储到缓冲区
				sx = x + Dx - 1;

				if (flag)
					color_input(sx, y + Dy, color);
				else
					color_input(sx, y + Dy, black);

				if (Dx == breadth - 1)
				{
					Dx = 0; // 横向写入最后一个像素完毕，回车
					Dy++;	// 横向写入最后一个像素完毕，回车
					i = 8;	// 横向写入最后一个像素完毕，强制退出，等待地址偏移
				}
				else
					Dx++; // 确定写入完成一个像素
			}
			byte_number--; // 一个字节写入完成
		}
		xSemaphoreGive(refresh_Task_Mutex);
	}
}

/// @brief 彩色图像直显方式 取模方式适配Img2Lcd
/// @brief [水平扫描，从左到右，从顶到底扫描，24位真彩（RGB顺序），需要图像数据头]
/// @brief 将自动获取图像头参数,之后ledarray_set_and_write需要被调用才可显示
/// @param x 图案横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 图案纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param p        导入图像指针
/// @param change   亮度调整（1-100）警告:过高的调整幅度可能导致色彩失真
void direct_draw(int x, int y, const uint8_t* p, uint8_t change)
{
	if (p == NULL)
	{
		ESP_LOGE("direct_draw", "输入了无法处理的空指针");
		return;
	}
	if (xSemaphoreTake(refresh_Task_Mutex, portMAX_DELAY)) {
		uint64_t Dx = 0, Dy = 0;				  // xy的增加量
		int sx = 0;						  // 临时存储选定的横坐标	
		uint8_t* pT1 = p, * pT2 = p, * pT3 = p; // 临时指针

		// 获取图案长宽数据
		uint64_t length = 0, breadth = 0; // 长宽信息
		uint8_t data[4] = { 0x00 };		 // 临时数据存储
		p += 0x02;						 // 偏移到长宽数据区
		for (uint8_t i = 0; i < 4; i++)
		{
			data[i] = *p;
			p++;
		}
		breadth = (data[1] << 8) | data[0];
		length = (data[3] << 8) | data[2];
		// 图像解析
		p += 0x02;				   // 偏移到图像数据区
		uint8_t color[3] = { 0x00 }; // 临时数据存储
		while (length)
		{
			// 获取颜色数据
			pT1 = p;
			pT2 = p + 0x01;
			pT3 = p + 0x02;
			color[0] = *pT1;
			color[1] = *pT2;
			color[2] = *pT3;

			ledarray_intensity_change(&color[0], &color[1], &color[2], change);

			sx = x + Dx - 1;
			color_input(sx, y + Dy, color);

			if (Dx == breadth - 1)
			{
				Dx = 0; // 横向写入最后一个像素完毕，回车
				Dy++;	// 横向写入最后一个像素完毕，回车
				length--;
			}
			else
				Dx++;
			p += 0x03; // 地址被动偏移
		}
		xSemaphoreGive(refresh_Task_Mutex);
	}
}

/*******************************************************图像操作绘制函数**********************************************************/
/// @brief 清除屏幕上的所有图案以及数据缓存
void clean_draw() {
	for (int i = 0; i <= VERTICAL_LED_NUMBER / 2 - 1; i++)
	{
		clean_draw_buf(i * 2 + 1);
		clean_draw_buf(i * 2 + 2);
		ledarray_set_and_write(i);
	}
}

/// @brief 清空指定行的图像
/// @param y 指定行纵坐标(从1开始)
void clean_draw_buf(int y)
{
	uint8_t data[3] = { 0 };
	for (int i = 0; i < LINE_LED_NUMBER; i++)
		color_input(i, y, data);
}

/// @brief 渐进指定行的图像，使得颜色向目标颜色以步进值偏移一步，这只会对绘制状态非活动的区域起作用
/// @param y 指定行纵坐标
/// @param step 步进值
/// @param color 目标颜色
void progress_draw_buf(int y, uint8_t step, uint8_t* color)
{
	uint8_t data[3] = { 0 };
	for (int i = 0; i < LINE_LED_NUMBER; i++)
	{
		color_output(i, y, data);
		for (int j = 0; j < 3; j++)
		{
			if (color[j] >= 0 && color[j] <= 255)
			{
				if (data[j] < color[j])
				{
					if (255 - data[j] >= step)
						data[j] += step;
					else
						data[j] = color[j];
				}
				if (data[j] > color[j])
				{
					if (data[j] >= step)
						data[j] -= step;
					else
						data[j] = color[j];
				}
			}
			else
			{
				data[j] = color[j];
			}
		}
		color_input(i, y, data);
	}
}

/*******************************************************扩展绘制函数**********************************************************/

/// @brief 打印一个数字(软件字模)
/// @param x 起始坐标x
/// @param y 起始坐标x
/// @param figure 输入整型0-9数字,不支持负数
/// @param color 颜色RGB
/// @param change 亮度0-100
void print_number(int x, int y, int8_t figure, uint8_t color[3], uint8_t change)
{
	uint8_t* p = NULL;
	// 将p指向对应数字字模
	switch (figure)
	{
	case 0:
		p = matrix_0;
		break;

	case 1:
		p = matrix_1;
		break;

	case 2:
		p = matrix_2;
		break;

	case 3:
		p = matrix_3;
		break;

	case 4:
		p = matrix_4;
		break;

	case 5:
		p = matrix_5;
		break;

	case 6:
		p = matrix_6;
		break;

	case 7:
		p = matrix_7;
		break;

	case 8:
		p = matrix_8;
		break;

	case 9:
		p = matrix_9;
		break;
	}
	if (p == NULL)
	{
		ESP_LOGE("print_number", "输入了0-9之外的数字");
		return;
	}
	separation_draw(x, y, FIGURE_BREATH, p, sizeof(matrix_7), color, change); // 因为，数字字模数据大小一样，随便输入一个字模就可以
}


/// @brief 通过字库芯片支持在LED阵列打印任意字符,图像不含运动效果
/// @param x 图案横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 图案纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param color 字符颜色
/// @param change 亮度调制 1-100
/// @param format 形式同printf的可变参量表
void font_raw_print_12x(int x, int y, uint8_t color[3], uint8_t change, char* format, ...) {
	const char* TAG = "font_raw_print_12x";

	//申请字符unicode编码缓存
	uint32_t* buf_unicode = NULL;
	buf_unicode = (uint32_t*)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	while (!buf_unicode)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请buf_unicode资源发现问题 正在重试");
		buf_unicode = (uint32_t*)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	}
	memset(buf_unicode, 0, FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));

	//申请UTF-8编码缓存
	char* str_buf = NULL;
	str_buf = (char*)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	while (!str_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请str_buf资源发现问题 正在重试");
		str_buf = (char*)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	}
	memset(str_buf, 0, FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));

	//格式化源字符串(UTF-8编码数据)到UTF-8编码缓存
	va_list ap;
	va_start(ap, format);
	vsnprintf(str_buf, FONT_CHIP_PRINT_FMT_BUF_SIZE, format, ap);

	//获取所有要显示字符的Unicode,以及字符总个数
	uint32_t total_unit = UTF8_Unicode_get(str_buf, buf_unicode, FONT_CHIP_PRINT_NUM_MAX);

	//申请字符点阵数据缓存
	uint8_t* font_buf = NULL;
	font_buf = (uint8_t*)malloc(total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));
	while (!font_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请font_buf资源发现问题 正在重试");
		font_buf = (uint8_t*)malloc(total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));
	}
	memset(font_buf, 0, total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));

	int idx = 0;//选定操作的为[idx]号字符
	uint32_t ASCII_num = 0;//总共含有的ASCII字符个数

	//从字库读取所有字符的点阵数据到font_buf
	for (idx = 0;idx < total_unit;idx++) {
		if (buf_unicode[idx] >= 128) {
			fonts_read_zh_CN_12x(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES]);//读取汉字字符 宽度12
		}
		else {//Unicode小于128兼容ASCII字符集
			fonts_read_ASCII_6x12(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES]);//读取ASCII字符 宽度6
			ASCII_num++;
		}
	}

	int x_buf = 0;//当前选定的[idx]号字符点阵图像的起始x轴坐标
	int x_base = 0;//当前选定的[idx]号字符坐标点(字模点阵左上角)与第一个字符即idx=0的水平点阵距离,这在计算[idx-1]号字符时完成累加

	//绘制所有字符
	for (idx = 0;idx < total_unit;idx++) {
		x_buf = x + x_base;//获取当前选定的[idx]号字符点阵图像的起始x轴坐标
		if (buf_unicode[idx] >= 128) {
			if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
				separation_draw(x_buf, y, 12, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ZH_CN_12X_BYTES, color, change);
			x_base += 12;
		}
		else {//Unicode小于128兼容ASCII字符集
			if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
				separation_draw(x_buf, y, 6, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ASCII_6X12_BYTES, color, change);
			x_base += 6;
		}
	}
	//释放所有缓存
	free(buf_unicode);
	buf_unicode = NULL;
	free(str_buf);
	str_buf = NULL;
	free(font_buf);
	font_buf = NULL;
}

/// @brief 通过字库芯片支持在LED阵列滚动打印任意字符
/// @param x 初始横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 初始纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param color 字符颜色
/// @param change 亮度调制 1-100
/// @param cartoon_handle sevetest30_UI动画支持句柄,填写句柄启用预设的动画,填写NULL以使用默认效果
/// @param format 形式同printf的可变参量表
void font_roll_print_12x(int x, int y, uint8_t color[3], uint8_t change, cartoon_handle_t cartoon_handle, char* format, ...) {
	const char* TAG = "font_roll_print_12x";

	//申请字符unicode编码缓存
	uint32_t* buf_unicode = NULL;
	buf_unicode = (uint32_t*)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	while (!buf_unicode)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请buf_unicode资源发现问题 正在重试");
		buf_unicode = (uint32_t*)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	}
	memset(buf_unicode, 0, FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));

	//申请UTF-8编码缓存
	char* str_buf = NULL;
	str_buf = (char*)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	while (!str_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请str_buf资源发现问题 正在重试");
		str_buf = (char*)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	}
	memset(str_buf, 0, FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));

	//格式化源字符串(UTF-8编码数据)到UTF-8编码缓存
	va_list ap;
	va_start(ap, format);
	vsnprintf(str_buf, FONT_CHIP_PRINT_FMT_BUF_SIZE, format, ap);

	//获取所有要显示字符的Unicode,以及字符总个数
	uint32_t total_unit = UTF8_Unicode_get(str_buf, buf_unicode, FONT_CHIP_PRINT_NUM_MAX);

	//申请字符点阵数据缓存
	uint8_t* font_buf = NULL;
	font_buf = (uint8_t*)malloc(total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));
	while (!font_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请font_buf资源发现问题 正在重试");
		font_buf = (uint8_t*)malloc(total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));
	}
	memset(font_buf, 0, total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));

	int idx = 0;//选定操作的为[idx]号字符
	uint32_t ASCII_num = 0;//总共含有的ASCII字符个数

	//从字库读取所有字符的点阵数据到font_buf
	for (idx = 0;idx < total_unit;idx++) {
		if (buf_unicode[idx] >= 128) {
			fonts_read_zh_CN_12x(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES]);//读取汉字字符 宽度12
		}
		else {//Unicode小于128兼容ASCII字符集
			fonts_read_ASCII_6x12(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES]);//读取ASCII字符 宽度6
			ASCII_num++;
		}
	}

	//绘制图像形成滚动效果
	uint32_t step = 0;//当前步进值
	int x_buf = 0;//当前选定的[idx]号字符点阵图像的起始x轴坐标
	int x_base = 0;//当前选定的[idx]号字符坐标点(字模点阵左上角)与第一个字符即idx=0的水平步数距离,这在计算[idx-1]号字符时完成累加

	//如果把要滚动的字符看做一列火车车厢,屏幕看作一条小于车长的直隧洞
	//那么隧洞有车厢存在的时间,为车头进入一刻,直到车尾离开一刻,这段时间移动距离为隧洞和车厢总长和
	//因此这里,滚动总长度为字符链长+屏幕长,由于可显示的最小移动为一个像素点的偏移,将这个偏移称为1步,长度使用对应步数来标识
	//因为每个字符将发生的位移一致,使用变量step作为所有字符的当前向左偏移步数,由于偏移方向与规定的屏幕x轴正方向(向右)相反,在坐标偏移运算中作减法
	//从而,x_buf的值为对应字符([idx]号字符)在运动未开始时的初始x坐标,再减去step,过程中,step将由0累加到字符链长+屏幕长

	//第1种方式 - 使用默认动画绘制
	if (!cartoon_handle) {
		for (step = 0;step < ASCII_num * 6 + (total_unit - ASCII_num) * 12 + LINE_LED_NUMBER;step++) {
			//在当前step偏移下刷新一帧图像
			for (idx = 0;idx < total_unit;idx++) {
				x_buf = (x - 1) + LINE_LED_NUMBER + x_base - step;//获取当前选定的[idx]号字符点阵图像的起始x轴坐标(x-1为初始坐标的绝对偏移坐标)
				//仅对可视范围内字符进行绘制

				if (buf_unicode[idx] >= 128) {
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, y, 12, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ZH_CN_12X_BYTES, color, change);
					x_base += 12;
				}
				else {//Unicode小于128兼容ASCII字符集
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, y, 6, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ASCII_6X12_BYTES, color, change);
					x_base += 6;
				}

			}
			vTaskDelay(pdMS_TO_TICKS(50));
			x_base = 0;//重置字符间隔偏移缓存
		}
	}

	//第2种方式 - 运行sevetest30_UI提供的动画支持服务
	if (cartoon_handle) {
		cartoon_handle->create_callback(cartoon_handle,
			ASCII_num * 6 + (total_unit - ASCII_num) * 12 + LINE_LED_NUMBER);//生成动画
		//创建控制对象
		int32_t cx = x;//需要控制的x轴坐标数据,hook函数只写     
		int32_t cy = y;//需要控制的y轴坐标数据,hook函数只写 
		uint8_t ccolor[3] = { color[0],color[1],color[2] };//需要控制的颜色数据,hook函数只写 
		uint8_t cchange = change;//需要控制的亮度数据位置,hook函数只写 
		cartoon_ctrl_object_t object = {
			.pstep = &step,
			.px = &cx,
			.py = &cy,
			.pcolor = ccolor,
			.pchange = &cchange,
		};
		for (step = 0;step < ASCII_num * 6 + (total_unit - ASCII_num) * 12 + LINE_LED_NUMBER;step++) {
			//调用钩子函数调整控制对象
			cartoon_handle->ctrl_hook(cartoon_handle, &object);
			for (idx = 0;idx < total_unit;idx++) {
				x_buf = (x - 1) + (cx - 1) + LINE_LED_NUMBER + x_base;//获取当前选定的[idx]号字符点阵图像的起始x轴坐标(x-1 cx-1为绝对偏移坐标)
				//仅对可视范围内字符进行绘制
				if (buf_unicode[idx] >= 128) {
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, cy + (y - 1), 12, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ZH_CN_12X_BYTES, ccolor, cchange);
					x_base += 12;
				}
				else {//Unicode小于128兼容ASCII字符集
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, cy + (y - 1), 6, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ASCII_6X12_BYTES, ccolor, cchange);
					x_base += 6;
				}
			}
			vTaskDelay(pdMS_TO_TICKS(50));
			x_base = 0;//重置字符间隔偏移缓存
		}
	}





	//释放所有缓存
	free(buf_unicode);
	buf_unicode = NULL;
	free(str_buf);
	str_buf = NULL;
	free(font_buf);
	font_buf = NULL;
}

/*******************************************************显示驱动函数**********************************************************/

/// @brief  初始化灯板阵列
/// @return [ESP_OK 成功]
/// @return [ESP_FAIL 创建refresh_Task_Mutex互斥量时发现问题 / refresh_Task_Mutex互斥量已经被意外创建]
/// @return [ESP_ERR_INVALID_STATE 灯板阵列之前已经初始化,运行ledarray_deinit以去初始化 / RMT控制器之前已经安装,请调用对应rmt_driver_uninstall释放需要的资源]
/// @return [ESP_ERR_INVALID_ARG 参数错误]
/// @return [ESP_ERR_NO_MEM 内存不足]
esp_err_t ledarray_init()
{
	const char* TAG = "ledarray_init";

	if (!refresh_Task_Mutex) {
		refresh_Task_Mutex = xSemaphoreCreateMutex();
		if (!refresh_Task_Mutex) {
			ESP_LOGE(TAG, "创建refresh_Task_Mutex互斥量时发现问题");
			return ESP_FAIL;
		}
	}
	else {
		if (strip0 != NULL || strip1 != NULL) {
			ESP_LOGE(TAG, "灯板阵列不可重复初始化,运行ledarray_deinit以去初始化");
			return ESP_ERR_INVALID_STATE;
		}
		else {
			ESP_LOGE(TAG, "refresh_Task_Mutex互斥量已经被意外创建");
			return ESP_FAIL;
		}
	}

	led_strip_config_t strip_config_0 = {
		.strip_gpio_num = ledarray_gpio_num_list[0],
		.max_leds = LINE_LED_NUMBER,
		.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
		.led_model = LEDARRAY_LED_MODEL,
		.flags.invert_out = LEDARRAY_LED_INVERT_OUT,
	};

	led_strip_config_t strip_config_1 = {
		.strip_gpio_num = ledarray_gpio_num_list[1],
		.max_leds = LINE_LED_NUMBER,
		.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
		.led_model = LEDARRAY_LED_MODEL,
		.flags.invert_out = LEDARRAY_LED_INVERT_OUT,
	};

	led_strip_rmt_config_t rmt_config = {
		.clk_src = LEDARRAY_LED_RMT_CLK_SRC,
		.resolution_hz = LEDARRAY_LED_RMT_RESOLUTION_HZ,
		.mem_block_symbols = LEDARRAY_LED_RMT_MEM_BLOCK_SYMBOLS,
		.flags.with_dma = LEDARRAY_LED_WITH_DMA,
	};

	esp_err_t ret = ESP_OK;

	//创建通过RMT通讯的实例
	ret = led_strip_new_rmt_device(&strip_config_0, &rmt_config, &strip0);
	ESP_RETURN_ON_ERROR(ret, TAG, "创建 strip0 时发现问题 描述 %s", esp_err_to_name(ret));
	ret = led_strip_new_rmt_device(&strip_config_1, &rmt_config, &strip1);
	ESP_RETURN_ON_ERROR(ret, TAG, "创建 strip1 时发现问题 描述 %s", esp_err_to_name(ret));

	//通过偏移强制访问内部封装资源(不推荐)
	strip0_rmt_channel = (rmt_channel_handle_t)((void*)strip0 + sizeof(led_strip_t) / sizeof(void*));
	strip1_rmt_channel = (rmt_channel_handle_t)((void*)strip0 + sizeof(led_strip_t) / sizeof(void*));

	//设置自动刷新模式为默认模式
	ledarray_set_auto_refresh_mode(LEDARRAY_REFRESH_INIT_MODE);

	ESP_LOGW(TAG, " %d X %d LED阵列初始化完成 当前自动刷新服务模式 %d", LINE_LED_NUMBER, VERTICAL_LED_NUMBER, LEDARRAY_REFRESH_INIT_MODE);

	return ESP_OK;
}

/// @brief  灯板阵列反初始化操作
/// @return [ESP_OK 成功]
/// @return [ESP_FAIL 释放资源失败 / refresh_Task_Mutex互斥量异常]
/// @return [ESP_ERR_INVALID_STATE 灯板阵列未初始化,无需反初始化]
/// @return [ESP_ERR_INVALID_ARG ledarray_gpio_num_list中存在错误的GPIO号码]
esp_err_t ledarray_deinit()
{
	const char* TAG = "ledarray_deinit";

	if (!refresh_Task_Mutex) {
		if (strip0 != NULL || strip1 != NULL) {
			ESP_LOGE(TAG, "灯板阵列未初始化,无需反初始化");
			return ESP_ERR_INVALID_STATE;
		}
		else {
			ESP_LOGE(TAG, "refresh_Task_Mutex互斥量异常");
			return ESP_FAIL;
		}
	}

	esp_err_t ret = ESP_OK;

	//禁用刷新服务
	ledarray_set_auto_refresh_mode(LEDARRAY_AUTO_REFRESH_DISABLE);

	//禁用访问
	strip0_rmt_channel = NULL;
	strip1_rmt_channel = NULL;

	//释放strip0资源
	ret = led_strip_del(strip0);
	ESP_RETURN_ON_ERROR(ret, TAG, "释放 strip0资源 时发现问题 描述 %s", esp_err_to_name(ret));
	strip0 = NULL;

	//释放strip1资源
	ret = led_strip_del(strip1);
	ESP_RETURN_ON_ERROR(ret, TAG, "释放 strip1资源 时发现问题 描述 %s", esp_err_to_name(ret));
	strip1 = NULL;

	//重置互斥量
	vSemaphoreDelete(refresh_Task_Mutex);
	refresh_Task_Mutex = NULL;

	//确保不再有信号输出
	for (int i = 0;i < VERTICAL_LED_NUMBER;i++) {
		esp_rom_gpio_pad_select_gpio(ledarray_gpio_num_list[i]);
		ret = gpio_set_direction(ledarray_gpio_num_list[i], GPIO_MODE_INPUT);
		ESP_RETURN_ON_ERROR(ret, TAG, "错误的GPIO号码");
		gpio_set_level(ledarray_gpio_num_list[i], 0);
	}

	ESP_LOGW(TAG, " %d X %d LED阵列反初始化操作完成,所有资源已释放 当前自动刷新服务模式 %d", LINE_LED_NUMBER, VERTICAL_LED_NUMBER, LEDARRAY_AUTO_REFRESH_DISABLE);
	return ESP_OK;
}

/// @brief 灯板阵列选定并写入，未通过ledarray_init()初始化ledarray，函数内会自动初始化
/// @param group_num 选定要刷新的组,每组有两串WS2812,如12行WS2812,共6组,取值为0-5,不足一组单独按一组计算
void ledarray_set_and_write(int group_num)
{
	const char* TAG = "ledarray_set_and_write";

	if (group_num > VERTICAL_LED_NUMBER / 2 - 1)
	{
		return; // 不在显示范围退出即可，允许在范围外但不报告
	}
	if (strip0 == NULL || strip1 == NULL)
	{
		ESP_LOGE(TAG, "LED阵列未初始化");
		return;
	}

	if (xSemaphoreTake(refresh_Task_Mutex, portMAX_DELAY)) {		//竞争互斥锁
		// 记录了上次调用函数刷新的组的输出IO
		static gpio_num_t former_select0 = ledarray_gpio_num_list[0];
		static gpio_num_t former_select1 = ledarray_gpio_num_list[1];

		rmt_tx_channel_config_t strip0_channel_config = {
			.gpio_num = ledarray_gpio_num_list[group_num * 2 + 0],
			.clk_src = LEDARRAY_LED_RMT_CLK_SRC,
			.resolution_hz = LEDARRAY_LED_RMT_RESOLUTION_HZ,
			.mem_block_symbols = LEDARRAY_LED_RMT_MEM_BLOCK_SYMBOLS,
			.trans_queue_depth = 4,
			.flags.with_dma = LEDARRAY_LED_WITH_DMA,
			.flags.invert_out = LEDARRAY_LED_INVERT_OUT,
		};

		rmt_tx_channel_config_t strip1_channel_config = {
			.gpio_num = ledarray_gpio_num_list[group_num * 2 + 1],
			.clk_src = LEDARRAY_LED_RMT_CLK_SRC,
			.resolution_hz = LEDARRAY_LED_RMT_RESOLUTION_HZ,
			.mem_block_symbols = LEDARRAY_LED_RMT_MEM_BLOCK_SYMBOLS,
			.trans_queue_depth = 4,
			.flags.with_dma = LEDARRAY_LED_WITH_DMA,
			.flags.invert_out = LEDARRAY_LED_INVERT_OUT,
		};

		esp_err_t err = ESP_OK;

		// strip0切换输出引脚
		err = ESP_OK;
		err |= rmt_tx_wait_all_done(strip0_rmt_channel, -1);//等待之前的传输完成
		err |= rmt_del_channel(strip0_rmt_channel);//卸载当前RMT通道
		err |= rmt_new_tx_channel(&strip0_channel_config, &strip0_rmt_channel);//安装新的RMT通道(切换到下一个GPIO)
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "strip0切换输出引脚时发现问题,用于错误,将暂时禁用自动刷新服务");
			ledarray_set_auto_refresh_mode(LEDARRAY_AUTO_REFRESH_DISABLE);
		}

		// strip1切换输出引脚
		err = ESP_OK;
		err |= rmt_tx_wait_all_done(strip1_rmt_channel, -1);//等待之前的传输完成
		err |= rmt_del_channel(strip1_rmt_channel);//卸载当前RMT通道
		err |= rmt_new_tx_channel(&strip1_channel_config, &strip1_rmt_channel);//安装新的RMT通道(切换到下一个GPIO)
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "strip1切换输出引脚时发现问题,用于错误,将暂时禁用自动刷新服务");
			ledarray_set_auto_refresh_mode(LEDARRAY_AUTO_REFRESH_DISABLE);
		}

		//写入strip0数据并刷新
		err = ESP_OK;
		color_compound(group_num * 2 + 1);// 合成strip0数据到缓存
		for (int j = 0; j < LINE_LED_NUMBER * 3; j += 3) {
			err |= led_strip_set_pixel(strip0, j / 3, compound_result[j + 1], compound_result[j + 0], compound_result[j + 2]); // 设置即将刷新的数据
		}
		err |= led_strip_refresh(strip0); // 对现在绑定的IO写入数据
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "写入并刷新strip0时发现问题,用于错误,将暂时禁用自动刷新服务");
			ledarray_set_auto_refresh_mode(LEDARRAY_AUTO_REFRESH_DISABLE);
		}

		//写入strip1数据并刷新
		err = ESP_OK;
		color_compound(group_num * 2 + 2);// 合成strip1数据到缓存
		for (int j = 0; j < LINE_LED_NUMBER * 3; j += 3) {
			err |= led_strip_set_pixel(strip1, j / 3, compound_result[j + 1], compound_result[j + 0], compound_result[j + 2]); // 设置即将刷新的数据
		}
		err |= led_strip_refresh(strip1); // 对现在绑定的IO写入数据
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "写入并刷新strip1时发现问题,用于错误,将暂时禁用自动刷新服务");
			ledarray_set_auto_refresh_mode(LEDARRAY_AUTO_REFRESH_DISABLE);
		}

		//存储本次操作的输出GPIO
		former_select0 = ledarray_gpio_num_list[group_num * 2 + 0];
		former_select1 = ledarray_gpio_num_list[group_num * 2 + 1];

		//释放互斥锁
		xSemaphoreGive(refresh_Task_Mutex);
	}
}

/// @brief 颜色导入,SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必修改这个函数
/// @param x 绝对横坐标(0 到 LINE_LED_NUMBER-1)
/// @param y 一般纵坐标(1 到 VERTICAL_LED_NUMBER)
/// @param data 导入的颜色RGB数据位置
void color_input(int x, int y, uint8_t* data)
{

	if (x < 0 || x > LINE_LED_NUMBER - 1 || y < 1 || y > VERTICAL_LED_NUMBER) {
		return; // 不在显示范围退出即可，允许在范围外但不报告
	}

	if (draw_line_count[y - 1] < 255)
		draw_line_count[y - 1]++;//该行发生绘制活动,计数值增加

	switch (y)
	{
	case 1:
		red_y1[x] = data[0];
		green_y1[x] = data[1];
		blue_y1[x] = data[2];
		break;

	case 2:
		red_y2[x] = data[0];
		green_y2[x] = data[1];
		blue_y2[x] = data[2];
		break;

	case 3:
		red_y3[x] = data[0];
		green_y3[x] = data[1];
		blue_y3[x] = data[2];
		break;

	case 4:
		red_y4[x] = data[0];
		green_y4[x] = data[1];
		blue_y4[x] = data[2];
		break;

	case 5:
		red_y5[x] = data[0];
		green_y5[x] = data[1];
		blue_y5[x] = data[2];
		break;

	case 6:
		red_y6[x] = data[0];
		green_y6[x] = data[1];
		blue_y6[x] = data[2];
		break;

	case 7:
		red_y7[x] = data[0];
		green_y7[x] = data[1];
		blue_y7[x] = data[2];
		break;

	case 8:
		red_y8[x] = data[0];
		green_y8[x] = data[1];
		blue_y8[x] = data[2];
		break;

	case 9:
		red_y9[x] = data[0];
		green_y9[x] = data[1];
		blue_y9[x] = data[2];
		break;

	case 10:
		red_y10[x] = data[0];
		green_y10[x] = data[1];
		blue_y10[x] = data[2];
		break;

	case 11:
		red_y11[x] = data[0];
		green_y11[x] = data[1];
		blue_y11[x] = data[2];
		break;

	case 12:
		red_y12[x] = data[0];
		green_y12[x] = data[1];
		blue_y12[x] = data[2];
		break;

	default:
		break;
	}
}

/// @brief 颜色导出,SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必修改这个函数
/// @param x 绝对横坐标(0 到 LINE_LED_NUMBER-1)
/// @param y 一般纵坐标(1 到 LINE_LED_NUMBER)
/// @param data 导出存储的颜色RGB数据的位置
void color_output(int x, int y, uint8_t* data)
{
	if (!data) {
		ESP_LOGE("color_output", "输入了无法处理的空指针");
		return;
	}
	if (x < 0 || x > LINE_LED_NUMBER - 1 || y < 1 || y > VERTICAL_LED_NUMBER)
		return; // 不在显示范围退出即可，允许在范围外但不报告
	switch (y)
	{
	case 1:
		data[0] = red_y1[x];
		data[1] = green_y1[x];
		data[2] = blue_y1[x];
		break;

	case 2:
		data[0] = red_y2[x];
		data[1] = green_y2[x];
		data[2] = blue_y2[x];
		break;

	case 3:
		data[0] = red_y3[x];
		data[1] = green_y3[x];
		data[2] = blue_y3[x];
		break;

	case 4:
		data[0] = red_y4[x];
		data[1] = green_y4[x];
		data[2] = blue_y4[x];
		break;

	case 5:
		data[0] = red_y5[x];
		data[1] = green_y5[x];
		data[2] = blue_y5[x];
		break;

	case 6:
		data[0] = red_y6[x];
		data[1] = green_y6[x];
		data[2] = blue_y6[x];
		break;

	case 7:
		data[0] = red_y7[x];
		data[1] = green_y7[x];
		data[2] = blue_y7[x];
		break;

	case 8:
		data[0] = red_y8[x];
		data[1] = green_y8[x];
		data[2] = blue_y8[x];
		break;

	case 9:
		data[0] = red_y9[x];
		data[1] = green_y9[x];
		data[2] = blue_y9[x];
		break;

	case 10:
		data[0] = red_y10[x];
		data[1] = green_y10[x];
		data[2] = blue_y10[x];
		break;

	case 11:
		data[0] = red_y11[x];
		data[1] = green_y11[x];
		data[2] = blue_y11[x];
		break;

	case 12:
		data[0] = red_y12[x];
		data[1] = green_y12[x];
		data[2] = blue_y12[x];
		break;

	default:
		break;
	}
}


/// @brief 颜色数据合成,将R red_y(x) G green_y(x) B blue_y(x) 合成为 为WS2812发送的数据
/// @brief SE30中VERTICAL_LED_NUMBER=12 如果您希望扩展屏幕纵向长度，务必修改这个函数
/// @param line_num 合成选定的行(1-VERTICAL_LED_NUMBER)
void color_compound(int line_num)
{
	uint8_t i = 0;
	// 初始化
	uint8_t* red = NULL;
	uint8_t* green = NULL;
	uint8_t* blue = NULL;
	switch (line_num)
	{
	case 1:
		red = red_y1;
		green = green_y1;
		blue = blue_y1;
		break;
	case 2:
		red = red_y2;
		green = green_y2;
		blue = blue_y2;
		break;
	case 3:
		red = red_y3;
		green = green_y3;
		blue = blue_y3;
		break;
	case 4:
		red = red_y4;
		green = green_y4;
		blue = blue_y4;
		break;
	case 5:
		red = red_y5;
		green = green_y5;
		blue = blue_y5;
		break;
	case 6:
		red = red_y6;
		green = green_y6;
		blue = blue_y6;
		break;
	case 7:
		red = red_y7;
		green = green_y7;
		blue = blue_y7;
		break;
	case 8:
		red = red_y8;
		green = green_y8;
		blue = blue_y8;
		break;
	case 9:
		red = red_y9;
		green = green_y9;
		blue = blue_y9;
		break;
	case 10:
		red = red_y10;
		green = green_y10;
		blue = blue_y10;
		break;
	case 11:
		red = red_y11;
		green = green_y11;
		blue = blue_y11;
		break;
	case 12:
		red = red_y12;
		green = green_y12;
		blue = blue_y12;
		break;
	default:
		return;
		break;
	}

	draw_line_count[line_num - 1] = 0;//该行发生刷新活动,绘制计数值重置
	// 填充数据
	for (i = 0; i < LINE_LED_NUMBER; i++)
	{
		// GRB顺序
		compound_result[i * 3 + 0] = *green;
		compound_result[i * 3 + 1] = *red;
		compound_result[i * 3 + 2] = *blue;
		red++, green++, blue++; // 地址偏移
	}
}


/*******************************************************运算转换函数**********************************************************/
// RGB亮度调制  导入r g b数值地址+亮度
void ledarray_intensity_change(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t intensity)
{
	// 注意，RGB和HSV的取值范围并不一致，标准定义是 R G B 为 0-255  H 为 0-360 S V 为0-1（为了方便计算，这里 S V 映射到 0-100）

	if (*r == 0 && *g == 0 && *b == 0)
		return; //	倘若 r g b 三个分量值都是0，显然客观上不需要变换，增加明度只会干扰数值

	if (intensity > 100)
	{
		ESP_LOGE("ledarray_intensity_change", "错误的亮度数值 %d", intensity);
		*r = 0x00;
		*g = 0x00;
		*b = 0x00;
		return;
	}
	uint32_t h = 0, s = 0, v = 0;
	rgb_to_hvs(*r, *g, *b, &h, &s, &v);
	v = intensity;
	led_strip_hsv2rgb(h, s, v, (uint32_t*)r, (uint32_t*)g, (uint32_t*)b);
}

// 取三个double元素最大的那个
double value_max(double value1, double value2, double value3)
{
	double buffer[3] = { value1, value2, value3 };
	uint8_t a = 0, b = 0;
	for (a = 0; a < 2; a++)
	{
		for (b = a + 1; b < 3; b++)
		{
			if (buffer[a] > buffer[b])
			{
				double save = buffer[b];
				buffer[b] = buffer[a];
				buffer[a] = save;
			}
		}
	}
	return buffer[2];
}

// 取三个double元素最小的那个
double value_min(double value1, double value2, double value3)
{
	double buffer[3] = { value1, value2, value3 };
	uint8_t a = 0, b = 0;
	for (a = 0; a < 2; a++)
	{
		for (b = a + 1; b < 3; b++)
		{
			if (buffer[a] > buffer[b])
			{
				double save = buffer[b];
				buffer[b] = buffer[a];
				buffer[a] = save;
			}
		}
	}
	return buffer[0];
}

// 注意，RGB和HSV的取值范围并不一致，标准定义是 R G B 为 0-255  H 为 0-360 S V 为0-1
// 然而，为了方便计算，这里 S V 映射到 0-100
// 将RGB转换到HSV颜色空间,计算方法来自网络
void rgb_to_hvs(uint8_t red_buf, uint8_t green_buf, uint8_t blue_buf, uint32_t* p_h, uint32_t* p_s, uint32_t* p_v)
{
	// HSV需要浮点存储
	double h = 0, s = 0, v = 0;

	// 将RGB映射到0 - 1之间,并由浮点变量 r g b 存储
	double r = 0, g = 0, b = 0;
	r = red_buf / 255.0;
	g = green_buf / 255.0;
	b = blue_buf / 255.0;

	// 计算V
	v = value_max(r, g, b);

	// 计算S
	if (v != 0)
	{
		s = v - value_min(r, g, b);
		s = s / v;
	}
	else
		s = 0;

	// 计算H
	if (v == r)
		h = 60 * (g - b) / (v - value_min(r, g, b));
	if (v == g)
		h = 120 + 60 * (b - r) / (v - value_min(r, g, b));
	if (v == b)
		h = 240 + 60 * (r - g) / (v - value_min(r, g, b));

	if (h < 0)
		h = h + 360;

	// 映射到需求范围 0 - 100
	s = s * 100;
	v = v * 100;

	static uint32_t out_h = 0, out_s = 0, out_v = 0;
	// 类型转换，随便四舍五入一下
	out_h = ceil(h);
	out_s = ceil(s);
	out_v = ceil(v);
	// 数据输出
	*p_h = out_h;
	*p_s = out_s;
	*p_v = out_v;
}

// 以下函数来自ESP-IDFv4.4 led_strip.c 例程文件

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t* r, uint32_t* g, uint32_t* b)
{
	h %= 360; // h -> [0,360]
	uint32_t rgb_max = v * 2.55f;
	uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

	uint32_t i = h / 60;
	uint32_t diff = h % 60;

	// RGB adjustment amount by hue
	uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

	switch (i)
	{
	case 0:
		*r = rgb_max;
		*g = rgb_min + rgb_adj;
		*b = rgb_min;
		break;
	case 1:
		*r = rgb_max - rgb_adj;
		*g = rgb_max;
		*b = rgb_min;
		break;
	case 2:
		*r = rgb_min;
		*g = rgb_max;
		*b = rgb_min + rgb_adj;
		break;
	case 3:
		*r = rgb_min;
		*g = rgb_max - rgb_adj;
		*b = rgb_max;
		break;
	case 4:
		*r = rgb_min + rgb_adj;
		*g = rgb_min;
		*b = rgb_max;
		break;
	default:
		*r = rgb_max;
		*g = rgb_min;
		*b = rgb_max - rgb_adj;
		break;
	}
}
