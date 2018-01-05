/**
 * Copyright (c) 2017-2018 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file service.h
 *
 * @breif Expose main entry function to other modules
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 21:02:28 January 4, 2018 GTM+8
 *
 * */
#ifndef SERVICE_H
#define SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int service_cmd_parse(uint8_t *msg, size_t mes_len);
int service_send_error_log(const char *msg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SERVICE_H */

