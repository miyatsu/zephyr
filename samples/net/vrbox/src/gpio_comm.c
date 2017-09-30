#include <stdint.h>

#include "gpio_comm.h"

const char *gpio_group_dev_name_table[] =
{
	"GPIOA", "GPIOB", "GPIOC", "GPIOD", "GPIOE", "GPIOF", "GPIOG"
};

void gpio_comm_read(struct gpio_group_pin_t *gpio, uint32_t *value_addr)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table[gpio->gpio_group]);
	gpio_pin_configure(dev, gpio->gpio_pin, GPIO_DIR_IN | GPIO_PUD_PULL_UP);
	gpio_pin_read(dev, gpio->gpio_pin, value_addr);
}

void gpio_comm_write(struct gpio_group_pin_t *gpio, uint32_t value)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table[gpio->gpio_group]);
	gpio_pin_configure(dev, gpio->gpio_pin, GPIO_DIR_OUT | GPIO_PUD_PULL_UP);
	gpio_pin_write(dev, gpio->gpio_pin, value);
}

