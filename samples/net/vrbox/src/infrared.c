#include <stdint.h>
#include <stdbool.h>

#include <kernel.h>
#include <gpio.h>

#ifdef CONFIG_APP_INFRARED_DEBUG
#include <misc/printk.h>
#endif /* CONFIG_APP_INFRARED_DEBUG */

#include "gpio_comm.h"

/**
 * STM32F4_EXPLO On board IO map, P3:
 *
 * -----------------------
 * | PE1    1 |  2  PE0  |	Check
 * | PE3    3 |  4  PE2  |	Check
 * | PE5    5 |  6  PE4  |	Check
 * | PC13   7 |  8  PE6  |	Check
 * | PF1    9 | 10  PF0  |	Check
 * | PF3   11 | 12  PF2  |	Check
 * | PF5   13 | 14  PF4  |	Check
 * | PF7   15 | 16  PF6  |	PF7, Suggest Output Only.		PF6, Check.
 * | PF9   17 | 18  PF8  |	PF8, Not suggest use for GPIO.	PF9, Suggest Output Only.
 * | PC0   19 | 20  PF10 |	PC0, Can NOT use.				PF10, Suggest Output Only.
 * | PC2   21 | 22  PC1  |	PC2, Can NOT use.				PC1, ETH usage.
 * | PA1   23 | 24  PC3  |	PA1, ETH usage.					PC3, Check.
 * | PA4   25 | 26  PA0  |	Check
 * | PA6   27 | 28  PA5  |	Check
 * | PC4   29 | 30  PA7  |	PC4, ETH usage.					PA7, ETH usage.
 * | PB0   31 | 32  PC5  |	PC5, ETH usage.					PB0, Check.
 * | PB2   33 | 34  PB1  |	PB2, BOOT1.						PB1, Check(Touch INT)
 * | PF12  35 | 36  PF11 |	Check
 * | PF14  37 | 38  PF13 |	Check
 * | PG0   39 | 40  PF15 |	Check
 * | PB13  41 | 42  PG1  |	Check
 * | GND   43 | 44  PB12 |
 * -----------------------
 *
 * Note: We use two separated pin PF6(16) and PC3(24) for infrared detector input.
 *
 * */

static struct gpio_group_pin_t infrared_gpio_table[4 * 7] =
{
	{GPIO_GROUP_E,  1}, {GPIO_GROUP_E,  0}, {GPIO_GROUP_E,  3}, {GPIO_GROUP_E,  2},
	{GPIO_GROUP_E,  5}, {GPIO_GROUP_E,  4}, {GPIO_GROUP_C, 13}, {GPIO_GROUP_E,  6},
	{GPIO_GROUP_F,  1}, {GPIO_GROUP_F,  0}, {GPIO_GROUP_F,  3}, {GPIO_GROUP_F,  2},
	{GPIO_GROUP_F,  5}, {GPIO_GROUP_F,  4}, {GPIO_GROUP_F,  6}, {GPIO_GROUP_C,  3},
	/* The pin PF6 and PC3 are NOT nearby to each other */
	{GPIO_GROUP_A,  4}, {GPIO_GROUP_A,  0}, {GPIO_GROUP_A,  6}, {GPIO_GROUP_A,  5},
	{GPIO_GROUP_F, 12}, {GPIO_GROUP_F, 11}, {GPIO_GROUP_F, 14}, {GPIO_GROUP_F, 13},
	{GPIO_GROUP_G,  0}, {GPIO_GROUP_F, 15}, {GPIO_GROUP_B, 13}, {GPIO_GROUP_G,  1},
};

/**
 * @brief Read the value of infrared detector in the box
 *
 * @param index Infrared detector number
 *
 * @return true, if the box is empty
 *		   false, if the box is NOT empty
 * */
static bool infrared_is_box_empty_read_gpio(uint8_t index)
{
	/* Value read from infrared, 0 means box not empty, 1 means box empty */
	uint32_t is_box_empty;

	/**
	 * The infrared detector input pin level map:
	 *
	 * -----------------------------------
	 * | Box NOT Empty |   Box is Empty  |	Application perspective
	 * -----------------------------------
	 * |  VR Detected  | VR NOT Detected |	Driver perspective
	 * -----------------------------------
	 * |   Low Level   |    High Level   |	Hardware Perspective
	 * -----------------------------------
	 *
	 * Here we default think the box is empty once the infrared detector is not working.
	 *
	 * That is to say, we configure the gpio default GPIO_DIR_IN input mode and
	 * GPIO_PUD_PULL_UP have high level when no input at all.
	 * */
	gpio_comm_read(&infrared_gpio_table[index], &is_box_empty);

	return is_box_empty;
}

/**
 * @brief Wrapper function for infrared_is_box_empty_read_gpio(uint8_t)
 *
 * @param layer Number of door, [0-3] is expected. The top layer is 1
 * and the bottom is 4.
 *
 * @param axle_position Axle position, [0-6] is expected.
 *
 * @return true, if the box is empty
 *		   false, if the box is not empty
 * */
static bool infrared_is_box_empty(uint8_t layer, uint8_t axle_position)
{
	uint8_t infrared_detector_number = layer * 7 + axle_position;
	return infrared_is_box_empty_read_gpio(infrared_detector_number);
}

void infrared_box_update(bool *box)
{
	uint8_t i, j;
	for ( i = 0; i < 4; ++i )
	{
		for ( j = 0; j < 7; ++j )
		{
			box[i * 7 + j] = infrared_is_box_empty(i, j);
		}
	}
}

/**
 * @brief infrared initial function
 *
 * Some of the gpio need to drain its default input.
 * More detail please refer to the the comment within this function.
 * */
void infrared_init(void)
{
	/**
	 * We need an initial function to flush the original data in GPIO.
	 *
	 * Such as PC3 as we tested, the first value read from it after
	 * power on is not the initial value that we have set.
	 *
	 * To prevent read wrong value from any other GPIOs, this initial
	 * must be called before use any API we provide.
	 * */
	for ( uint8_t i = 0; i < 4 * 7; ++i )
	{
		infrared_is_box_empty_read_gpio(i);
	}
}

#ifdef CONFIG_APP_INFRARED_DEBUG

void infrared_debug(void)
{
	/*
	uint32_t g1, g2, g3, g4;
	struct device *dev0 = device_get_binding("GPIOE");

	if ( 0 != gpio_pin_configure(dev0, 0, GPIO_DIR_IN | GPIO_PUD_PULL_UP) )
	{
		printk("gpio_pin_configure error!");
		return ;
	}
	struct device *dev1 = device_get_binding("GPIOF");
	struct device *dev2 = device_get_binding("GPIOB");
	while (1)
	{
		gpio_pin_read(dev0, 7, &g1);
		gpio_pin_read(dev0, 8, &g2);
		gpio_pin_read(dev0, 9, &g3);
		gpio_pin_read(dev0, 10, &g4);
		printk("%d, %d, %d, %d\n", g1, g2, g3, g4);
		k_sleep(200);
	}
	*/
	for ( uint8_t i = 1; i <= 4; ++i )
	{
		for ( uint8_t j = 1; j <= 7; ++j )
		{
			if ( infrared_is_box_empty(i, j) )
			{
				printk("Empty\n");
			}
			else
			{
				printk("Not Empty\n");
			}
			k_sleep(100);
		}
	}
	return ;
}

#endif /*CONFIG_APP_INFRARED_DEBUG */

