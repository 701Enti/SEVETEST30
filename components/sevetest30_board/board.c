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

#include "esp_log.h"
#include "esp_err.h"
#include "board.h"
#include "audio_mem.h"
#include "es8388.h"

 //这是一个已修改的文件,原作者信息见上方声明,在原程序基础上,
 //更改为项目需要的形式或设置
 //这个修改为适应硬件环境的一个项目需求, 不是否认原作者设计的可靠性
 //为了明确原作者信息,此文件API帮助及相关内容不在文档中显示

static const char* TAG = "SEVETEST30_BOARD";

static audio_board_handle_t board_handle = NULL;
extern audio_hal_func_t AUDIO_CODEC_ES8388_DEFAULT_HANDLE;

audio_board_handle_t audio_board_init(void)
{
    if (board_handle)
    {
        ESP_LOGW(TAG, "The board has already been initialized!");
        return board_handle;
    }
    board_handle = (audio_board_handle_t)audio_calloc(1, sizeof(struct audio_board_handle));
    AUDIO_MEM_CHECK(TAG, board_handle, return NULL);
    board_handle->audio_hal = audio_board_codec_init();

    return board_handle;
}

esp_err_t init_audio_codec()
{
    esp_err_t ret = ESP_OK;
    audio_board_handle_t board_handle = audio_board_get_handle();
    ret = audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    return ret;
}

audio_hal_handle_t audio_board_codec_init(void)
{
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_hal_handle_t codec_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES8388_DEFAULT_HANDLE);
    AUDIO_NULL_CHECK(TAG, codec_hal, return NULL);
    return codec_hal;
}

audio_board_handle_t audio_board_get_handle(void)
{
    return board_handle;
}

esp_err_t audio_board_deinit(audio_board_handle_t audio_board)
{
    esp_err_t ret = ESP_OK;
    ret = audio_hal_deinit(audio_board->audio_hal);
    audio_free(audio_board);
    board_handle = NULL;
    return ret;
}

void codec_set_mic_gain(board_ctrl_t* board_ctrl)
{
    if (es8388_set_mic_gain(board_ctrl->codec_adc_gain) != ESP_OK)
        ESP_LOGE("board", "设置麦克风增益发现问题");
}

void codec_config_adc_input(board_ctrl_t* board_ctrl)
{
    if (es8388_config_adc_input(board_ctrl->codec_adc_pin) != ESP_OK)
        ESP_LOGE("board", "设置麦克风端口发现问题");
}

void codec_config_dac_output(board_ctrl_t* board_ctrl)
{
    if (es8388_config_dac_output(board_ctrl->codec_dac_pin) != ESP_OK)
        ESP_LOGE("board", "设置音频输出端口发现问题");
}
