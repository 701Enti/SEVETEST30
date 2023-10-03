/* Play MP3 Stream from Baidu Text to Speech service

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
// 该文件被701Enti添加，以上是原文件要求保留的声明。利用ESP32S3的WIFI和I2S模块使用百度文字转语音TTS实现进一步语音处理，原例程是pipeline_baidu_speech_mp3
// 官方例程连接：https://github.com/espressif/esp-adf/tree/master/examples/cloud_services/pipeline_baidu_speech_mp3
// 敬告：使用了Espressif官方提供的pipeline_baidu_speech_mp3例程文件,非常感谢Espressif
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// 邮箱：   hi_701enti@yeah.net
// github: https://github.com/701Enti
// bilibili账号: 701Enti
// 美好皆于不懈尝试之中，热爱终在不断追逐之下！            - 701Enti  2023.7.26

#ifndef _API_BAIDUTTS_H_
#define _API_BAIDUTTS_H_
#endif

//TTS服务
void tts_service();

void init_i2c_and_audio_device();