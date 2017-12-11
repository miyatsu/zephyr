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

/**
 * @brief Check if the downloaded firmware match the given md5 value
 *
 * @param firmware_size Firmware size that downloaded
 *
 * @param md5_str The firmware remote md5 string
 *
 * @return 0 Local md5 value match the remote md5 value
 *		  -1 Local md5 value NOT match the remote md5 value
 * */
int dfu_md5_check(size_t firmware_size, const char *md5_str);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DFU_HTTP_H */

