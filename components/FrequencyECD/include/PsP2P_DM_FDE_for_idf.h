
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
 // PsP2P_DM_FDE是一个PsP2P_DM的快速开发扩展库 (伪P2P数据市场 - PsP2P_DM(Pseudo-P2P Data Market) 是一个借鉴P2P设计的单机本地数据通信框架)
 // "for_idf"表示此版本为使用ESP-IDF开发平台的设备提供
 // 敬告：本库是一个扩展库,其中代码逻辑为快速开发设计,可能不适用于一些情况,其中存在的一些特殊检查逻辑,参数限制,实现方式等,可能不是PsP2P_DM的原生设定,请注意甄别
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti

#pragma once

#include "PsP2P_DM_for_idf.h"


#define PSP2P_DM_NODE_MESSAGE_MAX_ACK_WAIT_TIME_MS 5000
#define PSP2P_DM_NODE_MESSAGE_MAX_RESULT_WAIT_TIME_MS 5000



esp_err_t PsP2P_DM_FDE_Producer_destory(PsP2P_DM_node_handle_t producer);

