
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

#pragma once

#include "esp_heap_caps.h"

#define MATH_TOOLS_MALLOC_CAP_DEFAULT MALLOC_CAP_SPIRAM //默认内存申请位置

 /*******基本最小操作*******/

  //基本矩阵编辑操作
esp_err_t matrix_transpose(float* input, float* output, int m, int n);
void matrix_swap_rows(float* A, int n, int r1, int r2);
float matrix_only_row_elimination_step(float* A, int aim_row, int pivot_row, int pivot_col, bool major_default, int LDA);

//基本矩阵提取操作
int matrix_get_the_row_of_aim_col_pivot(const float* A, int m, int n, int aim_col, int start_row);
void matrix_extract_triangle_region(const float* A, float* output, int n, bool lower, bool other, bool set1, bool set2);

//基本矩阵可视化操作
void matrix_log_print(const float* A, int m, int n, bool major_default, bool show_rc);

/*******二级封装操作(经典基本数学计算方法实现)*******/

//LU相关
esp_err_t matrix_decomposition_LU(float* A, float* L, float* U, int* P, int m, int n);
esp_err_t matrix_square_solve_LU(const float* L, const float* U, const int* P, float* b, float* x, int n);
esp_err_t matrix_inverse_LU(const float* A, float* inv_A, int n);


/*******三级封装操作(开箱即用实用化操作)*******/

//求解 - solve_x

esp_err_t solve_overdet_system_ols_mlr(const float* X, const float* y, float* beta, int m, int n);

//评估 - appraisal_x

esp_err_t appraisal_residual_linear_model(float* r, float* J, const float* X, const float* y, float* beta, int m, int n);


