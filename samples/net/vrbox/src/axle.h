/**
 * Copyright (c) 2017-2018 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file axle.h
 *
 * @brief Main axle of vrbox API
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 19:40:58 October 18, 2017 GTM+8
 *
 * */
#ifndef AXLE_H
#define AXLE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

/**
 * @brief Axle hardware initializer
 *
 * @return  0 Succeess
 *		   <0 Some error happened
 * */
int8_t axle_init(void);

/**
 * @brief Rotate the axle to particular position
 *
 * @param destination_position
 *		  The real position we want to go, value [1-7] is expected.
 * @return  0 Success
 *		   <0 Some error happened
 * */
int8_t axle_rotate_to(uint8_t destination_position);

/**
 * @brief Get axle functionality status
 *
 * @return  true, axle is fine
 *		    false, axle is broken
 * */
bool axle_get_status(void);

#ifdef CONFIG_APP_AXLE_FACTORY_TEST
int axle_ft_lock(void);
int axle_ft_unlock(void);
int axle_ft_rotate_desc(void);
int axle_ft_rotate_asc(void);
int axle_ft_rotate_stop(void);

int axle_ft_position(void);
int axle_ft_relocation(void);
int axle_ft_rotate_to(int position);
#endif /* CONFIG_APP_AXLE_FACTORY_TEST */

#ifdef CONFIG_APP_AXLE_DEBUG
void axle_debug(void);
#endif /* CONFIG_APP_AXLE_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AXLE_H */

