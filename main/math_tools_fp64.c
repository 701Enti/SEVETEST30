
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

#include "math_tools_fp64.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "esp_cpu.h"
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "matrix_fp64.h"


static const char* math_tools_TAG = __FILE__;

/// @brief 通用矩阵转置
/// @param input 矩阵输入 
/// @param output 矩阵输出
/// @param m 原矩阵的行数
/// @param n 原矩阵的列数
void general_matrix_transpose_fp64(double* input, double* output, int m, int n) {
    for (int i = 0;i < m;i++) {
        for (int j = 0;j < n;j++) {
            //input(共m行,n列) 第i行,第j列 为 output(共n行,m列) 第j行,第i列的值
            output[j * m + i] = input[i * n + j];
        }
    }
}

void aligned_16_matrix_transpose_fp64(double* input, double* output, int m, int n) {

}

/// @brief 方阵原地转置,直接在输入上操作
/// @param local 矩阵输入 
/// @param n 原矩阵(n,n)的列数
void square_matrix_inplace_transpose_fp64(double* local, int n) {
    for (int i = 0;i < n;i++) {
        for (int j = i + 1;j < n;j++) {//仅遍历上三角
            double buf = local[i * n + j];
            local[i * n + j] = local[j * n + i];
            local[j * n + i] = buf;
        }
    }
}




void square_aligned_16_matrix_transpose_fp64(double* local, int n) {

}

/// @brief [目前仅支持行主序]矩阵转置,自动根据上下文参数选择更优更快的处理方式(如何触发见下方备注),若满足多个优化条件,优化可能叠加 (一般不会修改源数据矩阵,除非方阵触发原地转置优化)
/// @note [触发-内存16对齐优化]通过一些方式,在申请内存后获得16对齐内存(可获得更快的速度)
/// @note [触发-方阵优化(前提是运算对象是方阵)](会修改源数据矩阵)使得 m = n , input == output , input != NULL , output != NULL 同时满足,即输入输出都是一个内存区域(可实现原地转置,减少内存消耗)
/// @param input 矩阵输入 
/// @param output 矩阵输出
/// @param m 原矩阵的行数
/// @param n 原矩阵的列数
/// @return [ESP_OK 运算完成]  
/// @return [ESP_ERR_INVALID_ARG input为NULL / output为NULL / m <= 0 / n <= 0] 
esp_err_t matrix_transpose_fp64(double* input, double* output, int m, int n) {
    //参数检查
    ESP_RETURN_ON_FALSE(input && output && m > 0 && n > 0, ESP_ERR_INVALID_ARG, math_tools_TAG, "传入参数异常 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));

    // //选出最佳策略
    // if ((uintptr_t)input % 16 == 0 && (uintptr_t)output % 16 == 0) {
    //     //input output 16内存对齐
    //     if (input == output) {
    //         //在同一内存区域操作 + input output 16内存对齐
    //         if (m == n) {
    //             //[触发-方阵优化]+[触发-内存16对齐优化] <- 方阵 + 在同一内存区域操作 + input output 16内存对齐
    //             square_aligned_16_matrix_transpose(input, n);
    //             return ESP_OK;
    //         }
    //     }

    //     //在上方如果未获得更优策略,就不会return,从而执行下方策略
    //     aligned_16_matrix_transpose(input, output, m, n);
    //     return ESP_OK;
    // }
    // else {
    //     if (input == output) {
    //         //在同一内存区域操作
    //         if (m == n) {
    //             //[触发-方阵优化] <- 方阵 + 在同一内存区域操作
    //             square_matrix_inplace_transpose_fp64(input, n);
    //             return ESP_OK;
    //         }
    //     }

        //在上方如果未获得更优策略,就不会return,从而执行下方策略
    general_matrix_transpose_fp64(input, output, m, n);
    return ESP_OK;
    // }

    //请勿在本函数末尾添加return语句
    //请勿在本函数末尾添加return语句
    //请勿在本函数末尾添加return语句
    //编译器根据if中的return自动检查if逻辑,请勿在本函数末尾添加return语句,报错说明if逻辑异常,或未在本次更改新添加的策略函数调用下方紧接着return ESP_OK;
}



/// @brief [目前仅支持行主序]获取矩阵中目标列的主元的行号(找出矩阵目标列中的主元位置,输出它的行号)(不会修改源数据矩阵)
/// @param A 需要进行查找操作的矩阵A(m x n)
/// @param m 矩阵A的总行数
/// @param n 矩阵A的总列数
/// @param aim_col 目标列(列号,范围 0 - n-1)
/// @param start_row [优化参数]要求从行号(范围 0 - m-1)为start_row的行开始向下查找,不需要填写0即可(整列数全部参与查找)
/// @return 目标列中的主元的行号(范围 0 - m-1)
int matrix_get_the_row_of_aim_col_pivot_fp64(const double* A, int m, int n, int aim_col, int start_row) {
    int ret = start_row;//返回值缓存
    double max = 0;//当前发现的最大绝对值
    double a = 0;//当前位置元素绝对值缓存
    for (int r = start_row;r < m;r++) {//r为当前遍历的行号
        //主元即绝对值最大
        a = fabs(A[r * n + aim_col]);//当前位置元素绝对值
        if (a > max) {
            max = a;
            ret = r;
        }
    }
    return ret;
}

/// @brief [目前仅支持行主序](会修改源数据矩阵)矩阵中的两行(r1和r2)交换所有元素
/// @param A 需要进行交换操作的矩阵A(m x n)
/// @param n 矩阵A的总列数
/// @param r1 需要进行交换操作的行号(范围 0 - m-1)
/// @param r2 需要进行交换操作的行号(范围 0 - m-1)
void matrix_swap_rows_fp64(double* A, int n, int r1, int r2) {
    if (r1 == r2) {
        return;
    }
    else {
        double buf = 0;//元素值缓存
        for (int c = 0;c < n;c++) {
            buf = A[r1 * n + c];
            A[r1 * n + c] = A[r2 * n + c];
            A[r2 * n + c] = buf;
        }
        return;
    }
}


/// @brief [目前仅支持行主序]提取矩阵(nxn,必须为方阵)中的三角区(上三角/下三角),含对角线(不会修改源数据矩阵)
/// @param A 从矩阵A(nxn,必须为方阵)中提取
/// @param output 提取后输出到output(nxn,必须为方阵)
/// @param n 矩阵A的边长n
/// @param lower true = 提取下三角 / false = 提取上三角
/// @param other 是否对超出提取区域位置处output的元素设置为0 (true = 设置为0 / false = 不进行任何操作)
/// @param set1 true = 强制输出矩阵对角线元素为固定值,固定值由set2设置 / false = 根据set2执行其他方案
/// @param set2[情况1.当set1=true]  选择强制输出矩阵的对角线元素为1还是0 (true = 强制对角线元素为1 / false = 强制对角线元素为0)
/// @param set2[情况2.当set1=false] 执行其他方案, (true = 复制矩阵A对角线元素到输出矩阵对角线元素 / false = 跳过输出矩阵对角线元素存储区域,不进行任何更改)
void matrix_extract_triangle_region_fp64(const double* A, double* output, int n, bool lower, bool other, bool set1, bool set2) {
    for (int r = 0;r < n;r++) {
        for (int c = 0;c < n;c++) {
            if (r == c) {
                //处于矩阵A对角线位置
                if (set1) {
                    //强制输出矩阵对角线元素为固定值
                    output[r * n + c] = set2;
                }
                else {
                    //根据set2执行其他方案
                    if (set2) {
                        //执行其他方案 - 复制矩阵A对角线元素到输出矩阵对角线元素
                        output[r * n + c] = A[r * n + c];
                    }
                    //set2 = false:跳过输出矩阵对角线元素存储区域,不进行任何更改
                }
            }
            else {
                //不处于矩阵A对角线位置
                if (lower ? (r > c) : (r < c)) {
                    //不处于矩阵A对角线位置,且在需提取区内
                    output[r * n + c] = A[r * n + c];//提取

                }
                else {
                    //不处于矩阵A对角线位置,且在需提取区外
                    if (other) {
                        //other=true:对超出提取区域位置处output的元素设置为0
                        output[r * n + c] = 0.0f;
                    }
                    //other=false:不进行任何操作
                }
            }
        }
    }
}

/// @brief (会修改源数据矩阵)矩阵仅行消元操作(高斯消元)(之前要预先完成如归一化,行交换等预处理),用主元行消去目标行(高斯消元),本函数是消元操作步骤中的单一一步,不会遍历迭代完成所有消元,需要外部手动遍历
/// @param A 需要操作的矩阵,非必须为方阵
/// @param aim_row 目标行
/// @param pivot_row 主元行
/// @param pivot_col 主元列
/// @param major_default 是否使用默认主序(行主序),无要求的话填写true  [ture = 使用行主序(C语言惯用,默认) / false = 使用列主序(列主序情况下,内存访问连续,效率更高,但是这不是标准C语言支持的主序,需要手动实现内存策略)]
/// @param LDA 逻辑存储跨度,表示需要操作的范围的列数(行主序情况下)或行数(列主序情况下),单位: 个元素,无要求的话,为输入矩阵的列数(当major_default = true)
/// @return 当前乘数,如果需要的话,可以存储在外部,如存储在L矩阵用于LU消元过程
double matrix_only_row_elimination_step_fp64(double* A, int aim_row, int pivot_row, int pivot_col, bool major_default, int LDA) {
    double pivot = major_default ? A[pivot_row * LDA + pivot_col] : A[pivot_col * LDA + pivot_row];//获取主元值
    double aim = major_default ? A[aim_row * LDA + pivot_col] : A[pivot_col * LDA + aim_row];//获取目标值
    double multiplier = aim / pivot;//计算乘数
    //单步消元
    if (major_default) {
        //行主序,修改目标行的右侧元素
        for (int c = pivot_col + 1;c < LDA;c++) {
            A[aim_row * LDA + c] -= multiplier * A[pivot_row * LDA + c];
        }
    }
    else {
        //列主序,修改目标列的下方元素
        for (int r = pivot_row + 1;r < LDA;r++) {
            A[pivot_col * LDA + r] -= multiplier * A[pivot_col * LDA + pivot_row];
        }
    }
    return multiplier;
}


/// @brief [目前仅支持行主序]矩阵分解-LU分解(带部分主元法,支持非方阵,自动解决主元为0情况而不报错)(不会修改源数据矩阵)
/// @param A 要进行分解的矩阵(m x n)
/// @param L 下三角/梯形矩阵 L (m x min(m,n)) 输出到
/// @param U 上三角/梯形矩阵 U (min(m,n) x n) 输出到
/// @param P 置换数组 P (m) (存储为普通int一维数组,大小为m) 输出到 [置换矩阵P记录行交换索引,本函数运行完后,有P[原行号]=现行号(若不需要可以设置为NULL以禁止输出矩阵P,但是其他任何操作正常执行,包括自动解决主元为0情况)]
/// @param m 矩阵A行数
/// @param n 矩阵A列数
/// @return [ESP_OK 运算完成]
/// @return [ESP_ERR_INVALID_ARG A为NULL / L为NULL / U为NULL / m <= 0 / n <= 0] 
esp_err_t matrix_decomposition_LU_fp64(double* A, double* L, double* U, int* P, int m, int n) {
    //入参检查
    ESP_RETURN_ON_FALSE(A && L && U && m > 0 && n > 0, ESP_ERR_INVALID_ARG, math_tools_TAG, "传入参数异常 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));

    //初始化
    if (P) {
        //若需要输出 矩阵P (当矩阵P不等于NULL),初始化P为单位置换矩阵,初始存储为原行号
        for (int r = 0;r < m;r++) {
            P[r] = r;
        }
    }
    //复制矩阵A到U以初始化U,初始化L矩阵为单位矩阵
    for (int r = 0;r < n;r++) {
        for (int c = 0;c < n;c++) {
            U[r * n + c] = A[r * n + c];
            L[r * n + c] = (r == c) ? 1.0f : 0.0f;
        }
    }
    //分解
    for (int k = 0;k < m && k < n;k++) {
        int pivot_row = matrix_get_the_row_of_aim_col_pivot_fp64(U, m, n, k, k);//获取主元行
        matrix_swap_rows_fp64(U, n, k, pivot_row);//矩阵U当前行与主元行交换
        if (k > 0)matrix_swap_rows_fp64(L, n, k, pivot_row);//矩阵L当前行与主元行交换(k=0的初始交换无意义)
        //若需要输出 矩阵P (当矩阵P不等于NULL),更新矩阵P(记录行交换索引,本函数运行完后,有P[原行号]=现行号)
        if (P) {
            int buf = P[k];
            P[k] = P[pivot_row];
            P[pivot_row] = buf;
        }
        //高斯消元,顺便填充L矩阵
        for (int r = k + 1;r < m;r++) {
            L[r * n + k] = matrix_only_row_elimination_step_fp64(U, r, k, k, true, n);//注意,这里主元行的行号为k,因为之前进行了行交换,现在实际主元在行号为k的行,不是pivot_row
        }
    }

    return ESP_OK;
}


/// @brief [目前仅支持行主序]矩阵(方阵)求解-LU求解(LU分解的原矩阵A必须为方阵n x n,否则无法支持)(不会修改源数据矩阵)
/// @param L (导入LU分解的输出结果)下三角矩阵L(n x n,并且对角线元素需要为1)
/// @param U (导入LU分解的输出结果)上三角矩阵U(n x n)
/// @param P (导入LU分解的输出结果)置换数组P(n) (设置为NULL表示明确无置换) (存储为普通int一维数组,大小为n) [置换矩阵P记录行交换索引,LU分解运行完后,有P[原行号]=现行号]
/// @param b 右端项向量b(n) (存储为普通double一维数组,大小为n)
/// @param x 解向量x(n) 输出到 (存储为普通double一维数组,大小为n)
/// @param n LU分解的原矩阵A边长
/// @return [ESP_OK 运算完成]
/// @return [ESP_ERR_INVALID_STATE 原矩阵奇异, 求解操作无法完成]
/// @return [ESP_ERR_INVALID_ARG L为NULL / U为NULL / P为NULL / b为NULL / X为NULL / m <= 0 / n <= 0] 
/// @return [ESP_ERR_NO_MEM  内存不足] 
esp_err_t matrix_square_solve_LU_fp64(const double* L, const double* U, const int* P, double* b, double* x, int n) {
    //入参检查
    ESP_RETURN_ON_FALSE(L && U && b && x && n > 0, ESP_ERR_INVALID_ARG, math_tools_TAG, "传入参数异常 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));

    //内存申请
    double* y = (double*)heap_caps_aligned_alloc(16, n * sizeof(double), MATH_TOOLS_MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(y, ESP_ERR_NO_MEM, math_tools_TAG, "内存不足 描述%s", esp_err_to_name(ESP_ERR_NO_MEM));

    //前代求解 Ly = Pb
    for (int r = 0;r < n;r++) {
        y[r] = P ? b[P[r]] : b[r];
        for (int c = 0;c < r;c++) {
            y[r] -= L[r * n + c] * y[c];
        }
    }

    //回代求解 Ux = y
    for (int i = n - 1;i >= 0;i--) {
        if (fabs(U[i * n + i]) < 1e-10f) {
            heap_caps_free(y);
            y = NULL;
            ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_STATE, math_tools_TAG, "原矩阵奇异,求解操作无法完成(回代求解 Ux = y,i=%d时发现U[i * %d + i]=%f) 描述%s", i, n, U[i * n + i], esp_err_to_name(ESP_ERR_INVALID_STATE));
        }
        x[i] = y[i];
        for (int j = i + 1;j < n;j++) {
            x[i] -= U[i * n + j] * x[j];
        }
        x[i] /= U[i * n + i];
    }

    //内存释放,返回
    heap_caps_free(y);
    y = NULL;
    return ESP_OK;
}



/// @brief [目前仅支持行主序]矩阵(方阵)求逆-LU求逆(不会修改源数据矩阵)
/// @param A 输入原矩阵A (n x n)
/// @param inv_A 输出逆矩阵inv_A (n x n)
/// @param n 原矩阵A 的边长 
/// @return [ESP_OK 运算完成]
/// @return [ESP_ERR_INVALID_STATE 原矩阵奇异, 求逆操作无法完成]
/// @return [ESP_ERR_INVALID_ARG A为NULL / inv_A为NULL / n <= 0] 
/// @return [ESP_ERR_NO_MEM  内存不足] 
esp_err_t matrix_inverse_LU_fp64(const double* A, double* inv_A, int n) {
    //入参检查
    ESP_RETURN_ON_FALSE(A && inv_A && n > 0, ESP_ERR_INVALID_ARG, math_tools_TAG, "传入参数异常 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));

    //申请临时内存
    double* L = (double*)heap_caps_aligned_alloc(16, n * n * sizeof(double), MATH_TOOLS_MALLOC_CAP_DEFAULT);
    double* U = (double*)heap_caps_aligned_alloc(16, n * n * sizeof(double), MATH_TOOLS_MALLOC_CAP_DEFAULT);
    int* P = (int*)heap_caps_aligned_alloc(16, n * sizeof(int), MATH_TOOLS_MALLOC_CAP_DEFAULT);
    double* b = (double*)heap_caps_aligned_alloc(16, n * sizeof(double), MATH_TOOLS_MALLOC_CAP_DEFAULT);
    if (!L || !U || !P || !b) {
        heap_caps_free(L);
        heap_caps_free(U);
        heap_caps_free(P);
        heap_caps_free(b);
        L = NULL;
        U = NULL;
        P = NULL;
        b = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NO_MEM, math_tools_TAG, "内存不足 描述%s", esp_err_to_name(ESP_ERR_NO_MEM));
    }
    memset(b, 0.0f, n * sizeof(double));//仅填充b,不需填充L,U,P,因为运行LU分解会完全填充L,U,P

    //运行LU分解
    matrix_decomposition_LU_fp64(A, L, U, P, n, n);

    //求逆
    for (int c = 0;c < n;c++) {
        //对单位矩阵拆分为n列,作为右端项向量b求解即可求逆
        //因为单位矩阵的每一列都是固定的, 直接确定当前右端项向量b
        //注意,此处求解结果填充存在隐式转置,逐列求解结果应按列填充,但由于默认行主序,因此我们会暂时行填充,之后转置
        b[c] = 1.0f;
        if (c != 0)b[c - 1] = 0.0f;//如果之前其他位置置1,取消之前的置1
        esp_err_t solve_ret = matrix_square_solve_LU_fp64(L, U, P, b, &inv_A[c * n], n);//注意,inv_A存在隐式转置(列求解结果暂时强制行填充)
        if (solve_ret != ESP_OK) {
            heap_caps_free(L);
            heap_caps_free(U);
            heap_caps_free(P);
            heap_caps_free(b);
            L = NULL;
            U = NULL;
            P = NULL;
            b = NULL;
            ESP_RETURN_ON_FALSE(false, solve_ret, math_tools_TAG, "求解时发现问题 描述%s", esp_err_to_name(solve_ret));
        }
    }
    square_matrix_inplace_transpose_fp64(inv_A, n);//原地转置,解决LU求解时的隐式转置问题


    //[5.内存释放,返回]
    heap_caps_free(L);
    heap_caps_free(U);
    heap_caps_free(P);
    heap_caps_free(b);
    L = NULL;
    U = NULL;
    P = NULL;
    b = NULL;
    return ESP_OK;
}

/// @brief 矩阵在日志中打印出来
/// @note  输入矩阵为NULL时,会只打印NULL字符
/// @param A 输入需要打印的矩阵(输入矩阵为NULL时,会只打印NULL字符)
/// @param m 需要打印的矩阵的行数
/// @param n 需要打印的矩阵的列数
/// @param major_default 是否使用默认主序(行主序),无要求的话填写true  [ture = 使用行主序(C语言惯用,默认) / false = 使用列主序(这不是标准C语言支持的主序,需要手动实现内存策略)]
/// @param show_rc 显示元素的行和列(坐标表示),假设A中一个元素1.123456,它在第2行第3列,当show_rc设为true,显示为(2,3)[1.123456]
void matrix_log_print_fp64(const double* A, int m, int n, bool major_default, bool show_rc) {
    if (A != NULL) {
        for (int r = 0;r < m;r++) {
            for (int c = 0;c < n;c++) {
                if (major_default) {
                    if (show_rc)printf("(%d,%d)", r + 1, c + 1);
                    printf("[%f] ", A[r * n + c]);
                }
                else {
                    if (show_rc)printf("(%d,%d)", r + 1, c + 1);
                    printf("[%f] ", A[c * m + r]);
                }
            }
            printf("\n");//打印完成一行,换行
        }
    }
    else {
        printf("NULL\n");
    }
}




/// @brief [目前仅支持行主序]求解超定方程组 - 基于普通最小二乘法(OLS)实现的多元线性回归(MLR)
/// @note  [注意]当用于普通线性回归,多项式回归等需要获取截距beta[0]的情况,设计向量左侧必须为全一列以获取beta[0];需要强制无截距时,或者多项式方程中有其他偏移决定项(如椭球方程的一次项)而无需显式beta[0]的情况,设计向量全部都是直接填充正常样本数据即可
/// @brief [适用问题]适用于求解m个形如 y = beta[0] + beta[1]*x[1] + beta[2]*x[2] + ... + beta[n]*x[n] (可以没有beta[0]) 的方程构成的超定方程组 X * beta = y (超定方程组即方程组满足m>n,但建议要有m>>n以避免过拟合)
/// @brief [适用领域]可由于传感器校准(椭球方程求逆等)和数据分析,机器学习等领域
/// @brief [运算原理] 对于超定方程组, 给定 X* beta = y, 最小二乘解即为 beta = (XT * X)^{-1} * XT * y, 其中T代表转置 , ^{-1}代表矩阵求逆(inverse)运算
/// @param X 设计矩阵 (m x n,必须有m>n,并且建议要有m>>n以避免过拟合,当需要获取截距beta[0]时,左侧必须为全一列以获取beta[0])
/// @param y 观测向量 (m x 1)
/// @param beta 输出参数向量 (n x 1) , 其结构和意义由设计向量决定
/// @param m 设计矩阵的行数(样本个数/方程个数) 
/// @param n 设计矩阵的列数(参数个数,列数,当需要获取截距beta[0]时,beta[0]占一个参数个数)
/// @return [ESP_OK 运算完成]  
/// @return [ESP_ERR_INVALID_ARG X为NULL / y为NULL / beta为NULL / m <= 0 / n <= 0] 
/// @return [ESP_ERR_INVALID_STATE 运算时失败,矩阵 XT * X 无法求逆]
/// @return [ESP_ERR_NOT_SUPPORTED m<=n 的方程组]
/// @return [ESP_ERR_NO_MEM  内存不足] 
esp_err_t solve_overdet_system_ols_mlr_fp64(const double* X, const double* y, double* beta, int m, int n) {
    //入参检查
    ESP_RETURN_ON_FALSE(X && y && beta && m > 0 && n > 0, ESP_ERR_INVALID_ARG, math_tools_TAG, "传入参数异常 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));
    ESP_RETURN_ON_FALSE(m > n, ESP_ERR_NOT_SUPPORTED, math_tools_TAG, "不支持 m<=n 的方程组 描述%s", esp_err_to_name(ESP_ERR_NOT_SUPPORTED));

    // [运算原理] 对于超定方程组, 给定 X* beta = y, 最小二乘解即为 beta = (XT * X)^{-1} * XT * y, 其中T代表转置 , ^{-1}代表矩阵求逆(inverse)运算

    //申请内存
    double* XT = (double*)heap_caps_aligned_alloc(16, n * m * sizeof(double), MATH_TOOLS_MALLOC_CAP_DEFAULT); // XT
    double* XTy = (double*)heap_caps_aligned_alloc(16, n * sizeof(double), MATH_TOOLS_MALLOC_CAP_DEFAULT);// XT * y
    double* XTX = (double*)heap_caps_aligned_alloc(16, n * n * sizeof(double), MATH_TOOLS_MALLOC_CAP_DEFAULT);// XT * X
    double* inverse_XTX = (double*)heap_caps_aligned_alloc(16, n * n * sizeof(double), MATH_TOOLS_MALLOC_CAP_DEFAULT);// (XT * X)^{-1}
    if (!XT || !XTy || !XTX || !inverse_XTX) {
        heap_caps_free(XT);
        heap_caps_free(XTy);
        heap_caps_free(XTX);
        heap_caps_free(inverse_XTX);
        XT = NULL;
        XTy = NULL;
        XTX = NULL;
        inverse_XTX = NULL;
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NO_MEM, math_tools_TAG, "内存不足 描述%s", esp_err_to_name(ESP_ERR_NO_MEM));
    }
    memset(XT, 0, n * m * sizeof(double));
    memset(XTy, 0, n * sizeof(double));
    memset(XTX, 0, n * n * sizeof(double));
    memset(inverse_XTX, 0, n * n * sizeof(double));

    //计算 XT (T代表转置)
    matrix_transpose_fp64(X, XT, m, n);

    //计算 XT * y (T代表转置)
    matrix_fp64_mult_ansi(XT, y, XTy, n, m, 1);

    //计算 XT * X (T代表转置)
    matrix_fp64_mult_ansi(XT, X, XTX, n, m, n);

    //计算 (XT * X)^{-1} (T代表转置,^{-1}代表矩阵求逆)
    esp_err_t inv_ret = matrix_inverse_LU_fp64(XTX, inverse_XTX, n);
    if (inv_ret != ESP_OK) {
        heap_caps_free(XT);
        heap_caps_free(XTy);
        heap_caps_free(XTX);
        heap_caps_free(inverse_XTX);
        XT = NULL;
        XTy = NULL;
        XTX = NULL;
        inverse_XTX = NULL;
        ESP_RETURN_ON_FALSE(false, inv_ret, math_tools_TAG, "运算时失败,矩阵 XT * X 无法求逆 描述%s", esp_err_to_name(inv_ret));
    }

    //计算(XT * X) ^ {-1} * XT * y , 其结果为beta
    matrix_fp64_mult_ansi(inverse_XTX, XTy, beta, n, n, 1);

    //释放内存
    heap_caps_free(XT);
    heap_caps_free(XTy);
    heap_caps_free(XTX);
    heap_caps_free(inverse_XTX);
    XT = NULL;
    XTy = NULL;
    XTX = NULL;
    inverse_XTX = NULL;

    //运算完成,返回 ESP_OK
    return ESP_OK;
}


/// @brief [目前仅支持行主序]残差评估 - 获取任意线性模型的残差
/// @note  运算过程中的X * beta临时值暂时会在r上缓存以节省内存,这不会产生任何其他影响或需求,正常传入残差向量(m x 1)存储位置即可
/// @brief [适用问题]判断线性运算结果的可靠性
/// @brief [适用领域]可由于如传感器校准(椭球拟合结果可靠性验证等)和数据分析,机器学习等领域
/// @brief [运算原理] 残差r = X * beta - y
/// @param r 残差向量 (m x 1) 存储到 
/// @param SSR 残差平方和(double数值) 存储到 
/// @param X 设计矩阵 (m x n)
/// @param y 观测向量 (m x 1)
/// @param beta 结果参数向量 (n x 1) , 其结构和意义由设计向量决定
/// @param m 设计矩阵的行数(样本个数/方程个数) 
/// @param n 设计矩阵的列数(参数个数,列数)
/// @return [ESP_OK 运算完成]  
/// @return [ESP_ERR_NO_MEM  内存不足] 
/// @return [ESP_ERR_INVALID_ARG X为NULL / y为NULL / beta为NULL / m <= 0 / n <= 0] 
esp_err_t appraisal_residual_linear_model_fp64(double* r, double* SSR, const double* X, const double* y, double* beta, int m, int n) {
    //入参检查
    ESP_RETURN_ON_FALSE(r && X && y && beta && m > 0 && n > 0, ESP_ERR_INVALID_ARG, math_tools_TAG, "传入参数异常 描述%s", esp_err_to_name(ESP_ERR_INVALID_ARG));

    //[运算原理] 残差r = X * beta - y

    //X * beta
    matrix_fp64_mult_ansi(X, beta, r, m, n, 1);//暂时在r上缓存以节省内存

    //X * beta - y
    matrix_fp64_sub_ansi(r, y, r, m, 1, 1, 1);

    //计算残差平方和 - SSR (平方和就等于r与自身的点积)
    matrix_fp64_dotprod_ansi(r, r, SSR, m);

    return ESP_OK;
}