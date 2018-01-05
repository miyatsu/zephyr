/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file headset.h
 *
 * @brief Main headset of vrbox API
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 12:02:41 November 10, 2017 GTM+8
 *
 * */
#ifndef HEADSET_H
#define HEADSET_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>

int8_t headset_buy(void);
int8_t headset_add(void);
int8_t headset_stock_init();
int8_t headset_init(void);
int8_t headset_get_stock(void);

#ifdef CONFIG_APP_HEADSET_FACTORY_TEST
int headset_ft_rotate(void);
int headset_ft_stop(void);
int headset_ft_push(void);
int headset_ft_infrared(void);
int headset_ft_accuracy(void);
#endif /* CONFIG_APP_HEADSET_FACTORY_TEST */

#ifdef CONFIG_APP_HEADSET_DEBUG
int8_t headset_debug(void);
#endif /* CONFIG_APP_HEADSET_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HEADSET_H */

