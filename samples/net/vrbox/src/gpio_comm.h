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

void gpio_comm_read(struct gpio_group_pin_t *gpio, uint32_t *value_addr);
void gpio_comm_write(struct gpio_group_pin_t *gpio, uint32_t value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GPIO_COMM_H */
