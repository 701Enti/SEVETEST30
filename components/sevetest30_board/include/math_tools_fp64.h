
/*
 * 701Enti MIT License
 *
 * Copyright © 2025 <701Enti organization>
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

 // 包含各种基于ESP-DSP的数学工具函数
 // "fp64"表示相关计算使用double类型进行
 // 部分矩阵运算,使用由esp-dsp项目中的一些运算函数修改而来的double版本函数构成的matrix_f64库,非常感谢原作者,具体声明见本项目README和相关源文件声明
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include "esp_heap_caps.h"

#define MATH_TOOLS_MALLOC_CAP_DEFAULT MALLOC_CAP_SPIRAM //默认内存申请位置

 /*******基本最小操作*******/

  //基本矩阵编辑操作
esp_err_t matrix_transpose_fp64(double* input, double* output, int m, int n);
void matrix_swap_rows_fp64(double* A, int n, int r1, int r2);
double matrix_only_row_elimination_step_fp64(double* A, int aim_row, int pivot_row, int pivot_col, bool major_default, int LDA);

//基本矩阵提取操作
int matrix_get_the_row_of_aim_col_pivot_fp64(const double* A, int m, int n, int aim_col, int start_row);
void matrix_extract_triangle_region_fp64(const double* A, double* output, int n, bool lower, bool other, bool set1, bool set2);

//基本矩阵可视化操作
void matrix_log_print_fp64(const double* A, int m, int n, bool major_default, bool show_rc);

/*******二级封装操作(经典基本数学计算方法实现)*******/

//LU相关
esp_err_t matrix_decomposition_LU_fp64(double* A, double* L, double* U, int* P, int m, int n);
esp_err_t matrix_square_solve_LU_fp64(const double* L, const double* U, const int* P, double* b, double* x, int n);
esp_err_t matrix_inverse_LU_fp64(const double* A, double* inv_A, int n);


/*******三级封装操作(开箱即用实用化操作)*******/

//求解 - solve_x

esp_err_t solve_overdet_system_ols_mlr_fp64(const double* X, const double* y, double* beta, int m, int n);

//评估 - appraisal_x

esp_err_t appraisal_residual_linear_model_fp64(double* r, double* J, const double* X, const double* y, double* beta, int m, int n);


