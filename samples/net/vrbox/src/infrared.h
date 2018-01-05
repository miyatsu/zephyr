/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file infrared.h
 *
 * @brief In box infrared detector API
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 10:33:33 October 18, 2017 GTM+8
 *
 * */
#ifndef INFRARED_H
#define INFRARED_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>

/**
 * @brief infrared initial function
 *
 * Some of the gpio need to drain its default input.
 * More detial please refer to the comment within this function.
 *
 * @return  0, initial ok
 *		   -1, initial error
 * */
int8_t infrared_init(void);

/**
 * @brief Get infrared status array
 *
 * @return pointer of poiner point to infrared_status[4 * 7]
 * */
uint8_t* infrared_get_status_array(void);

#ifdef CONFIG_APP_INFRARED_FACTORY_TEST
uint8_t* infrared_ft_refresh(void);
#endif /* CONFIG_APP_INFRARED_FACTORY_TEST */

#ifdef CONFIG_APP_INFRARED_DEBUG
void infrared_debug(void);
#endif /* CONFIG_APP_INFRARED_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* INFRARED */

