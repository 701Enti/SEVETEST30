// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//这是一个已修改的文件,非常感谢原作者!
//在原程序基础上,类型为float的输入/输出参数被修改为double类型以提高运算精度
//值得明确的是修改为double后,运算需要的时间可能延长
//函数名中的"_f32"被删除, "dspm"或"dsps"改为"matrix_fp64"
//文件重命名：dspm_mult_f32_ansi.c → matrix_fp64_mult_ansi.c
//注释了原文件所有包含,仅包含接口头文件matrix_fp64.h
//为了明确原作者信息,此文件API帮助及相关内容不在SEVETEST30文档中显示
//(修改者: 701Enti)

// #include "dsps_dotprod.h"
// #include "dspm_mult.h"

#include "matrix_fp64.h"

// Matrinx A(m,n), m - amount or rows, n - amount of columns
// C(m,k) = A(m,n)*B(n,k)
// c(i,j) = sum(a(i,s)*b(s,j)) , s=1..n
esp_err_t matrix_fp64_mult_ansi(const double* A, const double* B, double* C, int m, int n, int k)
{
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < k; j++) {
            C[i * k + j] = A[i * n] * B[j];
            for (int s = 1; s < n; s++) {
                C[i * k + j] += A[i * n + s] * B[s * k + j];
            }
        }
    }
    return ESP_OK;
}
