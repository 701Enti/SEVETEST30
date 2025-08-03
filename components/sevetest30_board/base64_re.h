/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

 //这是一个已修改的文件,非常感谢原作者!
 //在原程序基础上,与已修改内容有关的函数添加了"_re"后缀,文件名从"base64.h"改为"base64_re.h"
 //取消了修改者的项目没有使用到的函数声明
 //为了明确原作者信息,此文件API帮助及相关内容不在SEVETEST30文档中显示
 //原作者项目README:https://w1.fi/cgit/hostap/plain/hostapd/README
 //原作者项目URL地址:https://w1.fi/hostapd/
 //(修改者: 701Enti)

#ifndef BASE64_RE_H
#define BASE64_RE_H

char* base64_encode_re(const void* src, size_t len, size_t* out_len);
char* base64_url_encode_re(const void* src, size_t len, size_t* out_len);

#endif /* BASE64_H */
