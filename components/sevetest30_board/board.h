/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

 //这是一个已修改的文件,非常感谢原作者!
 //在原程序基础上,更改为项目需要的数值设置
 //为了明确原作者信息,此文件API帮助及相关内容不在文档中显示

#ifndef _AUDIO_BOARD_H_
#define _AUDIO_BOARD_H_

#include "audio_hal.h"
#include "board_def.h"
#include "board_ctrl.h"
#include "board_pins_config.h"
#include "esp_peripherals.h"
#include "esxxx_common.h"

#ifdef __cplusplus
extern "C" {
#endif    

    extern audio_hal_func_t AUDIO_CODEC_ES8388_DEFAULT_HANDLE;



    /**
     * @brief Audio board handle
     */
    struct audio_board_handle {
        audio_hal_handle_t audio_hal; /*!< audio hardware abstract layer handle */
    };

    typedef struct audio_board_handle* audio_board_handle_t;





    /// @brief (已修改) 初始化音频面板
    /// 
    /// @return 
    esp_err_t audio_board_init();

    /**
     * @brief Initialize codec chip
     *
     * @return The audio hal handle
     */
    audio_hal_handle_t audio_board_codec_init(void);

    /**
     * @brief Query audio_board_handle
     *
     * @return The audio board handle
     */
    audio_board_handle_t audio_board_get_handle(void);

    /**
     * @brief Uninitialize the audio board
     *
     * @param audio_board The handle of audio board
     *
     * @return  0       success,
     *          others  fail
     */
    esp_err_t audio_board_deinit(audio_board_handle_t audio_board);

    void codec_set_mic_gain(board_ctrl_t* board_ctrl);
    void codec_config_adc_input(board_ctrl_t* board_ctrl);
    void codec_config_dac_output(board_ctrl_t* board_ctrl);

#ifdef __cplusplus
}
#endif

#endif
