# 文档相关

## 在 Github 上可访问/编辑的 SEVETEST30 文档仓库

- **仓库链接**: https://github.com/701Enti/DOC-SEVETEST30
- **文档撰写方式**: Doxygen 根据代码和注释生成
- **特别声明**:
  - SEVETEST30 文档仓库不是私有仓库,您完全可以 push 个人分支生成的文档到独立分支,甚至可以是您对 SEVETEST30 的见解等其他任何与 SEVETEST30 相关的内容,只要您没有影响自动文档或进行不合理的推送
  - 为保证资源充足,只会为关键分支提供自动文档生成,具体会触发工作流的关键分支在工作流文件 DoxygenAutoDocSync.yml 中可设置和查看
  - 当然,如果您希望自己的分支像关键分支一样可云端部署文档,您可以在本地运行 Doxygen 生成文档,并推送到 SEVETEST30 文档仓库与个人分支同名的分支,之后可联系 SEVETEST30 团队协商,他们管理文档的云端部署
- **文档修改**:
  - 更改 SEVETEST30 仓库的源码和注释内容即可,因为生成就是根据这些完成的,代码即文档
  - 由于生成结果文件非常多,不适合编辑修改管理工作,并且生成后的推送会覆盖之前的所有内容,更改源码注释反而更可靠,因此,目前不考虑生成后文件内容再修改的工作,即使是发布分支
- **相关技术实现**:
  - 当 SEVETEST30 仓库的关键分支有新的 push/pull&request 时(或者手动触发),自动在 Github 工作流中使用 Doxygen 生成文档,并提交新文档到文档仓库的同名分支
  - 例如 SEVETEST30 名为 A 分支的推送,将触发文档仓库名为 A 分支的更新
  - 文档生成和推送工作使用可复用工作流: https://github.com/701Enti/Github-WorkFlows/blob/master/.github/workflows/doxygen-auto-code-to-doc.yml (它存储在 701Enti 的专门工作流文件存储库)
  - 为保证资源充足,只会为关键分支提供自动文档生成,具体会触发工作流的关键分支在工作流文件 DoxygenAutoDocSync.yml 中可设置和查看
- **相关工作流**:
  - .github/workflows/DoxygenAutoDocSync.yml
  - https://github.com/701Enti/Github-WorkFlows/blob/master/.github/workflows/doxygen-auto-code-to-doc.yml

# 安全设施

## .gitignore 敏感文件排除

- **SDK 配置文件 - sdkconfig**

  - SEVETEST30 的 WIFI 连接密码和 API 相关密钥均需要在 SDK 配置编辑器(menuconfig)设置,其生成的 sdkconfig 硬编码这些敏感内容
  - sdkconfig 及其任意扩展名的文件均被排除,包括 sdkconfig.default
  - 在本仓库任何分支中都请不要使用 sdkconfig.default 配置默认值,请在./main/Kconfig.prebuild 直接配置默认值,它更易维护

- **其他常见敏感文件**:
  - 环境变量和其他配置文件（含 API 密钥/数据库密码）
  - 加密密钥和证书文件（SSL/SSH 私钥）
  - 云服务凭证文件（AWS/GCP 访问密钥）
  - 数据库文件和备份（可能含敏感数据）
  - 日志文件（可能泄露调试信息）
  - 开发工具配置文件（含私有仓库令牌）
  - 网络连接配置文件（VPN/RDP 凭证）

## 关键分支的 PR 检查(基于 TruffleHog)

- **相关技术实现**:

  - 当 SEVETEST30 仓库的关键分支有新的 pull&request 时,必须经过 Github 工作流检查才可以进行 pull&request(需要仓库设置规则)
  - 工作流中使用 TruffleHog 检查整个提交历史是否含敏感内容
  - 也可以手动触发来人工检查结果

- **相关工作流**:
  - .github/workflows/TruffleHogPRCheck.yml

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

## main/AGS10.c 和 components/sevetest30_board/include/AGS10.h 的 Calc_CRC8 函数

- **原文件**:来自奥松电子官方的 AGS10 数据手册内(需登录后点击"文件下载"获取手册) https://www.aosong.com/Products/info.aspx?lcid=&proid=49
- **原文件协议类型**: (未发现声明)
- **修改说明**:
  - 添加日志打印相关

## [^1]: 值得明确的是修改为 double 后,运算需要的时间可能延长

## [^2]: 在原程序基础上,取消了 base64 编码的"\n"操作,因为将音频编码为 base64 时添加"\n"引起了一些 API 服务无法识别的问题

## [^3]: 这个修改为适应 API 服务的一个项目需求, 不是否认原作者设计的可靠性, 事实上添加"\n"是一个正常编码操作

# 法律声明

1. 所有文件头部均已保留原始版权声明
2. 完整协议文本请通过附加的相关链接查看
3. 详细修改记录见各文件头部注释块
4. 为了明确第三方代码的原作者信息,相关内容及 API 帮助不在 SEVETEST30 文档中显示
