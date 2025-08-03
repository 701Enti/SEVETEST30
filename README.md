# 文档

## 在 Github 上可访问/编辑的 SEVETEST30 文档仓库

- **仓库链接**: https://github.com/701Enti/DOC-SEVETEST30
- **文档撰写方式**: Doxygen 生成 + 贡献者修改
- **相关技术实现**: 当 SEVETEST30 仓库有新的推送时,自动在 Github 工作流中使用 Doxygen 生成文档,并提交新文档到文档仓库
- **相关工作流**:
  - https://github.com/701Enti/SEVETEST30/blob/develop/.github/workflows/DoxygenAutoDocSync.yml
  - https://github.com/701Enti/Github-WorkFlows/blob/master/.github/workflows/doxygen-auto-code-to-doc.yml

# 第三方代码使用声明

## components/matrix_f64/matrix_fp64_mult_ansi.c

- **原项目**: esp-dsp
- **项目链接**: https://github.com/espressif/esp-dsp
- **原文件**: dspm_mult_f32_ansi.c
- **原文件协议类型**: Apache License 2.0
- **原文件 LICENSE 链接**: http://www.apache.org/licenses/LICENSE-2.0
- **修改说明**:
  - 类型为 float 的输入/输出参数被修改为 double 类型以提高运算精度[^1]
  - 函数名中的"\_f32"被删除,"dspm"或"dsps"改为"matrix_fp64"
  - 文件重命名：dspm_mult_f32_ansi.c → matrix_fp64_mult_ansi.c
  - 注释了原文件所有包含,仅包含接口头文件 matrix_fp64.h

## components/matrix_f64/matrix_fp64_sub_ansi.c

- **原项目**: esp-dsp
- **项目链接**: https://github.com/espressif/esp-dsp
- **原文件**: dsps_sub_f32_ansi.c
- **原文件协议类型**: Apache License 2.0
- **原文件 LICENSE 链接**: http://www.apache.org/licenses/LICENSE-2.0
- **修改说明**:
  - 类型为 float 的输入/输出参数被修改为 double 类型以提高运算精度[^1]
  - 函数名中的"\_f32"被删除,"dspm"或"dsps"改为"matrix_fp64"
  - 文件重命名：dsps_sub_f32_ansi.c → matrix_fp64_sub_ansi.c
  - 添加接口头文件 matrix_fp64.h 的包含

## components/matrix_f64/matrix_fp64_dotprod_ansi.c

- **原项目**: esp-dsp
- **项目链接**: https://github.com/espressif/esp-dsp
- **原文件**: dsps_dotprod_f32_ansi.c
- **原文件协议类型**: Apache License 2.0
- **原文件 LICENSE 链接**: http://www.apache.org/licenses/LICENSE-2.0
- **修改说明**:
  - 类型为 float 的输入/输出参数被修改为 double 类型以提高运算精度[^1]
  - 函数名中的"\_f32"被删除,"dspm"或"dsps"改为"matrix_fp64"
  - 文件重命名：dspm_sub_f32_ansi.c → matrix_fp64_dotprod_ansi.c
  - 注释了原文件所有包含,仅包含接口头文件 matrix_fp64.h

## components/base64_re/base64_re.c

- **原项目**: hostapd
- **项目链接**: https://w1.fi/hostapd/
- **项目 README 链接**: https://w1.fi/cgit/hostap/plain/hostapd/README
- **原文件**: base64.c
- **原文件协议类型**: BSD 3-Clause License
- **修改说明**:
  - 取消了 base64 编码的"\n"操作以满足特殊需要[^2][^3]
  - 文件重命名：base64.c → base64_re.c
  - 与已修改内容有关的函数添加了"\_re"后缀

## components/base64_re/include/base64_re.h

- **原项目**: hostapd
- **项目链接**: https://w1.fi/hostapd/
- **项目 README 链接**: https://w1.fi/cgit/hostap/plain/hostapd/README
- **原文件**: base64.h
- **原文件协议类型**: BSD 3-Clause License
- **修改说明**:
  - 文件重命名：base64.h → base64_re.h
  - 与已修改内容有关的函数添加了"\_re"后缀
  - 取消了修改者的项目没有使用到的函数声明

## components/sevetest30_board/board_def.h

- **原项目**: esp-adf
- **项目链接**: https://github.com/espressif/esp-adf
- **项目 LICENSE 链接**: https://github.com/espressif/esp-adf/blob/master/LICENSE
- **原文件**: board_def.h
- **原文件协议类型**: MIT
- **修改说明**:
  - 更改为项目需要的数值设置
  - 添加一些项目个性化需要的定义

## components/sevetest30_board/board_pins_config.c

- **原项目**: esp-adf
- **项目链接**: https://github.com/espressif/esp-adf
- **项目 LICENSE 链接**: https://github.com/espressif/esp-adf/blob/master/LICENSE
- **原文件**: board_pins_config.c
- **原文件协议类型**: MIT
- **修改说明**:
  - 更改为项目需要的数值设置

## components/sevetest30_board/board_pins_config.h

- **原项目**: esp-adf
- **项目链接**: https://github.com/espressif/esp-adf
- **项目 LICENSE 链接**: https://github.com/espressif/esp-adf/blob/master/LICENSE
- **原文件**: board_pins_config.h
- **原文件协议类型**: MIT
- **修改说明**:
  - 更改为项目需要的数值设置

## components/sevetest30_board/board.c

- **原项目**: esp-adf
- **项目链接**: https://github.com/espressif/esp-adf
- **项目 LICENSE 链接**: https://github.com/espressif/esp-adf/blob/master/LICENSE
- **原文件**: board.c
- **原文件协议类型**: MIT
- **修改说明**:
  - 更改为项目需要的数值设置

## components/sevetest30_board/board.h

- **原项目**: esp-adf
- **项目链接**: https://github.com/espressif/esp-adf
- **项目 LICENSE 链接**: https://github.com/espressif/esp-adf/blob/master/LICENSE
- **原文件**: board.h
- **原文件协议类型**: MIT
- **修改说明**:
  - 更改为项目需要的数值设置

## [^1]: 值得明确的是修改为 double 后,运算需要的时间可能延长

## [^2]: 在原程序基础上,取消了 base64 编码的"\n"操作,因为将音频编码为 base64 时添加"\n"引起了一些 API 服务无法识别的问题

## [^3]: 这个修改为适应 API 服务的一个项目需求, 不是否认原作者设计的可靠性, 事实上添加"\n"是一个正常编码操作

# 法律声明

1. 所有文件头部均已保留原始版权声明
2. 完整协议文本请通过附加的相关链接查看
3. 详细修改记录见各文件头部注释块
4. 为了明确第三方代码的原作者信息,相关内容及 API 帮助不在 SEVETEST30 文档中显示
