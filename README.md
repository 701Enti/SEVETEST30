# 简介

- 具有 12\*24RGB 显示阵列和大量环境传感器的物联网 AI 个性化时钟
- 基于 ESP32S3 模组和 ESP-IDF 框架开发
- 仅 TYPE-C 接入即可实现充电和程序调试(支持接口静电保护)
- 板载电池和充电管理
- 支持电池电量计量
- 只需连接互联网即可获取设备位置并同步天气数据
- 支持英语,中文等多语言显示
- 最高支持 24bit 高保真音频播放/录制(DAC 最高支持 192kHz,ADC 最高支持 96kHz)
- 具有 3W 双声道音频功率放大器
- 提供耳机输出
- 软件音量调节并支持蓝牙控制
- 环境光传感
- 板载麦克风阵列
- 姿态传感
- 地磁传感
- 线性振动马达
- 关机后继续计时(通过板载电池供电)
- TVOC 空气质量传感
- 温湿度传感
- 支持语音识别 ASR
- 支持语音合成 TTS
- 支持百度文心一言语音聊天交互
- 支持 DeepSeek 语音聊天交互(等待开发中)
- 通过物联工具实现个性化定制(等待开发中)
- 交互式开关机(等待开发和优化中)

# 软件

- 业务代码编程语言为 C 语言
- 使用了 CMake 构建
- 基于 ESP-IDF 框架开发
- 本项目 IDE 开发环境推荐使用 Visual Studio Code

# 软件安全设施

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

# 硬件

- **控制板 - SEVETEST30 核心控制板(50mm \* 100mm)**:

  - (目前硬件调试中,以下芯片使用仅供参考,很可能随时终止或更换某些器件的使用,请勿用于现实业务,修改恕不通知,建议等待正式版发布)[^4]
  - (由于硬件更改的可能,硬件工程文件尚不公开分发,我们可能在正式版发布时正式发布相关硬件文件)
  - MCU 模组:ESP32-S3-WROOM-1(N16R8 版本)
  - 字库:GT32L32S0140
  - 音频编解码:ES8388
  - 音频功率放大器:NS4268
  - 数字电位器(音量调节):TPL0401B
  - IO 扩展器:TCA6416A
  - 环境光传感器:OPT3001
  - 麦克风阵列:ZTS6156
  - 姿态传感器:LSM6DS3TRC
  - 地磁传感器:HSCDTD008A
  - 线性振动马达
  - 线性振动马达驱动器:BD6210F-E2
  - 离线 RTC:BL5372
  - TVOC 空气质量传感器:AGS10
  - 温湿度传感器:AHT21
  - DC-DC 电池充放电管理:SLM6300
  - 电池电量计量:MAX17048G+T10
  - USB 接口保护:UBT26A05L03
  - 电池(3.7V) -> 辅助电源(5V) DC-DC 升压芯片:TPS61230ARNSR
  - 数字电位器(辅助电源电压下调):TPL0401A
  - 辅助电源(5V) -> 主电源(3.3V) LDO 降压芯片:CJT1117B-3.3
  - 主电源(3.3V) -> AGS10 供电(3v) LDO 降压芯片:H7130-1

- **显示板 - SEVETEST30 灯板(50mm \* 100mm)**:

  - 串行可控彩色 LED:WS2812

- **必要设备 I2C 硬件通信地址**:

  - ES8388 (音频编解码) - 10
  - TPL0401B(音量调整) - 3E
  - TPL0401A(5V 电压下调) - 2E
  - BL5372(离线 RTC) - 32
  - TCA6416A(IO 扩展) - 20
  - MAX17048(电量计量) - 36

- **自举设备 I2C 硬件通信地址**:

  - AHT21(温湿度传感) - 38
  - AGS10(空气质量传感) - 1A
  - OPT3001(环境光传感) - 44
  - LSM6DS3TR-C(姿态传感) - 6A
  - HSCDTD008A(地磁传感) - 0C

# 硬件安全设施

## 接口保护

- **USB 接口**:
  - 支持静电保护,通过 UBT26A05L03
  - 支持浪涌保护,通过 UBT26A05L03

# 文档相关

## 在 Github 上的 SEVETEST30 文档仓库

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
- **全流程案例**:
  - 以下是通过 Doxygen 自动生成官方文档的相关工作全流程(仅针对关键分支,如分支 A)
  - SEVETEST30 仓库关键分支 A 的 push
  - -> DoxygenAutoDocSync 触发
  - -> DoxygenAutoDocSync 调用 doxygen-auto-code-to-doc
  - -> doxygen-auto-code-to-doc 触发,根据 A 分支代码生成文档并将更改推送到文档仓库的 A 分支
  - (doxygen-auto-code-to-doc 结束)
  - -> DoxygenAutoDocSync 调用 OfficialDocSubmoduleSync
  - -> OfficialDocSubmoduleSync 触发,更新子模块,将更改推送到 A 分支的机器人分支并发起该分支到 A 分支的 PR
  - (OfficialDocSubmoduleSync 结束)
  - (DoxygenAutoDocSync 结束)
  - -> PR 审查 -> 完成
  - [注意: SEVETEST30 仓库是将每个文档仓库作为子模块独立管理的,子模块名必须与文档仓库名一致]
- **相关工作流**:
  - .github/workflows/DoxygenAutoDocSync.yml
  - .github/workflows/OfficialDocSubmoduleSync.yml
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

## main/AGS10.c 和 components/sevetest30_board/include/AGS10.h 的 Calc_CRC8 函数

- **原文件**:来自奥松电子官方的 AGS10 数据手册内(需登录后点击"文件下载"获取手册) https://www.aosong.com/Products/info.aspx?lcid=&proid=49
- **原文件协议类型**: (未发现声明)
- **修改说明**:
  - 添加日志打印相关

## [^1]: 值得明确的是修改为 double 后,运算需要的时间可能延长

## [^2]: 在原程序基础上,取消了 base64 编码的"\n"操作,因为将音频编码为 base64 时添加"\n"引起了一些 API 服务无法识别的问题

## [^3]: 这个修改为适应 API 服务的一个项目需求, 不是否认原作者设计的可靠性, 事实上添加"\n"是一个正常编码操作

## [^4]: 本项目开发者和贡献者不承担任何硬件更改活动导致的财产损失,信息泄露和人员伤亡(详见下方 法律声明 - 免责声明)

# 法律声明 - 版权相关

1. 所有文件头部均已保留原始版权声明
2. 完整协议文本请通过附加的相关链接查看
3. 详细修改记录见各文件头部注释块
4. 为了明确第三方代码的原作者信息,相关内容及 API 帮助不在 SEVETEST30 文档中显示

# 法律声明 - 免责声明

- 本项目的开发者及贡献者（以下简称“我们”）不对任何因使用本项目（包括但不限于硬件修改、软件配置或数据使用）导致的直接或间接损失负责，包括但不限于：
- 硬件损坏、财产损失或人身伤害；
- 数据泄露、隐私问题或系统故障；
- 因违反法律法规或第三方权利导致的后果。
- 使用本项目即代表您同意自行承担所有风险。我们不提供任何明示或默示的担保，包括适销性、特定用途适用性或无侵权保证。
- 如需正式法律条款，请参阅项目许可证（LICENSE 文件）或单独的法律文件。
