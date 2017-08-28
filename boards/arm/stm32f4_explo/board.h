/*
 * Copyright (c) 2017 Ding Tao <miyatsu@qq.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __INC_BOARD_H
#define __INC_BOARD_H

#include <soc.h>

/* USER push button */
#define USER_PB_GPIO_PORT	"GPIOA"
#define USER_PB_GPIO_PIN	0

/* LED0 red LED */
#define LED0_GPIO_PORT	"GPIOF"
#define LED0_GPIO_PIN	9

/* LED1 yellow LED */
#define LED1_GPIO_PORT	"GPIOF"
#define LED1_GPIO_PIN	10

/* Create aliases to make the basic samples work */
#define SW0_GPIO_NAME	USER_PB_GPIO_PORT
#define SW0_GPIO_PIN	USER_PB_GPIO_PIN

#endif /* __INC_BOARD_H */
