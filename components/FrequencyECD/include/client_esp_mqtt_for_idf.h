
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
 // ESP-MQTT的客户端实例化配置工作,同时支持PsP2P_DM,创建PsP2P_DM节点完成主动数据聚合,外部程序能够无需API操作即可完成MQTT传输
 // 注意:需要连接到外部MQTT的服务器,如内网开发机(需要完成MQTT的broker部署)或者云broker(需要根据提供商的协议接入,或者使用提供商的MQTT客户端SDK实现)
 // "for_idf"表示此版本为使用ESP-IDF开发平台的设备提供
 // 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
 // github: https://github.com/701Enti
 // bilibili: 701Enti

