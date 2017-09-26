/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All right reserved.
 *
 * @file axle.c
 *
 * @brief Axle driver for sharing-vr-box project
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 16:09:05 September 22, 2017 GTM+8
 *
 * */
#include <stdint.h>
#include <stdbool.h>

#include <kernel.h>
#include <gpio.h>
#include <pwm.h>

#ifdef CONFIG_APP_AXLE_DEBUG
#include <misc/printk.h>
#endif /* CONFIG_APP_AXLE_DEBUG */

#include "axle.h"

static uint8_t axle_position;

/**
 * @brief Read position index's level.
 *
 * High level means current axle is at @param index position
 * Low level means current axle is not at @param index position
 *
 * @param index Position that the gpio reside, value of [1-7]
 *
 * @return	0	low level (Axle current not at @index position)
 *			1	hight level (Axle current at @index position)
 * */
bool axle_position_read_gpio(uint8_t index)
{
	/**
	 *
	 * The GPIO pin number of axle position detector, we designed as follow:
	 *
	 * ----------------------------------
	 * | axle position | GPIO pin number|
	 * ----------------------------------
	 * |	A1				PB.15		|
	 * |	A2				PD.11		|
	 * |	A3				PD.12		|
	 * |	A4				PD.13		|
	 * |	A5				PG.2		|
	 * |	A6				PG.3		|
	 * |	A7				PG.4		|
	 * ----------------------------------
	 *
	 * All those GPIO pins are at the top of the left pin row.
	 *
	 * */

	/* Note: the first item of this table is reserved, DO NOT use it. */
	static const uint8_t axle_gpio_map_table[] =
	{
		0, 15, 11, 12, 13, 2, 3, 4
	};
	struct device *dev = NULL;
	uint32_t value_read_from_gpio = 0;

	/* Using extended GNU C sytax */
	switch ( index )
	{
		case 1:
			dev = device_get_binding("GPIOB");
			break;
		case 2 ... 4:
			dev = device_get_binding("GPIOD");
			break;
		case 5 ... 7:
			dev = device_get_binding("GPIOG");
			break;
		default:
			return -1;
	}

	gpio_pin_read(dev, axle_gpio_map_table[index], &value_read_from_gpio);

	return value_read_from_gpio;
}

/**
 * @brief Read the axle current position.
 *
 * @return	The axle current position, 1-7 is expected.
 *			return 0 if can not read the axle position,
 *			or some error happened during read the position,
 *			or axle are rotating.
 *
 * */
uint8_t axle_position_read(void)
{
	/* Axle position can ONLY at one place, possible value are placed as follow */
	const static uint8_t axle_position_table[] =
	{
		0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40
	};

	uint8_t i;
	uint8_t axle_position_bit_map = 0;
	uint8_t axle_position_bit;

	for ( i = 0; i < 7; ++i )
	{
		/* Read all status of axle position, in case of two position is valid. */
		axle_position_bit = axle_position_read_gpio(i) ? 1 : 0;

		axle_position_bit_map |= ( axle_position_bit << i );
	}

	/* Lookup all possibile value table */
	for ( i = 0; i < 7; ++i )
	{
		if ( axle_position_bit_map == axle_position_table[i] )
		{
			return i + 1;
		}
	}
	return 0;
}


/**
 *
 * Axle rotate pin number, we designed as follow:
 *
 * ------------------------------------
 * | axle_function  | GPIO pin number |
 * ------------------------------------
 * | axle_lock				PG.6	  |
 * | axle_direction			PG.7	  |
 * | axle_enable			PG.8	  |
 * ------------------------------------
 *
 * */

/**
 * @brief Set axle break, to control axle free rotate or lock it
 *
 * @param (AXLE_ROTATE_LOCK = 0)/(AXLE_ROTATE_UNLOCK = 1)
 * */
void axle_set_rotate_lock_unlock(enum axle_rotate_lock_unlock lock_unlock)
{
	struct device *dev = device_get_binding("GPIOG");
	gpio_pin_write(dev, 6, lock_unlock);
}

/**
 * @brief Set axle rotate direction
 *
 * @param (AXLE_ROTATE_LEFT)/(AXLE_ROTATE_RIGHT)
 * */
void axle_set_rotate_direction(enum axle_rotate_direction direction)
{
	struct device *dev = device_get_binding("GPIOG");
	gpio_pin_write(dev, 7, direction);
}

/**
 * @brief Enable or Disable axle rotate
 *
 * @param (AXLE_ROTATE_DISABLE = 0)/(AXLE_ROTATE_ENABLE = 1)
 * */
void axle_set_rotate_on_off(enum axle_rotate_on_off on_off)
{
	struct device *dev = device_get_binding("GPIOG");
	gpio_pin_write(dev, 8, on_off);
}


/**
 * --------------------
 * |       Center     |
 * |    (A7)   (A1)   |
 * |  (A6)       (A2) |
 * |    (A5)   (A3)   |
 * |		(A4)      |
 * --------------------
 *  Figure: Axle
 *  Hardware Design
 *
 * Note:
 * (0).Due to restricted hardware design, the axle can ONLY rotate either clockwise
 *     or anticlockwise for 195 degrees.
 * (1).One circle has 7 boxes, each box at a distance of 360/7 degrees.
 * (2).We assume A4 is at the start position, and make the axle can either rotate
 *     clockwise to A1 or rotate anticlockwise to A7 without reach the limitation
 *     of rotate angles.
 *
 *
 * Note: A1 is the start position of the axle.
 * (a). The max angle rotate to right is A1 to A4.
 * (b). The max angle rotate to left is A1 to A8.
 *
 * For example: (1). If we at A4 and want goto A5, MUST rotate left to A5
 *					all the way pass though A3, A2, A1.
 *				(2). If we at A4 and want goto A8, MUST rotate left to A8
 *					all the way pass though A3, A2, A1, A5, A6, A7.
 *
 * Due to this special hardware design, In order to simplify the software design,
 * we make a spwcial table for rotate operation.
 *
 * Row is the current axle position, and the column is the destination axle position.
 *
 * Positive means axle should rotate right N times.
 * Negetive means axle should rotate left N times.
 *
 * -----------------------------------------------
 * |   |  1  |  2  |  3  |  4  |  5  |  6  |  7  |
 * -----------------------------------------------
 * | 1 |  0  | -1  | -2  | -3  | -4  | -5  | -6  |
 * | 2 |  1  |  0  | -1  | -2  | -3  | -4  | -5  |
 * | 3 |  2  |  1  |  0  | -1  | -2  | -3  | -4  |
 * | 4 |  3  |  2  |  1  |  0  | -1  | -2  | -3  |
 * | 5 |  4  |  3  |  2  |  1  |  0  | -1  | -2  |
 * | 6 |  5  |  4  |  3  |  2  |  1  |  0  | -1  |
 * | 7 |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * -----------------------------------------------
 *
 * */

/**
 * @brief Rotate axle 360/7 degree with direction of @direction
 *
 * @param direction The direction the the axle should rotate
 *
 * @return 0, rotate success
 *		   -1, rotate error
 * */
int8_t axle_rotate(enum axle_rotate_direction direction)
{
	uint8_t wait_time;

	/* Set direction signal */
	axle_set_rotate_direction(direction);

	/* Unlock axle */
	axle_set_rotate_lock_unlock(AXLE_ROTATE_UNLOCK);

	/* Enable axle */
	axle_set_rotate_on_off(AXLE_ROTATE_ENABLE);

	/**
	 * TODO XXX FIXME
	 * 
	 * Create new thread to check position ?
	 * Using k_queue ?
	 * Pending to design...
	 *
	 * */
	/* Start to rotate, 45 degrees we will stop the axle */
	/* We need a little bit time to wait axle in position */
	k_sleep(1000);		/* WARNING: This period of time depending on PWM frequency */

	for ( wait_time = 0; wait_time < 100; wait_time += 10 )
	{
		/* Axle in position */
		if ( 0 != axle_position_read() )
		{
			break;
		}

		/* Axle is not in position, wait for more 10ms */
		k_sleep(10);
	}

	/* Disable axle */
	axle_set_rotate_on_off(AXLE_ROTATE_DISABLE);

	/* Lock axle in case of free rotate */
	axle_set_rotate_lock_unlock(AXLE_ROTATE_LOCK);

	/* Update axle position */
	axle_position = axle_position_read();

	/* Wait axle rotate in position timeout */
	if ( wait_time >= 100 )
	{
		return -1;
	}

	return 0;
}

/**
 * @brief Rotate the axle to particular position
 *
 * @param destination_position The real position want to get
 *
 * @return 0 success, < 0 error
 *
 * */
int8_t axle_rotate_to(uint8_t destination_position)
{
	/* Update current axle position */
	axle_position = axle_position_read();

	/* Boundary check */
	if ( !( axle_position >=1 && axle_position <= 7 ) ||
		 !( destination_position >= 1 && destination_position <= 7 ) )
	{
		return -1;
	}

	int8_t rotate_times = (int8_t)axle_position - (int8_t)destination_position;

	/* Already at requested position */
	if ( 0 == rotate_times )
	{
		return 0;
	}

	if ( rotate_times > 0 )
	{
		for ( uint8_t times = 0; times < rotate_times; ++times )
		{
			if ( 0 != axle_rotate(AXLE_ROTATE_CLOCKWISE) )
			{
				return -1;
			}
		}
	}
	else
	{
		for ( uint8_t times = 0; times < -rotate_times; ++times )
		{
			if (0 != axle_rotate(AXLE_ROTATE_ANTICLOCKWISE) )
			{
				return -1;
			}
		}
	}

	return 0;
}

int8_t axle_self_check(void)
{
	axle_position = axle_position_read();
	if ( 0 == axle_position )
	{
		/* Axle position sensor error */
		return -1;
	}

	/* Rotate to most left position */
	for ( uint8_t position = 8; position >= 1; --position )
	{
		if ( 0 != axle_rotate_to(position) )
		{
			return -1;
		}
	}

	/* Rotate to most right position, and leave it for door check */
	for ( uint8_t position = 1; position <= 8; ++position )
	{
		if ( 0 != axle_rotate_to(position) )
		{
			return -1;
		}
	}

	return 0;
}


int axle_init(void)
{
	/* TODO: Wait for other code done, PWM output is needed. */
	return 0;
}

#ifdef CONFIG_APP_AXLE_DEBUG
/**
 * PA2 TIM2_CH3
 * */
void axle_debug(void)
{
	/**
	 * WARNING: The Enable/Disable Input on stepper motor driver is NOT working!!!
	 *
	 * The stepper motor driver default enable without hight level input to it.
	 *
	 * So, to enable and disable the rotate of axle, we need use PWM generator
	 * to control it.
	 * */
	printk("Endtering %s() ...\n", __func__);
	struct device *dev = device_get_binding(CONFIG_PWM_STM32_2_DEV_NAME);
	if ( pwm_pin_set_usec(dev, 2, 5000, 2500) )
	{
		printk("pwm pin set failes!\n");
	}
	while (1)
	{
		k_sleep(1000);
	}
	return ;
}
#endif /* CONFIG_APP_AXLE_DEBUG */

