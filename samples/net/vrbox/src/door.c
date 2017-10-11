/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All right reserved.
 *
 * @file door.c
 *
 * @brief Door driver for sharing-vr-box project
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 11:32:38 October 10, 2017 GTM+8
 *
 * */
#include <stdint.h>
#include <stdbool.h>

#include <kernel.h>
#include <gpio.h>

#ifdef CONFIG_APP_DOOR_DEBUG
#include <misc/printk.h>
#endif /* CONFIG_APP_DOOR_DEBUG */

#include "gpio_comm.h"

#include "config.h"

/**
 * P5:
 * ------------
 * | PD14   1 |
 * | PD15   2 |
 * | PD0    3 |
 * | PD1    4 |
 * | PE7    5 |
 * | PE8    6 |
 * | PE9    7 |
 * | PE10   8 |
 * | PE11   9 |
 * | PE12  10 |
 * | PE13  11 |
 * | PE14  12 |
 * | PE15  13 |
 * | PD8   14 |
 * | PD9   15 |
 * | PD10  16 |
 * ------------
 * */

static struct gpio_group_pin_t door_gpio_table[4][5] =
{
	/* motor open, morotr close, opened detector, closed detector, infrared detector */
	{
		{GPIO_GROUP_D, 14}, {GPIO_GROUP_D, 15}, {GPIO_GROUP_D,  0}, {GPIO_GROUP_D,  1}, {GPIO_GROUP_E,  7}
	},
	{
		{GPIO_GROUP_E,  8}, {GPIO_GROUP_E,  9}, {GPIO_GROUP_E,  9}, {GPIO_GROUP_E, 10}, {GPIO_GROUP_E, 12}
	},
	{
		{GPIO_GROUP_E, 13}, {GPIO_GROUP_E, 14}, {GPIO_GROUP_E, 11}, {GPIO_GROUP_E, 12}, {GPIO_GROUP_D,  9}
	},
	{
		{GPIO_GROUP_D, 10}, {GPIO_GROUP_D, 15}, {GPIO_GROUP_E, 13}, {GPIO_GROUP_E, 14}, {GPIO_GROUP_E,  7}
	}
};

/**
 * @brief Set door motor to open
 *
 * @param layer Door number, [1-4] is expected
 * */
static void door_open_write_gpio(uint8_t layer)
{
	/* Set high level output on open motor */
	gpio_comm_write(&door_gpio_table[layer-1][0], 1);

	/* Set low level output on close motor */
	gpio_comm_write(&door_gpio_table[layer-1][1], 0);
}

/**
 * @brief Set door motor to close
 *
 * @param layer Door number, [1-4] is expected
 * */
static void door_close_write_gpio(uint8_t layer)
{
	/* Set low level output on open motor */
	gpio_comm_write(&door_gpio_table[layer-1][0], 0);

	/* Set high level output on close motor */
	gpio_comm_write(&door_gpio_table[layer-1][1], 1);
}

/**
 * @brief Set door motor stop
 *
 * @param layer Door number, [1-4] is expected
 * */
static void door_stop_write_gpio(uint8_t layer)
{
	/* Set low level output on both open and close motor */
	gpio_comm_write(&door_gpio_table[layer-1][0], 0);
	gpio_comm_write(&door_gpio_table[layer-1][1], 0);
}

/**
 * @brief This function is to check the door open state, and stop opening
 *		  operation.
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return -1 Door has never open
 *			0 Door has fully opened
 *			1 Door open timeout, and leave it half open
 * */
static int8_t door_wait_open(uint8_t layer)
{
	uint8_t i, wait_time_in_sec		= CONFIG_APP_DOOR_OPEN_TIMEOUT_IN_SEC;
	uint32_t door_not_in_position	= 1;	/* Default not in position */
	for ( i = 0; i < wait_time_in_sec * 10; ++i )
	{
		/**
		 * Pooling the open indicator state, and break the loop
		 * when the door is fully opened
		 * */
		gpio_comm_read(&door_gpio_table[layer-1][2], &door_not_in_position);
		if ( !door_not_in_position )
		{
			break;
		}
		k_sleep(100);
	}

	/* Stop door's motor after fully opened or timeout */
	door_stop_write_gpio(layer);

	if ( i >= wait_time_in_sec * 10 )
	{
		/* Wait door fully opened timeout */

		/**
		 * Read the close indicator state, return -1 if door is still close,
		 * return 1 if door is half opened.
		 * */
		gpio_comm_read(&door_gpio_table[layer-1][3], &door_not_in_position);
		if ( door_not_in_position )
		{
			/* Half open */
			return 1;
		}
		/* Never opened */
		return -1;
	}

	/* Fully opened */
	return 0;
}

/**
 * @brief This function is to check door close state, and stop close
 *		  operation
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return -3 Door has never close after timeout
 *		   -2 Door has been blocked by something which detected by on
 *			  door infared detector
 *		   -1 Door close timeout, and leave it half close
 *		    0 Door has fully closed
 * */
static int8_t door_wait_close(uint8_t layer)
{
	/* Times of try to close the door */
	uint8_t retry_times = 0;

	/* Time resolution when close the door */
	uint8_t i, wait_time_in_sec = CONFIG_APP_DOOR_CLOSE_TIMEOUT_IN_SEC;

	/**
	 * 1 means the door is not at close state
	 * 0 means the door is at close state
	 * */
	uint32_t door_not_in_position;

	/**
	 * 1 means the door is not blocking by foreign maters
	 * 0 means the door is blocking by foreign maters
	 * */
	uint32_t door_not_blocking;

retry:
	if ( 3 <= retry_times )
	{
		/* No more retry after trying three tiems */
		return -2;
	}
	else if ( 0 != retry_times )
	{
		/* Door infrared detected, retry entered */

		/* Open the door, no mater if it is fully opened */
		int8_t door_open(uint8_t);
		door_open(layer);

		/**
		 * Wait for three seconds, we assume the foreign mater
		 * that blocking the door will no longer exists
		 * */
		k_sleep(3000);

		/* Retry to close the door */
		door_close_write_gpio(layer);
	}

	door_not_in_position	= 1;		/* Default not in position */
	door_not_blocking		= 1;		/* Default not blocking */
	for ( i = 0; i < wait_time_in_sec * 10; ++i )
	{
		gpio_comm_read(&door_gpio_table[layer-1][3], &door_not_in_position);
		if ( !door_not_in_position )
		{
			break;
		}

		/**
		 * Read value from on door infrared detector, if there are foreign
		 * maters blocking the door, retry to close the door
		 * */
		gpio_comm_read(&door_gpio_table[layer-1][4], &door_not_blocking);
		if ( !door_not_blocking )
		{
			++retry_times;
			goto retry;
		}

		k_sleep(100);
	}

	/* Stop door's motor after fully closed or timeout */
	door_stop_write_gpio(layer);

	if ( i >= wait_time_in_sec * 10 )
	{
		/* Wait door close timeout */
		gpio_comm_read(&door_gpio_table[layer-1][2], &door_not_in_position);
		if ( door_not_in_position )
		{
			/* Half close */
			return -1;
		}
		/* Never closed */
		return -3;
	}

	/* Fully closed */
	return 0;
}

/**
 * @brief Public wrapper function of door open
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return -1 Door has never opened
 *			0 Door has fully opened
 *			1 Door open timeout, and leave it half open
 * */
int8_t door_open(uint8_t layer)
{
	door_open_write_gpio(layer);
	return door_wait_open(layer);
}

/**
 * @brief Public wrapper function of door close
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return -3 Door has never close after timeout
 *		   -2 Door has been blocked by something which detected by on
 *			  door infrared detector
 *		   -1 Door close timeout, and leave it half close
 *			0 Door has fully closed
 * */
int8_t door_close(uint8_t layer)
{
	door_close_write_gpio(layer);
	return door_wait_close(layer);
}

/**
 * @brief Door hardware initial function
 *
 * @return  0 Initial OK
 *		   <0 Initial Failed
 * */
int8_t door_init(void)
{
	uint8_t i;
	int8_t rc;
	for ( i = 1; i <= 4; ++i )
	{
		/* Close all four doors */
		rc = door_close(i);
		if ( 0 != rc )
		{
			return rc;
		}
	}
	return 0;
}

#ifdef CONFIG_APP_DOOR_FACTORY_TEST

int8_t door_factory_test(void)
{
	return 0;
}

#endif /* CONFIG_APP_DOOR_FACTORY_TEST */

#ifdef CONFIG_APP_DOOR_DEBUG

void door_debug(void)
{
	while (1)
	{
		k_sleep(3000);
		if( 0 == door_open(1) )	/* Open first layer's door */
		{
			printk("Open OK\n");
		}
		else
		{
			printk("Open Failed\n");
			continue;
		}

		k_sleep(3000);

		if ( 0 == door_close(1) )
		{
			printk("Close OK\n");
		}
		else
		{
			printk("Close Failed\n");
			door_stop_write_gpio(1);
			continue;
		}
		k_sleep(3000);
	}
	return ;
}

#endif /* CONFIG_APP_DOOR_DEBUG */

