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
//文件重命名：dsps_mult_f32_ansi.c → matrix_fp64_mult_ansi.c
//添加接口头文件matrix_fp64.h的包含
//为了明确原作者信息,此文件API帮助及相关内容不在SEVETEST30文档中显示
//(修改者: 701Enti)

#include "dsps_sub.h"
#include "matrix_fp64.h"

esp_err_t matrix_fp64_sub_ansi(const double* input1, const double* input2, double* output, int len, int step1, int step2, int step_out)
{
    if (NULL == input1) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == input2) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }
    if (NULL == output) {
        return ESP_ERR_DSP_PARAM_OUTOFRANGE;
    }

    for (int i = 0; i < len; i++) {
        output[i * step_out] = input1[i * step1] - input2[i * step2];
    }
    return ESP_OK;
}
