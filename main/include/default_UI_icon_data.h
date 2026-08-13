
/*
 * 701Enti MIT License
 *
 * Copyright © 2024 <701Enti organization>
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

// 默认图标库
// 如您发现一些问题，请及时联系我们，我们非常感谢您的支持
// github: https://github.com/701Enti

#pragma once

extern const unsigned char gImage_like[1544];
extern const unsigned char gImage_happy[1544];
extern const unsigned char gImage_angry[1544];
extern const unsigned char gImage_disgusting[1544];
extern const unsigned char gImage_fearful[1544];
extern const unsigned char gImage_sad[1544];
extern const unsigned char gImage_normal[1544];



extern const unsigned char weather_icon_100[251];
extern const unsigned char weather_icon_101[251];
extern const unsigned char weather_icon_102[251];
extern const unsigned char weather_icon_103[251];
extern const unsigned char weather_icon_104[251];
extern const unsigned char weather_icon_150[251];
extern const unsigned char weather_icon_151[251];
extern const unsigned char weather_icon_152[251];
extern const unsigned char weather_icon_153[251];
extern const unsigned char weather_icon_300[251];
extern const unsigned char weather_icon_301[251];
extern const unsigned char weather_icon_302[251];
extern const unsigned char weather_icon_303[251];
extern const unsigned char weather_icon_304[251];
extern const unsigned char weather_icon_305[251];
extern const unsigned char weather_icon_306[251];
extern const unsigned char weather_icon_307[251];
extern const unsigned char weather_icon_308[251];
extern const unsigned char weather_icon_309[251];
extern const unsigned char weather_icon_310[251];
extern const unsigned char weather_icon_311[251];
extern const unsigned char weather_icon_312[251];
extern const unsigned char weather_icon_313[251];
extern const unsigned char weather_icon_350[251];
extern const unsigned char weather_icon_351[251];
extern const unsigned char weather_icon_399[251];
extern const unsigned char weather_icon_400[251];
extern const unsigned char weather_icon_401[251];
extern const unsigned char weather_icon_402[251];
extern const unsigned char weather_icon_403[251];
extern const unsigned char weather_icon_404[251];
extern const unsigned char weather_icon_405[251];
extern const unsigned char weather_icon_406[251];
extern const unsigned char weather_icon_407[251];
extern const unsigned char weather_icon_456[251];
extern const unsigned char weather_icon_457[251];
extern const unsigned char weather_icon_499[251];
extern const unsigned char weather_icon_500[251];
extern const unsigned char weather_icon_501[251];
extern const unsigned char weather_icon_502[251];
extern const unsigned char weather_icon_503[251];
extern const unsigned char weather_icon_504[251];
extern const unsigned char weather_icon_507[251];
extern const unsigned char weather_icon_508[251];
extern const unsigned char weather_icon_509[251];
extern const unsigned char weather_icon_510[251];
extern const unsigned char weather_icon_511[251];
extern const unsigned char weather_icon_512[251];
extern const unsigned char weather_icon_513[251];
extern const unsigned char weather_icon_514[251];
extern const unsigned char weather_icon_515[251];
extern const unsigned char weather_icon_900[251];
extern const unsigned char weather_icon_901[251];
extern const unsigned char weather_icon_999[251];

int get_weather_icon_data(unsigned char icon_data[251], int index_id);