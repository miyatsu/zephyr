/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file gpio_comm.c
 *
 * @brief GPIO operation wrapper functions
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 17:43:07 October 20, 2017 GTM+8
 *
 * */
#include <stdint.h>

#include "gpio_comm.h"

/* GPIO name table */
const char *gpio_group_dev_name_table[] =
{
	"GPIOA", "GPIOB", "GPIOC", "GPIOD", "GPIOE", "GPIOF", "GPIOG"
};

/**
 * @brief Wrapper function of gpio_pin_read using struct gpio_group_pin_t
 *
 * @param gpio Pin define, type of struct gpio_group_pin_t
 *
 * @param value_addr Read out value will save to this address
 * */
void gpio_comm_read(struct gpio_group_pin_t *gpio, uint32_t *value_addr)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table[gpio->gpio_group]);
	gpio_pin_read(dev, gpio->gpio_pin, value_addr);
}

/**
 * @brief Wrapper function of gpio_pin_write using struct gpio_group_pin_t
 *
 * @param gpio Pin define, type of struct gpio_group_pin_t
 *
 * @param value Write out value
 * */
void gpio_comm_write(struct gpio_group_pin_t *gpio, uint32_t value)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table[gpio->gpio_group]);
	gpio_pin_write(dev, gpio->gpio_pin, value);
}

/**
 * @brief Wrapper function of gpio_pin_configure using struct gpio_group_pin_t
 *
 * @param gpio Pin define, type of struct gpio_group_pin_t
 *
 * @param flag GPIO config flag, defined in file gpio.h
 * */
void gpio_comm_conf(struct gpio_group_pin_t *gpio, uint32_t flag)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table[gpio->gpio_group]);
	gpio_pin_configure(dev, gpio->gpio_pin, flag);
}

