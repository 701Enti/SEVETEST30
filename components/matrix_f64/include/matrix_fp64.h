
// "fp64"表示相关计算使用double类型进行
// esp-dsp项目中的一些运算函数修改而来的double版本函数构成matrix_f64库,非常感谢原作者,具体声明见本项目README和相关源文件声明
// 相关函数类型为float的输入/输出参数被修改为 double 类型以提高运算精度
// 值得明确的是修改为double后, 运算需要的时间可能延长
// 函数名中的"_f32"被删除,"dspm"或"dsps"改为"matrix_fp64"

#pragma once

#include "esp_err.h"

esp_err_t matrix_fp64_mult_ansi(const double* A, const double* B, double* C, int m, int n, int k);

esp_err_t matrix_fp64_dotprod_ansi(const double* src1, const double* src2, double* dest, int len);

esp_err_t matrix_fp64_sub_ansi(const double* input1, const double* input2, double* output, int len, int step1, int step2, int step_out);