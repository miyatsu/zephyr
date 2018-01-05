/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file door.h
 *
 * @brief Main door of vrbox API
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 17:35:08 October 20, 2017 GTM+8
 *
 * */
#ifndef DOOR_H
#define DOOR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Door hardware initial function
 *
 * @return  0 Initial OK
 *		   <0 Initial Failed
 * */
int8_t door_init(void);

/**
 * @brief Get door functionality status
 *
 * @return pointer point to door status array
 * */
bool* door_get_status_array(void);

/**
 * @brief Public wrapper function of door open
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return	 0 Door has fully opened
 *			-1 Door open timeout
 *			-2 Door never opened
 *			-3 Door half opened
 *			-4 Door is at opened AND closed state, impossible
 * */
int door_open(uint8_t layer);

/**
 * @brief Public wrapper function of door close
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return	 1 On door infrared detected
 *			 0 Door has fully closed
 *			-1 Door close timeout
 *			-2 Door never closed
 *			-3 Door half closed
 *			-4 Door is at opened AND closed state, impossible
 * */
int door_close(uint8_t layer);

/**
 * @brief Open all four doors at a time
 *
 * @return  0 Door open success
 *		   -1 Door open error
 * */
int8_t door_admin_open(void);

/**
 * @brief Close all four doors at a time
 *
 * @return  0 Door close success
 *		   -1 Door close error
 * */
int8_t door_admin_close(void);

#ifdef CONFIG_APP_DOOR_FACTORY_TEST
int door_ft_open(int layer);
int door_ft_close(int layer);
int door_ft_stop(int layer);

int door_ft_open_all(void);
int door_ft_close_all(void);
int door_ft_stop_all(void);
#endif /* CONFIG_APP_DOOR_FACTORY_TEST */

#ifdef CONFIG_APP_DOOR_DEBUG
void door_debug(void);
#endif /* CONFIG_APP_DOOR_FACTORY_TEST */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DOOR_H */

