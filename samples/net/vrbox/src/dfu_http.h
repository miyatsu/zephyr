/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file dfu_http.h
 *
 * @brief DFU related API
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 15:45:22 December 6, 2017 GTM+8
 *
 * */
#ifndef DFU_HTTP_H
#define DFU_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Download firmware via HTTP
 *
 * This function will download the firmware using the @uri that caller give.
 * Firmware will automaticly write into image-slot1 and wait to be checked
 * and upgrede from it.
 *
 * @param uri The download url of firmware
 *
 * @param uri_len The length of the download url
 *
 * @return	 0 Download success
 *		   < 0 Some error happened, may write flash error
 * */
int dfu_http_download(const char *uri, size_t uri_len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DFU_HTTP_H */

