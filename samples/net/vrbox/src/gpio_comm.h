/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file GPIO wrapper function API
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 17:45:05 October 20, 2017 GTM+8
 *
 * */
#ifndef GPIO_COMM_H
#define GPIO_COMM_H

#include <gpio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum gpio_group_e
{
	GPIO_GROUP_A = 0,
	GPIO_GROUP_B,
	GPIO_GROUP_C,
	GPIO_GROUP_D,
	GPIO_GROUP_E,
	GPIO_GROUP_F,
	GPIO_GROUP_G
};

struct gpio_group_pin_t
{
	enum gpio_group_e gpio_group;
	uint8_t gpio_pin;
};

extern const char *gpio_group_dev_name_table[];

/**
 * @brief Wrapper function of gpio_pin_read using struct gpio_group_pin_t
 *
 * @param gpio Pin define, type of struct gpio_group_pin_t
 *
 * @param value_addr Read out value will save to this address
 * */
void gpio_comm_read(struct gpio_group_pin_t *gpio, uint32_t *value_addr);

/**
 * @brief Wrapper function of gpio_pin_write using struct gpio_group_pin_t
 *
 * @param gpio Pin define, type of struct gpio_group_pin_t
 *
 * @param value Write out value
 * */
void gpio_comm_write(struct gpio_group_pin_t *gpio, uint32_t value);

/**
 * @brief Wrapper function of gpio_pin_configure using struct gpio_group_pin_t
 *
 * @param gpio Pin define, type of struct gpio_group_pin_t
 *
 * @param flag GPIO config flag, defined in file gpio.h
 * */
void gpio_comm_conf(struct gpio_group_pin_t *gpio, uint32_t flag);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GPIO_COMM_H */

