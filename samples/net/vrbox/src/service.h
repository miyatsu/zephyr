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

#include <stdbool.h>
#include <stddef.h>

int service_cmd_parse(uint8_t *msg, size_t mes_len);
int service_send_error_log(const char *msg);

#ifdef CONFIG_APP_FACTORY_TEST
bool service_cmd_is_factory_test(uint8_t*, size_t);
#endif /* CONFIG_APP_FACTORY_TEST */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SERVICE_H */

