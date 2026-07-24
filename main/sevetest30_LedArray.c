
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

// 包含ICND2038S+ICND2013构成的LED阵列的图形与显示处理,RGB彩色显示由软件实现
// ICND2038S硬件只能控制灯的开关,所以这里使用BCM调光算法实现0-255灰度RGB彩色显示
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 敬告：文件本体包含硬件驱动代码
// 绘制函数本身不会刷新屏幕,需要运行屏幕刷新,才会在屏幕上点亮
// 绘制函数规定使用字模点阵的左上角的点作为其坐标表示点,长边方向向右为X轴正方向,短边方向向下为Y轴正方向,
// 原点坐标为(1,1)，即如果要让图像显示在左上角应填入函数 x = 1,y = 1
// 将使用SPI外设+软件IO控制ICND2038S,软件IO控制ICND2013,来实现灯的亮灭
// BCM调光算法实现0-255灰度RGB彩色显示
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// github: https://github.com/701Enti
// bilibili: 701Enti

#include "sevetest30_LedArray.h"
#include "hal/gpio_types.h"
#include "sevetest30_UI.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "gt32l32s0140.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "esp_task_wdt.h"
#include "board.h"
#include "driver/gpio.h"

// 一个图像可看作不同颜色的像素组合，而每个像素颜色可用红绿蓝三元色的深度（亮度）表示
// 因此，我们可以将一个图像分离成三个单色图层，每个图层的每个像素的值表示该像素在该图层的亮度，这里用uint8_t表示(0-255)
// 所以这里申请三块连续的uint8_t内存空间，每块大小为 LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t)
uint8_t *ledarray_green_layer_buf = NULL;
uint8_t *ledarray_red_layer_buf = NULL;
uint8_t *ledarray_blue_layer_buf = NULL;

uint8_t *ledarray_tx_buf = NULL; // 数据发送缓存

spi_device_handle_t *ledarray_spi_handle = NULL;

bool is_initialized = false;

// 数字 0-9
const uint8_t matrix_0[7] = {0xF0, 0x90, 0x90, 0x90, 0x90, 0x90, 0xF0};
const uint8_t matrix_1[7] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
const uint8_t matrix_2[7] = {0xF0, 0x10, 0x10, 0xF0, 0x80, 0x80, 0xF0};
const uint8_t matrix_3[7] = {0xF0, 0x10, 0x10, 0xF0, 0x10, 0x10, 0xF0};
const uint8_t matrix_4[7] = {0x10, 0x30, 0x50, 0xF0, 0x10, 0x10, 0x10};
const uint8_t matrix_5[7] = {0xF0, 0x80, 0x80, 0xF0, 0x10, 0x10, 0xF0};
const uint8_t matrix_6[7] = {0xF0, 0x80, 0x80, 0xF0, 0x90, 0x90, 0xF0};
const uint8_t matrix_7[7] = {0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10};
const uint8_t matrix_8[7] = {0xF0, 0x90, 0x90, 0xF0, 0x90, 0x90, 0xF0};
const uint8_t matrix_9[7] = {0xF0, 0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0};

/******************************自动屏幕刷新服务 [绘制函数本身不会刷新屏幕,需要手动或自动运行屏幕刷新,才会在屏幕上点亮] *****************************/
ledarray_auto_refresh_mode_t refresh_mode_buf = LEDARRAY_AUTO_REFRESH_DISABLE;
SemaphoreHandle_t refresh_Task_Mutex = NULL;

/// @brief [单次全刷任务 - ALL_ONCE]一次性刷新整个屏幕所有行,全屏刷新之后才发生延时
void refresh_ALL_ONCE_Task()
{
	// 完全占用CPU,不会释放给其他任务
	esp_task_wdt_add(NULL); // 将当前任务挂载到当前设置核心的任务看门狗
	while (1)
	{
		esp_task_wdt_reset(); // 及时喂狗,防止当前核心重启
		ledarray_show_frame();
	}
}

/// @brief 设置LED阵列的自动刷新模式,立即启动生效
/// @param mode 刷新模式,这是一个枚举类型
void ledarray_set_auto_refresh_mode(ledarray_auto_refresh_mode_t mode)
{
	switch (refresh_mode_buf)
	{
	case LEDARRAY_AUTO_REFRESH_ALL_ONCE:
		vTaskDelete(xTaskGetHandle("LED_REFRESH_AO"));
		break;

	default:
		break;
	}

	switch (mode)
	{
	case LEDARRAY_AUTO_REFRESH_ALL_ONCE:
		xTaskCreatePinnedToCore(&refresh_ALL_ONCE_Task, "LED_REFRESH_AO",
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
/// @param height  矩形纵向长度(1-VERTICAL_LED_NUMBER)
/// @return NULL 错误 / 返回值 rectangle_data 为矩形数据地址 [matrix_size(rectangle_data) 为 总数据大小(uint64_t)(单位:Byte)] [RECTANGLE_MATRIX(rectangle_data) 为 矩形字模]
/// @return 例 返回值为p separation_draw(x,y,b,RECTANGLE_MATRIX(p),matrix_size(p),color); free(p);
uint8_t *rectangle(int32_t breadth, int32_t height)
{
	if (breadth < 0 || height < 0)
		return NULL;

	uint64_t x_byte_num = 0, entire_byte_num = 0; // 横向字节个数，总数据有效字节个数（不包含entire_byte_num段）
	uint64_t Dx = 0;							  // 临时存储一下横向偏移长度，这只是用于计算。纵向偏移长度由绘制函数获取，不需要,
	bool flag = 0;								  // 即将写入的位数据值

	static uint8_t *pT1 = NULL;
	static uint8_t *p = NULL;

	// 进一法，最后不足8个点就补满8位。
	// 因为ceil传入的是浮点数，全部提前转换，防止整型相除而向下取整，否则ceil在这里就没意义了
	x_byte_num = ceil(breadth * 1.0 / 8.0);
	entire_byte_num = sizeof(uint8_t) * x_byte_num * height;

	if (entire_byte_num > RECTANGLE_SIZE_MAX * sizeof(uint8_t))
	{
		ESP_LOGE("rectangle", "RECTANGLE_SIZE_MAX常量设置过小,创建的缓存空间不足");
		return NULL;
	}

	uint8_t *rectangle_data = (uint8_t *)malloc(RECTANGLE_SIZE_MAX * sizeof(uint8_t) + sizeof(uint64_t)); // 8个字节(uint64_t)用于存储字模数据大小
	memset(rectangle_data, 0, RECTANGLE_SIZE_MAX * sizeof(uint8_t) + sizeof(uint64_t));

	// 装载entire_byte_num
	*((uint64_t *)rectangle_data) = entire_byte_num;

	p = rectangle_data;
	pT1 = rectangle_data + sizeof(uint64_t); // 获取到数据的起始地址

	// 先进行全图填充
	flag = 1;
	for (uint64_t i = 0; i < entire_byte_num; i++)
	{
		for (uint64_t j = 0; j < 8; j++)
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

/// @brief 获取字模有效图形数据大小(单位:Byte)
/// @param matrix_data 字模数据,如rectangle()的返回值
/// @return 有效图形数据大小(单位:Byte)
uint64_t matrix_size(uint8_t *matrix_data)
{
	return *((uint64_t *)matrix_data);
}

/*******************************************************基本绘制函数**********************************************************/
/// @brief 三色分离方式 取模适配PCtoLCD2002
/// @brief 取模说明：从第一行开始向右每取8个点作为一个字节，如果最后不足8个点就补满8位。
/// @brief 取模顺序是从高到低，即第一个点作为最高位。如*-------取为10000000
/// @brief RGB三色分离方式绘制,只支持单色绘制,只是写入缓存，不会控制硬件显示图像
/// @brief 取模方式请参考头文件
/// @param x 图案横坐标(有效显示区x=1 到 LINE_LED_NUMBER，超出不显示，不报错)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 图案纵坐标(有效显示区y=1 到 VERTICAL_LED_NUMBER，超出不显示，不报错)，灯板左上角设为原点（1，1），由上到下绘制
/// @param breadth 图案宽度（已定义的：FIGURE-数字）
/// @param p       导入字模指针
/// @param byte_number 总数据长度(Byte)
/// @param in_color 注入颜色 （RGB顺序）
/// @return [ESP_OK 成功 / ESP_FAIL 失败 / ESP_ERR_INVALID_ARG 失败,输入了无法处理的空指针]
esp_err_t separation_draw(int x, int y, uint64_t breadth, const uint8_t *p, uint64_t byte_number, uint8_t in_color[3])
{
	const char *TAG = "separation_draw";
	if (p == NULL)
	{
		ESP_LOGE(TAG, "输入了无法处理的空指针");
		return ESP_ERR_INVALID_ARG;
	}

	uint64_t Dx = 0, Dy = 0; // xy的增加量
	uint8_t data = 0x00;	 // 临时数据存储
	uint8_t i = 0;			 // 临时变量i
	int sx = 0;				 // 临时存储选定的横坐标
	bool flag = 0;			 // 该像素是否需要点亮
	uint8_t black[3] = {0};
	uint8_t color[3] = {in_color[0], in_color[1], in_color[2]}; // 因为数组本质也是指针，所以下级改动，上级数据也会破坏，所以需要隔离

	p--; // 地址初始补偿
	while (byte_number)
	{
		p++; // 地址偏移
		for (i = 0; i <= 7; i++)
		{
			// 数据解析
			data = *p;				   // 读取数据
			flag = (data << i) & 0x80; // 位移取出一个bit数据，flag显示了选定的像素要不要点亮

			// 存储到缓冲区
			sx = x + Dx;

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

	return ESP_OK;
}

/// @brief 彩色图像直显方式 取模方式适配Img2Lcd
/// @brief [水平扫描，从左到右，从顶到底扫描，24位真彩（RGB顺序），需要图像数据头]
/// @brief 将自动获取图像头参数,写入图像到缓存,只是写入缓存，不会控制硬件显示图像
/// @param x 图案横坐标(有效显示区x=1 到 LINE_LED_NUMBER，超出不显示，不报错)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 图案纵坐标(有效显示区y=1 到 VERTICAL_LED_NUMBER，超出不显示，不报错)，灯板左上角设为原点（1，1），由上到下绘制
/// @param p 导入图像数据(包含图像数据头)的位置
/// @return [ESP_OK 成功 / ESP_FAIL 失败 /ESP_ERR_INVALID_ARG  失败,输入了无法处理的空指针]
esp_err_t direct_draw(int x, int y, const uint8_t *p)
{
	const char *TAG = "direct_draw";
	if (p == NULL)
	{
		ESP_LOGE(TAG, "输入了无法处理的空指针");
		return ESP_ERR_INVALID_ARG;
	}

	uint64_t Dx = 0, Dy = 0;			  // xy的增加量
	int sx = 0;							  // 临时存储选定的横坐标
	uint8_t *pT1 = p, *pT2 = p, *pT3 = p; // 临时指针

	// 获取图案长宽数据
	uint64_t length = 0, breadth = 0; // 长宽信息
	uint8_t data[4] = {0x00};		  // 临时数据存储
	p += 0x02;						  // 偏移到长宽数据区
	for (uint8_t i = 0; i < 4; i++)
	{
		data[i] = *p;
		p++;
	}
	breadth = (data[1] << 8) | data[0];
	length = (data[3] << 8) | data[2];
	// 图像解析
	p += 0x02;				   // 偏移到图像数据区
	uint8_t color[3] = {0x00}; // 临时数据存储
	while (length)
	{
		// 获取颜色数据
		pT1 = p;
		pT2 = p + 0x01;
		pT3 = p + 0x02;
		color[0] = *pT1;
		color[1] = *pT2;
		color[2] = *pT3;

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
	return ESP_OK;
}

/*******************************************************图像操作绘制函数**********************************************************/
/// @brief 清空所有图像缓存
void clean_all_draw_buf()
{
	memset(ledarray_blue_layer_buf, 0, LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t));
	memset(ledarray_green_layer_buf, 0, LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t));
	memset(ledarray_red_layer_buf, 0, LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t));
	memset(ledarray_tx_buf, 0, LINE_LED_NUMBER / 8 * 3 * sizeof(uint8_t));
}

/// @brief 清空指定行的图像缓存
/// @param y 指定行纵坐标(从1开始,1到VERTICAL_LED_NUMBER)
void clean_draw_buf(int y)
{
	uint8_t data[3] = {0};
	for (int i = 0; i < LINE_LED_NUMBER; i++)
	{
		color_input(i, y, data);
	}
}

/// @brief 渐进指定行的图像，使得颜色向目标颜色以步进值偏移一步
/// @param y 指定行纵坐标
/// @param step 步进值
/// @param color 目标颜色
void progress_draw_buf(int y, uint8_t step, uint8_t *color)
{
	uint8_t data[3] = {0};
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
void print_number(int x, int y, int8_t figure, uint8_t color[3])
{
	uint8_t *p = NULL;
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
	separation_draw(x, y, FIGURE_BREATH, p, sizeof(matrix_7), color); // 因为，数字字模数据大小一样，随便输入一个字模就可以
}

/// @brief (12x12大小标准)通过字库芯片支持在LED阵列打印任意字符,图像不含运动效果
/// @param x 图案横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 图案纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param color 字符颜色
/// @param format 形式同printf的可变参量表
void font_raw_print_12x(int x, int y, uint8_t color[3], char *format, ...)
{
	const char *TAG = "font_raw_print_12x";

	// 申请字符unicode编码缓存
	uint32_t *buf_unicode = NULL;
	buf_unicode = (uint32_t *)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	while (!buf_unicode)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请buf_unicode资源发现问题 正在重试");
		buf_unicode = (uint32_t *)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	}
	memset(buf_unicode, 0, FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));

	// 申请UTF-8编码缓存
	char *str_buf = NULL;
	str_buf = (char *)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	while (!str_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请str_buf资源发现问题 正在重试");
		str_buf = (char *)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	}
	memset(str_buf, 0, FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));

	// 格式化源字符串(UTF-8编码数据)到UTF-8编码缓存
	va_list ap;
	va_start(ap, format);
	vsnprintf(str_buf, FONT_CHIP_PRINT_FMT_BUF_SIZE, format, ap);

	// 获取所有要显示字符的Unicode,以及字符总个数
	uint32_t total_unit = UTF8_Unicode_get(str_buf, buf_unicode, FONT_CHIP_PRINT_NUM_MAX);

	// 申请字符点阵数据缓存
	uint8_t *font_buf = NULL;
	font_buf = (uint8_t *)malloc(total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));
	while (!font_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请font_buf资源发现问题 正在重试");
		font_buf = (uint8_t *)malloc(total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));
	}
	memset(font_buf, 0, total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));

	int idx = 0;			// 选定操作的为[idx]号字符
	uint32_t ASCII_num = 0; // 总共含有的ASCII字符个数

	// 从字库读取所有字符的点阵数据到font_buf
	for (idx = 0; idx < total_unit; idx++)
	{
		if (buf_unicode[idx] >= 128)
		{
			fonts_read_zh_CN_12x(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES]); // 读取汉字字符 宽度12
		}
		else
		{																							  // Unicode小于128兼容ASCII字符集
			fonts_read_ASCII_6x12(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES]); // 读取ASCII字符 宽度6
			ASCII_num++;
		}
	}

	int x_buf = 0;	// 当前选定的[idx]号字符点阵图像的起始x轴坐标
	int x_base = 0; // 当前选定的[idx]号字符坐标点(字模点阵左上角)与第一个字符即idx=0的水平点阵距离,这在计算[idx-1]号字符时完成累加

	// 绘制所有字符
	for (idx = 0; idx < total_unit; idx++)
	{
		x_buf = x + x_base; // 获取当前选定的[idx]号字符点阵图像的起始x轴坐标
		if (buf_unicode[idx] >= 128)
		{
			if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
				separation_draw(x_buf, y, 12, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ZH_CN_12X_BYTES, color);
			x_base += 12;
		}
		else
		{ // Unicode小于128兼容ASCII字符集
			if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
				separation_draw(x_buf, y, 6, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ASCII_6X12_BYTES, color);
			x_base += 6;
		}
	}
	// 释放所有缓存
	free(buf_unicode);
	buf_unicode = NULL;
	free(str_buf);
	str_buf = NULL;
	free(font_buf);
	font_buf = NULL;
}

/// @brief (12x12大小标准)通过字库芯片支持在LED阵列滚动打印任意字符
/// @param x 初始横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 初始纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param color 字符颜色
/// @param cartoon_handle sevetest30_UI动画支持句柄,填写句柄启用预设的动画,填写NULL以使用默认效果
/// @param format 形式同printf的可变参量表
void font_roll_print_12x(int x, int y, uint8_t color[3], cartoon_handle_t cartoon_handle, char *format, ...)
{
	const char *TAG = "font_roll_print_12x";

	// 申请字符unicode编码缓存
	uint32_t *buf_unicode = NULL;
	buf_unicode = (uint32_t *)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	while (!buf_unicode)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请buf_unicode资源发现问题 正在重试");
		buf_unicode = (uint32_t *)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	}
	memset(buf_unicode, 0, FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));

	// 申请UTF-8编码缓存
	char *str_buf = NULL;
	str_buf = (char *)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	while (!str_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请str_buf资源发现问题 正在重试");
		str_buf = (char *)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	}
	memset(str_buf, 0, FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));

	// 格式化源字符串(UTF-8编码数据)到UTF-8编码缓存
	va_list ap;
	va_start(ap, format);
	vsnprintf(str_buf, FONT_CHIP_PRINT_FMT_BUF_SIZE, format, ap);

	// 获取所有要显示字符的Unicode,以及字符总个数
	uint32_t total_unit = UTF8_Unicode_get(str_buf, buf_unicode, FONT_CHIP_PRINT_NUM_MAX);
	// 申请字符点阵数据缓存
	uint8_t *font_buf = NULL;
	font_buf = (uint8_t *)malloc(total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));
	while (!font_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请font_buf资源发现问题 正在重试");
		font_buf = (uint8_t *)malloc(total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));
	}
	memset(font_buf, 0, total_unit * FONT_CHIP_READ_ZH_CN_12X_BYTES * sizeof(uint8_t));

	int idx = 0;			// 选定操作的为[idx]号字符
	uint32_t ASCII_num = 0; // 总共含有的ASCII字符个数

	// 从字库读取所有字符的点阵数据到font_buf
	for (idx = 0; idx < total_unit; idx++)
	{
		if (buf_unicode[idx] >= 128)
		{
			fonts_read_zh_CN_12x(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES]); // 读取汉字字符 宽度12
		}
		else
		{																							  // Unicode小于128兼容ASCII字符集
			fonts_read_ASCII_6x12(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES]); // 读取ASCII字符 宽度6
			ASCII_num++;
		}
	}

	// 绘制图像形成滚动效果
	uint32_t step = 0; // 当前步进值
	int x_buf = 0;	   // 当前选定的[idx]号字符点阵图像的起始x轴坐标
	int x_base = 0;	   // 当前选定的[idx]号字符坐标点(字模点阵左上角)与第一个字符即idx=0的水平步数距离,这在计算[idx-1]号字符时完成累加

	// 如果把要滚动的字符看做一列火车车厢,屏幕看作一条小于车长的直隧洞
	// 那么隧洞有车厢存在的时间,为车头进入一刻,直到车尾离开一刻,这段时间移动距离为隧洞和车厢总长和
	// 因此这里,滚动总长度为字符链长+屏幕长,由于可显示的最小移动为一个像素点的偏移,将这个偏移称为1步,长度使用对应步数来标识
	// 因为每个字符将发生的位移一致,使用变量step作为所有字符的当前向左偏移步数,由于偏移方向与规定的屏幕x轴正方向(向右)相反,在坐标偏移运算中作减法
	// 从而,x_buf的值为对应字符([idx]号字符)在运动未开始时的初始x坐标,再减去step,过程中,step将由0累加到字符链长+屏幕长

	// 第1种方式 - 使用默认动画绘制
	if (!cartoon_handle)
	{
		for (step = 0; step < ASCII_num * 6 + (total_unit - ASCII_num) * 12 + LINE_LED_NUMBER; step++)
		{
			// 在当前step偏移下刷新一帧图像
			for (idx = 0; idx < total_unit; idx++)
			{
				x_buf = (x - 1) + LINE_LED_NUMBER + x_base - step; // 获取当前选定的[idx]号字符点阵图像的起始x轴坐标(x-1为初始坐标的绝对偏移坐标)
				// 仅对可视范围内字符进行绘制

				if (buf_unicode[idx] >= 128)
				{
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, y, 12, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ZH_CN_12X_BYTES, color);
					x_base += 12;
				}
				else
				{ // Unicode小于128兼容ASCII字符集
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, y, 6, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ASCII_6X12_BYTES, color);
					x_base += 6;
				}
			}
			vTaskDelay(pdMS_TO_TICKS(50));
			x_base = 0; // 重置字符间隔偏移缓存
		}
	}

	// 第2种方式 - 运行sevetest30_UI提供的动画支持服务
	if (cartoon_handle)
	{
		cartoon_handle->create_callback(cartoon_handle,
										ASCII_num * 6 + (total_unit - ASCII_num) * 12 + LINE_LED_NUMBER); // 生成动画
		// 创建控制对象
		int32_t cx = x;										// 需要控制的x轴坐标数据,hook函数只写
		int32_t cy = y;										// 需要控制的y轴坐标数据,hook函数只写
		uint8_t ccolor[3] = {color[0], color[1], color[2]}; // 需要控制的颜色数据,hook函数只写
		cartoon_ctrl_object_t object = {
			.pstep = &step,
			.px = &cx,
			.py = &cy,
			.pcolor = ccolor,
		};
		for (step = 0; step < ASCII_num * 6 + (total_unit - ASCII_num) * 12 + LINE_LED_NUMBER; step++)
		{
			// 调用钩子函数调整控制对象
			cartoon_handle->ctrl_hook(cartoon_handle, &object);
			for (idx = 0; idx < total_unit; idx++)
			{
				x_buf = (x - 1) + (cx - 1) + LINE_LED_NUMBER + x_base; // 获取当前选定的[idx]号字符点阵图像的起始x轴坐标(x-1 cx-1为绝对偏移坐标)
				// 仅对可视范围内字符进行绘制
				if (buf_unicode[idx] >= 128)
				{
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, cy + (y - 1), 12, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ZH_CN_12X_BYTES, ccolor);
					x_base += 12;
				}
				else
				{ // Unicode小于128兼容ASCII字符集
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, cy + (y - 1), 6, &font_buf[idx * FONT_CHIP_READ_ZH_CN_12X_BYTES], FONT_CHIP_READ_ASCII_6X12_BYTES, ccolor);
					x_base += 6;
				}
			}
			vTaskDelay(pdMS_TO_TICKS(50));
			x_base = 0; // 重置字符间隔偏移缓存
		}
	}

	// 释放所有缓存
	free(buf_unicode);
	buf_unicode = NULL;
	free(str_buf);
	str_buf = NULL;
	free(font_buf);
	font_buf = NULL;
}

/// @brief (16x16大小标准)通过字库芯片支持在LED阵列打印任意字符,图像不含运动效果
/// @param x 图案横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 图案纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param color 字符颜色
/// @param format 形式同printf的可变参量表
void font_raw_print_16x(int x, int y, uint8_t color[3], char *format, ...)
{
	const char *TAG = "font_raw_print_16x";

	// 申请字符unicode编码缓存
	uint32_t *buf_unicode = NULL;
	buf_unicode = (uint32_t *)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	while (!buf_unicode)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请buf_unicode资源发现问题 正在重试");
		buf_unicode = (uint32_t *)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	}
	memset(buf_unicode, 0, FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));

	// 申请UTF-8编码缓存
	char *str_buf = NULL;
	str_buf = (char *)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	while (!str_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请str_buf资源发现问题 正在重试");
		str_buf = (char *)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	}
	memset(str_buf, 0, FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));

	// 格式化源字符串(UTF-8编码数据)到UTF-8编码缓存
	va_list ap;
	va_start(ap, format);
	vsnprintf(str_buf, FONT_CHIP_PRINT_FMT_BUF_SIZE, format, ap);

	// 获取所有要显示字符的Unicode,以及字符总个数
	uint32_t total_unit = UTF8_Unicode_get(str_buf, buf_unicode, FONT_CHIP_PRINT_NUM_MAX);

	// 申请字符点阵数据缓存
	uint8_t *font_buf = NULL;
	font_buf = (uint8_t *)malloc(total_unit * FONT_CHIP_READ_ZH_CN_16X_BYTES * sizeof(uint8_t));
	while (!font_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请font_buf资源发现问题 正在重试");
		font_buf = (uint8_t *)malloc(total_unit * FONT_CHIP_READ_ZH_CN_16X_BYTES * sizeof(uint8_t));
	}
	memset(font_buf, 0, total_unit * FONT_CHIP_READ_ZH_CN_16X_BYTES * sizeof(uint8_t));

	int idx = 0;			// 选定操作的为[idx]号字符
	uint32_t ASCII_num = 0; // 总共含有的ASCII字符个数

	// 从字库读取所有字符的点阵数据到font_buf
	for (idx = 0; idx < total_unit; idx++)
	{
		if (buf_unicode[idx] >= 128)
		{
			fonts_read_zh_CN_16x(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES]); // 读取汉字字符 宽度16
		}
		else
		{																							  // Unicode小于128兼容ASCII字符集
			fonts_read_ASCII_8x16(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES]); // 读取ASCII字符 宽度8
			ASCII_num++;
		}
	}

	int x_buf = 0;	// 当前选定的[idx]号字符点阵图像的起始x轴坐标
	int x_base = 0; // 当前选定的[idx]号字符坐标点(字模点阵左上角)与第一个字符即idx=0的水平点阵距离,这在计算[idx-1]号字符时完成累加

	// 绘制所有字符
	for (idx = 0; idx < total_unit; idx++)
	{
		x_buf = x + x_base; // 获取当前选定的[idx]号字符点阵图像的起始x轴坐标
		if (buf_unicode[idx] >= 128)
		{
			if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
				separation_draw(x_buf, y, 16, &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES], FONT_CHIP_READ_ZH_CN_16X_BYTES, color);
			x_base += 16;
		}
		else
		{ // Unicode小于128兼容ASCII字符集
			if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
				separation_draw(x_buf, y, 8, &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES], FONT_CHIP_READ_ASCII_8X16_BYTES, color);
			x_base += 8;
		}
	}
	// 释放所有缓存
	free(buf_unicode);
	buf_unicode = NULL;
	free(str_buf);
	str_buf = NULL;
	free(font_buf);
	font_buf = NULL;
}

/// @brief (16x16大小标准)通过字库芯片支持在LED阵列滚动打印任意字符
/// @param x 初始横坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由左到右绘制
/// @param y 初始纵坐标(无范围限制，超出不显示)，灯板左上角设为原点（1，1），由上到下绘制
/// @param color 字符颜色
/// @param cartoon_handle sevetest30_UI动画支持句柄,填写句柄启用预设的动画,填写NULL以使用默认效果
/// @param format 形式同printf的可变参量表
void font_roll_print_16x(int x, int y, uint8_t color[3], cartoon_handle_t cartoon_handle, char *format, ...)
{
	const char *TAG = "font_roll_print_16x";

	// 申请字符unicode编码缓存
	uint32_t *buf_unicode = NULL;
	buf_unicode = (uint32_t *)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	while (!buf_unicode)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请buf_unicode资源发现问题 正在重试");
		buf_unicode = (uint32_t *)malloc(FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));
	}
	memset(buf_unicode, 0, FONT_CHIP_PRINT_NUM_MAX * sizeof(uint32_t));

	// 申请UTF-8编码缓存
	char *str_buf = NULL;
	str_buf = (char *)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	while (!str_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请str_buf资源发现问题 正在重试");
		str_buf = (char *)malloc(FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));
	}
	memset(str_buf, 0, FONT_CHIP_PRINT_FMT_BUF_SIZE * sizeof(char));

	// 格式化源字符串(UTF-8编码数据)到UTF-8编码缓存
	va_list ap;
	va_start(ap, format);
	vsnprintf(str_buf, FONT_CHIP_PRINT_FMT_BUF_SIZE, format, ap);

	// 获取所有要显示字符的Unicode,以及字符总个数
	uint32_t total_unit = UTF8_Unicode_get(str_buf, buf_unicode, FONT_CHIP_PRINT_NUM_MAX);
	// 申请字符点阵数据缓存
	uint8_t *font_buf = NULL;
	font_buf = (uint8_t *)malloc(total_unit * FONT_CHIP_READ_ZH_CN_16X_BYTES * sizeof(uint8_t));
	while (!font_buf)
	{
		vTaskDelay(pdMS_TO_TICKS(1000));
		ESP_LOGE(TAG, "申请font_buf资源发现问题 正在重试");
		font_buf = (uint8_t *)malloc(total_unit * FONT_CHIP_READ_ZH_CN_16X_BYTES * sizeof(uint8_t));
	}
	memset(font_buf, 0, total_unit * FONT_CHIP_READ_ZH_CN_16X_BYTES * sizeof(uint8_t));

	int idx = 0;			// 选定操作的为[idx]号字符
	uint32_t ASCII_num = 0; // 总共含有的ASCII字符个数

	// 从字库读取所有字符的点阵数据到font_buf
	for (idx = 0; idx < total_unit; idx++)
	{
		if (buf_unicode[idx] >= 128)
		{
			fonts_read_zh_CN_16x(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES]); // 读取汉字字符 宽度16
		}
		else
		{																							  // Unicode小于128兼容ASCII字符集
			fonts_read_ASCII_8x16(buf_unicode[idx], &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES]); // 读取ASCII字符 宽度8
			ASCII_num++;
		}
	}

	// 绘制图像形成滚动效果
	uint32_t step = 0; // 当前步进值
	int x_buf = 0;	   // 当前选定的[idx]号字符点阵图像的起始x轴坐标
	int x_base = 0;	   // 当前选定的[idx]号字符坐标点(字模点阵左上角)与第一个字符即idx=0的水平步数距离,这在计算[idx-1]号字符时完成累加

	// 如果把要滚动的字符看做一列火车车厢,屏幕看作一条小于车长的直隧洞
	// 那么隧洞有车厢存在的时间,为车头进入一刻,直到车尾离开一刻,这段时间移动距离为隧洞和车厢总长和
	// 因此这里,滚动总长度为字符链长+屏幕长,由于可显示的最小移动为一个像素点的偏移,将这个偏移称为1步,长度使用对应步数来标识
	// 因为每个字符将发生的位移一致,使用变量step作为所有字符的当前向左偏移步数,由于偏移方向与规定的屏幕x轴正方向(向右)相反,在坐标偏移运算中作减法
	// 从而,x_buf的值为对应字符([idx]号字符)在运动未开始时的初始x坐标,再减去step,过程中,step将由0累加到字符链长+屏幕长

	// 第1种方式 - 使用默认动画绘制
	if (!cartoon_handle)
	{
		for (step = 0; step < ASCII_num * 8 + (total_unit - ASCII_num) * 16 + LINE_LED_NUMBER; step++)
		{
			// 在当前step偏移下刷新一帧图像
			for (idx = 0; idx < total_unit; idx++)
			{
				x_buf = (x - 1) + LINE_LED_NUMBER + x_base - step; // 获取当前选定的[idx]号字符点阵图像的起始x轴坐标(x-1为初始坐标的绝对偏移坐标)
				// 仅对可视范围内字符进行绘制

				if (buf_unicode[idx] >= 128)
				{
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, y, 16, &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES], FONT_CHIP_READ_ZH_CN_16X_BYTES, color);
					x_base += 16;
				}
				else
				{ // Unicode小于128兼容ASCII字符集
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, y, 8, &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES], FONT_CHIP_READ_ASCII_8X16_BYTES, color);
					x_base += 8;
				}
			}
			vTaskDelay(pdMS_TO_TICKS(50));
			x_base = 0; // 重置字符间隔偏移缓存
		}
	}

	// 第2种方式 - 运行sevetest30_UI提供的动画支持服务
	if (cartoon_handle)
	{
		cartoon_handle->create_callback(cartoon_handle,
										ASCII_num * 8 + (total_unit - ASCII_num) * 16 + LINE_LED_NUMBER); // 生成动画
		// 创建控制对象
		int32_t cx = x;										// 需要控制的x轴坐标数据,hook函数只写
		int32_t cy = y;										// 需要控制的y轴坐标数据,hook函数只写
		uint8_t ccolor[3] = {color[0], color[1], color[2]}; // 需要控制的颜色数据,hook函数只写
		cartoon_ctrl_object_t object = {
			.pstep = &step,
			.px = &cx,
			.py = &cy,
			.pcolor = ccolor,
		};
		for (step = 0; step < ASCII_num * 8 + (total_unit - ASCII_num) * 16 + LINE_LED_NUMBER; step++)
		{
			// 调用钩子函数调整控制对象
			cartoon_handle->ctrl_hook(cartoon_handle, &object);
			for (idx = 0; idx < total_unit; idx++)
			{
				x_buf = (x - 1) + (cx - 1) + LINE_LED_NUMBER + x_base; // 获取当前选定的[idx]号字符点阵图像的起始x轴坐标(x-1 cx-1为绝对偏移坐标)
				// 仅对可视范围内字符进行绘制
				if (buf_unicode[idx] >= 128)
				{
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, cy + (y - 1), 16, &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES], FONT_CHIP_READ_ZH_CN_16X_BYTES, ccolor);
					x_base += 16;
				}
				else
				{ // Unicode小于128兼容ASCII字符集
					if (x_buf > -LINE_LED_NUMBER && x_buf <= LINE_LED_NUMBER)
						separation_draw(x_buf, cy + (y - 1), 8, &font_buf[idx * FONT_CHIP_READ_ZH_CN_16X_BYTES], FONT_CHIP_READ_ASCII_8X16_BYTES, ccolor);
					x_base += 8;
				}
			}
			vTaskDelay(pdMS_TO_TICKS(50));
			x_base = 0; // 重置字符间隔偏移缓存
		}
	}

	// 释放所有缓存
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
/// @return [ESP_FAIL 创建refresh_Task_Mutex互斥量时发现问题 / 无法获取refresh_Task_Mutex互斥量 / refresh_Task_Mutex互斥量已经被意外创建]
/// @return [ESP_ERR_INVALID_STATE 灯板阵列之前已经初始化,运行ledarray_deinit以去初始化 / RMT控制器之前已经安装,请调用对应rmt_driver_uninstall释放需要的资源]
/// @return [ESP_ERR_INVALID_ARG 参数错误]
/// @return [ESP_ERR_NO_MEM 内存不足]
esp_err_t ledarray_init()
{
	const char *TAG = "ledarray_init";

	if (is_initialized)
	{
		ESP_LOGE(TAG, "灯板阵列之前已经初始化,运行ledarray_deinit以去初始化");
		return ESP_ERR_INVALID_STATE;
	}
	else
	{
		is_initialized = true;
		if (refresh_Task_Mutex == NULL)
		{
			refresh_Task_Mutex = xSemaphoreCreateMutex();
			if (!refresh_Task_Mutex)
			{
				ESP_LOGE(TAG, "创建refresh_Task_Mutex互斥量时发现问题");
				return ESP_FAIL;
			}
		}
		else
		{
			ESP_LOGE(TAG, "refresh_Task_Mutex互斥量已经被意外创建");
			return ESP_FAIL;
		}
	}

	if (xSemaphoreTake(refresh_Task_Mutex, pdMS_TO_TICKS(LEDARRAY_REFRESH_MUTEX_MANAGE_TAKE_TIMEOUT_MS)) == pdTRUE)
	{

		// 申请显示数据内存空间
		ledarray_blue_layer_buf = (uint8_t *)malloc(LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t));
		ledarray_green_layer_buf = (uint8_t *)malloc(LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t));
		ledarray_red_layer_buf = (uint8_t *)malloc(LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t));
		ledarray_tx_buf = (uint8_t *)malloc(LINE_LED_NUMBER / 8 * 3 * sizeof(uint8_t));
		if (!ledarray_blue_layer_buf || !ledarray_green_layer_buf || !ledarray_red_layer_buf || !ledarray_tx_buf)
		{
			ESP_LOGE(TAG, "申请显示数据内存空间失败");
			return ESP_ERR_NO_MEM;
		}
		memset(ledarray_blue_layer_buf, 0, LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t));
		memset(ledarray_green_layer_buf, 0, LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t));
		memset(ledarray_red_layer_buf, 0, LINE_LED_NUMBER * VERTICAL_LED_NUMBER * sizeof(uint8_t));
		memset(ledarray_tx_buf, 0, LINE_LED_NUMBER / 8 * 3 * sizeof(uint8_t));

		// 配置spi总线
		spi_bus_config_t bus_config = {
			.mosi_io_num = -1,
			.miso_io_num = -1,
			.sclk_io_num = -1,
			.quadwp_io_num = -1,
			.quadhd_io_num = -1,
			.data4_io_num = -1,
			.data5_io_num = -1,
			.data6_io_num = -1,
			.data7_io_num = -1,
			.max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
			.flags = SPICOMMON_BUSFLAG_MASTER,
		};
		spi_device_interface_config_t interface_config = {
			.command_bits = 0,
			.address_bits = 0,
			.dummy_bits = 0,
			.mode = 0,
			.clock_speed_hz = LEDARRAY_SPI_FREQ,
			.spics_io_num = -1,
			.queue_size = 1,
			.flags = SPI_DEVICE_HALFDUPLEX,
		};

		// 按照board_def中的引脚配置修改上面初步配置,之后是最终引脚配置
		ESP_RETURN_ON_ERROR(get_spi_pins_ledarray(&bus_config, &interface_config), TAG, "获取为LED阵列提供的SPI通讯IO定义时发现问题");

		// 配置spi并载入设备
		ESP_RETURN_ON_ERROR(spi_bus_initialize(LEDARRAY_SPI_ID, &bus_config, SPI_DMA_CH_AUTO), TAG, "初始化SPI异常");
		ESP_RETURN_ON_ERROR(spi_bus_add_device(LEDARRAY_SPI_ID, &interface_config, &ledarray_spi_handle), TAG, "添加SPI设备异常");

		// 设置合适的灯板SPI相关引脚驱动能力,减少干扰并提升抗干扰能力
		gpio_set_drive_capability(LEDARRAY_SPI_MOSI_IO, GPIO_DRIVE_CAP_0);
		gpio_set_drive_capability(LEDARRAY_SPI_SCLK_IO, GPIO_DRIVE_CAP_3);

		// 配置其他IO
		esp_err_t err = ESP_OK;

		gpio_config_t gpio_ledarray = {
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = GPIO_PULLUP_DISABLE,
			.pull_down_en = GPIO_PULLDOWN_DISABLE,
			.intr_type = GPIO_INTR_DISABLE,
		};

		err |= gpio_force_unhold_all();

		gpio_ledarray.pin_bit_mask = 1ULL << LEDARRAY_LE_IO;
		err |= gpio_reset_pin(LEDARRAY_LE_IO);
		err |= gpio_config(&gpio_ledarray);
		err |= gpio_set_level(LEDARRAY_LE_IO, 0);

		gpio_ledarray.pin_bit_mask = 1ULL << LEDARRAY_OE_IO;
		err |= gpio_reset_pin(LEDARRAY_OE_IO);
		err |= gpio_config(&gpio_ledarray);
		err |= gpio_set_level(LEDARRAY_OE_IO, 1);

		gpio_ledarray.pin_bit_mask = 1ULL << LEDARRAY_CSE_IO;
		err |= gpio_reset_pin(LEDARRAY_CSE_IO);
		err |= gpio_config(&gpio_ledarray);
		err |= gpio_set_level(LEDARRAY_CSE_IO, 0);

		gpio_ledarray.pin_bit_mask = 1ULL << LEDARRAY_CSA0_IO;
		err |= gpio_reset_pin(LEDARRAY_CSA0_IO);
		err |= gpio_config(&gpio_ledarray);
		err |= gpio_set_level(LEDARRAY_CSA0_IO, 0);

		gpio_ledarray.pin_bit_mask = 1ULL << LEDARRAY_CSA1_IO;
		err |= gpio_reset_pin(LEDARRAY_CSA1_IO);
		err |= gpio_config(&gpio_ledarray);
		err |= gpio_set_level(LEDARRAY_CSA1_IO, 0);

		gpio_ledarray.pin_bit_mask = 1ULL << LEDARRAY_CSA2_IO;
		err |= gpio_reset_pin(LEDARRAY_CSA2_IO);
		err |= gpio_config(&gpio_ledarray);
		err |= gpio_set_level(LEDARRAY_CSA2_IO, 0);

		// 设置合适的引脚驱动能力,减少干扰并提升抗干扰能力
		gpio_set_drive_capability(LEDARRAY_OE_IO, GPIO_DRIVE_CAP_3);
		gpio_set_drive_capability(LEDARRAY_LE_IO, GPIO_DRIVE_CAP_3);
		gpio_set_drive_capability(LEDARRAY_CSE_IO, GPIO_DRIVE_CAP_3);
		gpio_set_drive_capability(LEDARRAY_CSA0_IO, GPIO_DRIVE_CAP_3);
		gpio_set_drive_capability(LEDARRAY_CSA1_IO, GPIO_DRIVE_CAP_3);
		gpio_set_drive_capability(LEDARRAY_CSA2_IO, GPIO_DRIVE_CAP_3);

		xSemaphoreGive(refresh_Task_Mutex);

		// 设置自动刷新模式为默认模式
		ledarray_set_auto_refresh_mode(LEDARRAY_REFRESH_INIT_MODE);

		if (err == ESP_OK)
		{
			ESP_LOGW(TAG, " %d X %d LED阵列初始化完成 当前自动刷新服务模式 %d", LINE_LED_NUMBER, VERTICAL_LED_NUMBER, LEDARRAY_REFRESH_INIT_MODE);
			return ESP_OK;
		}
		else
		{
			ESP_LOGE(TAG, "LED阵列初始化GPIO时发现问题");
			return ESP_FAIL;
		}
	}
	else
	{
		ESP_LOGE(TAG, "LED阵列初始化时发现问题,无法获取refresh_Task_Mutex互斥量");
		return ESP_FAIL;
	}
}

/// @brief  灯板阵列反初始化操作
/// @return [ESP_OK 成功]
/// @return [ESP_FAIL 释放资源失败 / refresh_Task_Mutex互斥量异常]
/// @return [ESP_ERR_INVALID_STATE 灯板阵列未初始化,无需反初始化]
/// @return [ESP_ERR_INVALID_ARG ledarray_gpio_num_list中存在错误的GPIO号码]
esp_err_t ledarray_deinit()
{
	const char *TAG = "ledarray_deinit";

	if (!is_initialized)
	{
		ESP_LOGE(TAG, "灯板阵列未初始化,无需反初始化");
		return ESP_ERR_INVALID_STATE;
	}

	// 禁用刷新服务
	ledarray_set_auto_refresh_mode(LEDARRAY_AUTO_REFRESH_DISABLE);

	// 安全终止工作并删除互斥量
	BaseType_t ret = xSemaphoreTake(refresh_Task_Mutex, pdMS_TO_TICKS(LEDARRAY_REFRESH_MUTEX_MANAGE_TAKE_TIMEOUT_MS));
	if (ret != pdTRUE)
	{
		ESP_LOGE(TAG, "refresh_Task_Mutex互斥量异常,无法占用互斥量以安全清理");
		return ESP_FAIL;
	}

	is_initialized = false;

	xSemaphoreGive(refresh_Task_Mutex);
	vTaskDelay(100);

	ret = xSemaphoreTake(refresh_Task_Mutex, pdMS_TO_TICKS(LEDARRAY_REFRESH_MUTEX_MANAGE_TAKE_TIMEOUT_MS));
	if (ret != pdTRUE)
	{
		ESP_LOGE(TAG, "refresh_Task_Mutex互斥量异常,无法占用互斥量以安全清理");
		return ESP_FAIL;
	}

	free(ledarray_blue_layer_buf);
	ledarray_blue_layer_buf = NULL;

	free(ledarray_green_layer_buf);
	ledarray_green_layer_buf = NULL;

	free(ledarray_red_layer_buf);
	ledarray_red_layer_buf = NULL;

	free(ledarray_tx_buf);
	ledarray_tx_buf = NULL;

	spi_device_release_bus(ledarray_spi_handle);

	spi_bus_remove_device(ledarray_spi_handle);

	spi_bus_free(LEDARRAY_SPI_ID);

	vSemaphoreDelete(refresh_Task_Mutex);

	ESP_LOGW(TAG, " %d X %d LED阵列反初始化操作完成,所有资源已释放 当前自动刷新服务模式 %d", LINE_LED_NUMBER, VERTICAL_LED_NUMBER, LEDARRAY_AUTO_REFRESH_DISABLE);
	return ESP_OK;
}

/// @brief 灯板阵列显示一帧画面(闪烁一帧)
/// @return [ESP_OK 成功]
/// @return [ESP_FAIL refresh_Task_Mutex互斥量异常]
/// @return [ESP_ERR_INVALID_STATE 灯板阵列未初始化,无需反初始化]
esp_err_t ledarray_show_frame()
{
	const char *TAG = "ledarray_show_frame";

	if (!is_initialized)
	{
		ESP_LOGE(TAG, "灯板阵列未初始化");
		return ESP_ERR_INVALID_STATE;
	}

	if (spi_device_acquire_bus(ledarray_spi_handle, portMAX_DELAY) == ESP_OK)
	{
		int bcm_bit_plane_idx = 0;										   // BCM调光算法 - 场索引
		BaseType_t bcm_delay_nop_num = LEDARRAY_REFRESH_BCM_DELAY_NOP_NUM; // BCM调光算法 - 单位时延对应的NOP空指令个数
		const int bcm_weight[8] = {1, 2, 4, 8, 16, 32, 64, 128};		   // BCM调光算法 - 位权

		spi_transaction_t trans_tx = {
			.length = LINE_LED_NUMBER * 3,
			.tx_buffer = ledarray_tx_buf,
		};

		if (xSemaphoreTake(refresh_Task_Mutex, pdMS_TO_TICKS(LEDARRAY_REFRESH_MUTEX_SHOW_TAKE_TIMEOUT_MS)) != pdTRUE)
		{
			// ESP_LOGE(TAG, "refresh_Task_Mutex互斥量异常,无法占用互斥量以安全写入灯板阵列");
			return ESP_FAIL;
		}

		for (bcm_bit_plane_idx = 0; bcm_bit_plane_idx < 8; bcm_bit_plane_idx++)
		{
			for (int n = 0; n < VERTICAL_LED_NUMBER; n++)
			{

				// 计算行数据
				memset(ledarray_tx_buf, 0, LINE_LED_NUMBER / 8 * 3 * sizeof(uint8_t));
				for (int m = 0; m < LINE_LED_NUMBER; m++)
				{
					// 级联中，越靠后的芯片数据越先发送,16bits先发高八位,再发低八位,每个字节的位号与引脚对应，如 D0(低八位) -> OUT0,D1(低八位) -> OUT1,D0(高八位)->OUT8,D1(高八位)->OUT9
					ledarray_tx_buf[0 * LINE_LED_NUMBER / 8 + (LINE_LED_NUMBER / 8 - 1 - (int)(m / 8))] |= ((ledarray_blue_layer_buf[n * LINE_LED_NUMBER + m] >> bcm_bit_plane_idx) & 0x01) << (m - (int)(m / 8) * 8);
					ledarray_tx_buf[1 * LINE_LED_NUMBER / 8 + (LINE_LED_NUMBER / 8 - 1 - (int)(m / 8))] |= ((ledarray_red_layer_buf[n * LINE_LED_NUMBER + m] >> bcm_bit_plane_idx) & 0x01) << (m - (int)(m / 8) * 8);
					ledarray_tx_buf[2 * LINE_LED_NUMBER / 8 + (LINE_LED_NUMBER / 8 - 1 - (int)(m / 8))] |= ((ledarray_green_layer_buf[n * LINE_LED_NUMBER + m] >> bcm_bit_plane_idx) & 0x01) << (m - (int)(m / 8) * 8);
				}

				// 确保灭灯
				gpio_set_level(LEDARRAY_OE_IO, 1);

				// 发送行数据
				spi_device_polling_transmit(ledarray_spi_handle, &trans_tx);

				// 锁存行数据
				gpio_set_level(LEDARRAY_LE_IO, 1);
				gpio_set_level(LEDARRAY_LE_IO, 0);

				// 选定ICND2013,仅需要在需要切换时改变电平
				if (n == 0)
				{
					gpio_set_level(LEDARRAY_CSE_IO, 0); // 控制第一个ICND2013,对应y=1-8
				}
				if (n == VERTICAL_LED_NUMBER / 2)
				{
					gpio_set_level(LEDARRAY_CSE_IO, 1); // 控制第二个ICND2013,对应y=9-16
				}

				// 切换到下一行
				uint32_t s = 0;
				if (n >= 0 && n < VERTICAL_LED_NUMBER / 2)
				{
					s = n;
				}
				else
				{
					s = n - VERTICAL_LED_NUMBER / 2;
				}
				gpio_set_level(LEDARRAY_CSA0_IO, (s >> 0) & 0x01);
				gpio_set_level(LEDARRAY_CSA1_IO, (s >> 1) & 0x01);
				gpio_set_level(LEDARRAY_CSA2_IO, (s >> 2) & 0x01);

				// 点亮当前行
				gpio_set_level(LEDARRAY_OE_IO, 0);

				// 按BCM场数确定当前时延
				for (int d = 0; d < bcm_weight[bcm_bit_plane_idx] * bcm_delay_nop_num; d++)
				{
					asm volatile("nop");
				}

			}
		}

		// 最后一行灭灯消隐
		gpio_set_level(LEDARRAY_OE_IO, 1);

		xSemaphoreGive(refresh_Task_Mutex);

		spi_device_release_bus(ledarray_spi_handle);

		return ESP_OK;
	}
	else
	{
		ESP_LOGE(TAG, "SPI总线忙,无法占用SPI以安全写入灯板阵列");
		return ESP_FAIL;
	}
}

/// @brief 颜色数据导入
/// @param x 横坐标(1 到 LINE_LED_NUMBER)
/// @param y 纵坐标(1 到 VERTICAL_LED_NUMBER)
/// @param data 导入的颜色RGB数据位置
void color_input(int x, int y, uint8_t *data)
{
	if (!data)
	{
		ESP_LOGE("color_output", "输入了无法处理的空指针");
		return;
	}

	if (x < 1 || x > LINE_LED_NUMBER || y < 1 || y > VERTICAL_LED_NUMBER)
	{
		return; // 不在显示范围退出即可，允许在范围外但不报告
	}

	ledarray_red_layer_buf[(y - 1) * LINE_LED_NUMBER + x - 1] = data[0];
	ledarray_green_layer_buf[(y - 1) * LINE_LED_NUMBER + x - 1] = data[1];
	ledarray_blue_layer_buf[(y - 1) * LINE_LED_NUMBER + x - 1] = data[2];
}

/// @brief 颜色数据导出
/// @param x 横坐标(1 到 LINE_LED_NUMBER)
/// @param y 纵坐标(1 到 VERTICAL_LED_NUMBER)
/// @param data 导出存储的颜色RGB数据的位置
void color_output(int x, int y, uint8_t *data)
{
	if (!data)
	{
		ESP_LOGE("color_output", "输入了无法处理的空指针");
		return;
	}

	if (x < 1 || x > LINE_LED_NUMBER || y < 1 || y > VERTICAL_LED_NUMBER)
	{
		return; // 不在显示范围退出即可，允许在范围外但不报告
	}

	data[0] = ledarray_red_layer_buf[(y - 1) * LINE_LED_NUMBER + x - 1];
	data[1] = ledarray_green_layer_buf[(y - 1) * LINE_LED_NUMBER + x - 1];
	data[2] = ledarray_blue_layer_buf[(y - 1) * LINE_LED_NUMBER + x - 1];
}