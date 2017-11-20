/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
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
#include <misc/util.h>
#include <pwm.h>

#ifdef CONFIG_APP_AXLE_DEBUG
#include <misc/printk.h>
#endif /* CONFIG_APP_AXLE_DEBUG */

#include "config.h"
#include "gpio_comm.h"

#define SYS_LOG_DOMAIN	"axle"
#ifdef CONFIG_APP_AXLE_DEBUG
	#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_DBG
#else
	#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_ERR
#endif /* CONFIG_APP_AXLE_DEBUG */
#include <logging/sys_log.h>

/**
 * P4:
 * -----------------------
 * | PB9    1 |  2  PB8	 |	Not suggest using as GPIO
 * | PB7    3 |  4  PB6	 |	Check			Used by headset
 * | PB5    5 |  6  PB4  |	Check			Used by headset
 * | PB3    7 |  8  PG15 |	PB3 PWM_2_2		Used by headset (PG15 Only)
 * | PG14   9 | 10  PG13 |	ETH
 * | PG12  11 | 12  PG11 |	PG11 ETH
 * | PG10  13 | 14  PG9  |	Not suggest using as GPIO
 * | PD7   15 | 16  PD6  |					Used by door
 * | PD5   17 | 18  PD4  |					Used by door
 * | PD3   19 | 20  PD2  |	PD3 ETH
 * | PC12  21 | 22  PC11 |
 * | PC10  23 | 24  PA15 |
 * | PA14  25 | 26  PA13 |
 * | PA8   27 | 28  PC9  |
 * | PC8   29 | 30  PC7  |
 * | PC6   31 | 32  PG8  |
 * | PG7   33 | 34  PG6  |
 * | PG5   35 | 36  PG4  |
 * | PG3   37 | 38  PG2  |
 * | PD13  39 | 40  PD12 |
 * | PD11  41 | 42  PB15 |
 * | PB14  43 | 44  GND  |
 * -----------------------
 *
 * */

static struct gpio_group_pin_t axle_gpio_table[] =
{
	{GPIO_GROUP_G,  2},		/* Axle position start*/
	{GPIO_GROUP_G,  3},
	{GPIO_GROUP_G,  4},
	{GPIO_GROUP_G,  5},
	{GPIO_GROUP_G,  6},
	{GPIO_GROUP_G,  7},
	{GPIO_GROUP_G,  8},		/* Axle position end */

	{GPIO_GROUP_B,  3},		/* PWM output */
	{GPIO_GROUP_D,  2},		/* Axle rotate direction */
	{GPIO_GROUP_G, 12}		/* Stepper motor break */
};

/* Axle functionality status, true means work fine, false means axle is broken */
static bool axle_status = false;

/**
 * @brief Get axle functionality status
 *
 * @return true, axle fine
 *		   false, axle is broken
 * */
bool axle_get_status(void)
{
	return axle_status;
}

/**
 * @brief Read position index's level.
 *
 * High level means current axle is at @param index position
 * Low level means current axle is not at @param index position
 *
 * @param index Position that the gpio reside, value of [0-6]
 *
 * @return	0	low level (Axle current not at @index position)
 *			1	hight level (Axle current at @index position)
 * */
static bool axle_position_read_gpio(uint8_t index)
{
	uint32_t val = 0;

	gpio_comm_read(&axle_gpio_table[index], &val);

	return val;
}

/**
 * @brief Read the axle current position.
 *
 * @return	The axle current position, 1-7 is expected.
 *			return 0 if can not read the axle position,
 *			or some error happened during read the position,
 *			or axle are rotating when request the current
 *			axle position.
 * */
static uint8_t axle_position_read(void)
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
		axle_position_bit = axle_position_read_gpio(i) ? 0 : 1;

		axle_position_bit_map |= ( axle_position_bit << i );
	}

	/* Lookup all possibile value table */
	for ( i = 0; i < 7; ++i )
	{
		/* Position match the possibile position */
		if ( axle_position_bit_map == axle_position_table[i] )
		{
			return i + 1;
		}
	}
	return 0;
}

/**
 * @brief Set axle break, to control axle free rotate or lock it
 *
 * @param unlock 1 unlock
 *			   0 lock
 * */
static void axle_set_rotate_lock_unlock(uint8_t unlock)
{
	gpio_comm_write(&axle_gpio_table[9], !unlock);
}

/**
 * @brief Set axle rotate direction
 *
 * @param direction 1 clockwise
 *					0 anticlockwise
 * */
static void axle_set_rotate_direction(uint8_t direction)
{
	gpio_comm_write(&axle_gpio_table[8], direction);
}

/**
 * @brief Enable or Disable axle rotate
 *
 * @param enable 1 enable
 *				 0 disable
 * @return true Set Enable/Disable success
 * 		   false Otherwise
 * */
static bool axle_set_rotate_enable_disable(uint8_t enable)
{
	struct device *pwm_dev = device_get_binding(CONFIG_APP_AXLE_PWM_DEV_NAME);

	int8_t   rc;
	uint8_t  channel = CONFIG_APP_AXLE_PWM_OUTPUT_CHANNEL;
	uint32_t period = CONFIG_APP_AXLE_PWM_PERIOD;
	uint32_t pulse_width = period/2;

	if ( enable )
	{
		/* Start to rotate */
		rc = pwm_pin_set_usec(pwm_dev, channel, period, pulse_width);
	}
	else
	{
		/* Stop rotate */
		rc = pwm_pin_set_usec(pwm_dev, channel, period, 0);
	}
	return !rc;
}


/**
 * --------------------
 * |       Center     |
 * |    (A1)   (A7)   |
 * |  (A2)       (A6) |
 * |    (A3)   (A5)   |
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
 *     clockwise to A7 or rotate anticlockwise to A1 without reach the limitation
 *     of rotate angles.
 *
 *
 * Note: A4 is the start position of the axle.
 * (a). The max angle rotate clockwise is A4 to A7.
 * (b). The max angle rotate anticlockwise is A4 to A1.
 *
 * For example: (1). If we at A3 and want goto A6, MUST rotate clockwise to A6
 *					all the way pass though A4, A5.
 *				(2). If we at A7 and want goto A2, MUST rotate anticlockwise to
 *					A2 all the way pass though A6, A5, A4, A3.
 *
 * Due to this special hardware design, in order to simplify the software design,
 * we make a spwcial table for rotate operation.
 *
 * Row is the current axle position, and the column is the destination axle position.
 *
 * Positive means axle should rotate clockwise N times.
 * Negetive means axle should rotate anticlockwise N times.
 *
 * ------------------------------------------------
 * |   ||  1  |  2  |  3  |  4  |  5  |  6  |  7  |
 * ------------------------------------------------
 * | 1 ||  0  | -1  | -2  | -3  | -4  | -5  | -6  |
 * | 2 ||  1  |  0  | -1  | -2  | -3  | -4  | -5  |
 * | 3 ||  2  |  1  |  0  | -1  | -2  | -3  | -4  |
 * | 4 ||  3  |  2  |  1  |  0  | -1  | -2  | -3  |
 * | 5 ||  4  |  3  |  2  |  1  |  0  | -1  | -2  |
 * | 6 ||  5  |  4  |  3  |  2  |  1  |  0  | -1  |
 * | 7 ||  6  |  5  |  4  |  3  |  2  |  1  |  0  |
 * ------------------------------------------------
 *
 * */

/**
 * Due to asynchronous execution on main thread and axle in position
 * irq, we use a wemaphore to synchronous two thread in order to
 * execute next instruction in main thrad.
 * */
static struct k_sem axle_in_position_sem;

/**
 * @brief Callback function when axle rotate to certain position
 *
 * @param dev Gpio device pointer
 *
 * @param gpio_cb Gpio callback structure
 *
 * @param pins Mask of pins that trigger the callback
 * */
static void axle_in_position_irq_cb(struct device *dev,
									struct gpio_callback *gpio_cb,
									uint32_t pins)
{
	/**
	 * ONLY the axle rotate is needed, that this function can be called.
	 *
	 * (a). Assume we at N position and we want goto N position, the gpio
	 *		interrupt will not be enabled.
	 *		Thus this functino will not be called.
	 *
	 * (b). Assume we at N position and we want goto M position, the gpio
	 *		interrupt will ONLY enabled at M position's pin.
	 *		Thus onece this functino has been called, that means the axle
	 *		is already at M position. We don't have read the gpio status
	 *		to determine if the interrupt is rotate in or out (rising edge
	 *		or falling edge).
	 *
	 * According to anaiysis above, onece this function has been called,
	 * we only have to disable the rotate and disable the gpio interrupt.
	 * */

	printk("%s called\n", __func__);
	uint32_t pin = 0;

	/* Check if it really at certain position */
	if ( 0 == axle_position_read() )
	{
		printk("This irq says that current axle not in position\n");
		return ;
	}

	/* Parse pin mask to pin number */
	while ( 1 != pins )
	{
		pins >>= 1;
		pin++;
	}
	printk("position == %d\n", pin - 1);

	/* Position reached */

	/* Disable gpio interrupt */
	gpio_pin_disable_callback(dev, pin);

	/* Stop rotate */
	axle_set_rotate_enable_disable(0);

	/* Lock the break */
	axle_set_rotate_lock_unlock(0);

	/* Sync main handle thread and irq thread */
	k_sem_give(&axle_in_position_sem);
}

/**
 * @brief Enable position's gpio interrupt
 *
 * @position Position number of detector, [1-7] is expected
 * */
static void axle_in_position_irq_enable(uint8_t position)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table	\
			[axle_gpio_table[position - 1].gpio_group]);
	gpio_pin_enable_callback(dev, axle_gpio_table[position - 1].gpio_pin);
}

/**
 * @brief Disable position's gpio interrupt
 *
 * @position Position number of detector, [1-7] is expected
 * */
static void axle_in_position_irq_disable(uint8_t position)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table	\
			[axle_gpio_table[position - 1].gpio_group]);
	gpio_pin_disable_callback(dev, axle_gpio_table[position - 1].gpio_pin);
}

/**
 * @brief Initial axle position gpio interrupt
 *
 * This function will be called by axle_init()
 * */
static void axle_in_position_irq_init(void)
{
	uint8_t		i, j;
	uint32_t	pin_mask;
	bool		gpio_initialized_table[7] =
	{
		false, false, false, false, false, false, false
	};
	struct		device *dev = NULL;

	static struct	gpio_callback gpio_cb[7];
	static uint8_t	gpio_cb_number = 0;

	/**
	 * The "struct gpio_callback" MUST NOT alloc in heap memory, so we declare it
	 * as static and it will be link into .bss section.
	 *
	 * The "struct gpio_callback" can ONLY be initial ONCE by gpio_callback_init().
	 *
	 * If we use one loop to add all pins into one gpio_callback, we MUST assume
	 * all seven pins are the same group.
	 *
	 * For extendibility consideration, we use nested loop to find out which pins
	 * are the same group, and initial it with one particular "struct gpio_callback".
	 * */

	/* Initial all seven pins */
	for ( i = 0, gpio_cb_number = 0; i < 7; ++i )
	{
		if ( gpio_initialized_table[i] )
		{
			/* Already initialized, skip to next pin */
			continue;
		}

		dev = device_get_binding(gpio_group_dev_name_table	\
				[axle_gpio_table[i].gpio_group]);
		pin_mask = 0;

		/* Get the same group pins into pin_mask */
		for ( j = i; j < 7; ++j )
		{
			/**
			 * The index j's gpio name is the same as current dev,
			 * initial them at onece.
			 * */
			if ( axle_gpio_table[i].gpio_group == axle_gpio_table[j].gpio_group )
			{
				/* Configure current gpio as intrrupt input */
				gpio_comm_conf(&axle_gpio_table[j],
						GPIO_DIR_IN | GPIO_INT | GPIO_INT_DEBOUNCE |	\
						GPIO_PUD_PULL_UP | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW);

				/* Add new pin number into pin_mask */
				pin_mask |= BIT(axle_gpio_table[j].gpio_pin);

				/* Skip current pin on initialization */
				gpio_initialized_table[j] = true;
			}
		}

		/* Initial callback */
		gpio_init_callback(&gpio_cb[gpio_cb_number], axle_in_position_irq_cb, pin_mask);

		/* Add one particular callback structure */
		gpio_add_callback(dev, &gpio_cb[gpio_cb_number]);

		/* Increase group numbers */
		gpio_cb_number++;
	}
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
	uint8_t	axle_position;
	uint8_t	axle_rotate_direction;
	int8_t	axle_rotate_times;

	uint16_t i;
	int rc = 0;
again:
	/* Boundary check */
	if (!( destination_position >= 1 && destination_position <= 7 ) )
	{
		printk("Error at LINE: %d\n", __LINE__);
		return -1;
	}

	/* Get current axle position */
	axle_position = axle_position_read();

	/* Check position */
	if ( !( axle_position >=1 && axle_position <= 7 ) )
	{
		printk("Error at LINE: %d, axle_position == %d\n", __LINE__, axle_position);
		return -1;
	}

	/**
	 * When we assume the A4 is the center of the axle, either rotate clockwise
	 * or anticlockwise, the axle both need rotate three times to get the limit
	 * position.
	 *
	 * By analyzing the mathematics of axle rotate pattern, we find that: move
	 * axle from N position to M position, the rotate times will be |N-M|, the
	 * rotate direction depending on N and M.
	 * */
	axle_rotate_times = (int8_t)axle_position - (int8_t)destination_position;
	printk("Rotate times == %d\n", axle_rotate_times);

	/* Already at requested position ? */
	if ( 0 == axle_rotate_times )
	{
		return 0;
	}

	/* Positive rotate clockwise, Negetive rotate anticlockwise */
	axle_rotate_direction = axle_rotate_times > 0 ? 1 : 0;
	axle_set_rotate_direction(axle_rotate_direction);

	/* Get ride of signed, since we already set rotate direction */
	axle_rotate_times = axle_rotate_times >= 0 ? axle_rotate_times : -axle_rotate_times;

	/**
	 * At this point, we are not at destination position
	 * start rotate to destination position.
	 * */

	/* Enable destination position's gpio irq */
	axle_in_position_irq_enable(destination_position);

	/* Unlock the axle break */
	axle_set_rotate_lock_unlock(1);

	/* Wait the break fully unlocked */
	//k_sleep(50);

	k_sem_reset(&axle_in_position_sem);

	/* Start to rotate axle */
	axle_set_rotate_enable_disable(1);

	rc = k_sem_take(&axle_in_position_sem,
			CONFIG_APP_AXLE_ROTATE_TIMEOUT_IN_SEC * 1000 * axle_rotate_times);

	/* Lock the axle break */
	axle_set_rotate_lock_unlock(0);

	/* Stop rotate axle */
	axle_set_rotate_enable_disable(0);

	/* Disable destination position's gpio irq */
	axle_in_position_irq_disable(destination_position);

	if ( 0 != rc )
	{
		/* Semaphore take error */
		axle_status = false;
		return -1;
	}

	if ( 0 == axle_position_read() )
	{
		axle_status = false;
		return -1;
	}

	if ( destination_position != axle_position_read() )
	{
		/* Destination not reached, do it again */
		printk("Destination not reached, position = %d\n", axle_position_read());
		goto again;
	}
	/* Axle rotate to correct position */
	return 0;
}

/**
 * @brief Rotate to next position
 *
 * This function is for administrator usage
 *
 * @return  0, rotate success
 *		   -1, rotate error
 * */
int8_t axle_rotate_to_next(void)
{
	uint8_t axle_position = axle_position_read();

	/* Can not read the position */
	if ( 0 == axle_position )
	{
		return -1;
	}
	/* Already at the last position */
	else if ( 7 == axle_position )
	{
		return 0;
	}
	else
	{
		return axle_rotate_to(axle_position + 1);
	}
}

/**
 * @brief Rotate axle 360/7 degree with direction of @direction
 *
 * @param direction The direction the the axle should rotate
 *					1 rotate clockwise
 *					0 rotate anticlockwise
 *
 * @return  0, rotate success
 *		   -1, rotate error
 * */
static int8_t axle_rotate_init(uint8_t direction)
{
	uint8_t i, rc = 0;

	/* Set direction signal */
	axle_set_rotate_direction(direction);

	/* Unlock the break */
	axle_set_rotate_lock_unlock(1);

	/* Wait the lock fully unlocked */
	k_sleep(50);

	/* Enable gpio irq */
	for ( i = 1; i <= 7; ++i )
	{
		axle_in_position_irq_enable(i);
	}

	k_sem_reset(&axle_in_position_sem);
	axle_status = true;

	/* Enable axle */
	axle_set_rotate_enable_disable(1);

	rc = k_sem_take(&axle_in_position_sem,
				CONFIG_APP_AXLE_ROTATE_TIMEOUT_IN_SEC * 1000);

	/* Lock the axle */
	axle_set_rotate_lock_unlock(0);

	/* Disable axle */
	axle_set_rotate_enable_disable(0);

	/* Disable all position gpio irq */
	for ( i = 1; i <= 7; ++i )
	{
		axle_in_position_irq_disable(i);
	}

	if ( 0 != rc )
	{
		/* Semaphore take error */
		axle_status = false;
		SYS_LOG_ERR("Error at line: %d, rc = %d", __LINE__, rc);
		return rc;
	}

	if ( 0 == axle_position_read() )
	{
		axle_status = false;
		SYS_LOG_ERR("Error at line: %d, Can not read the position", __LINE__);
		return -1;
	}

	SYS_LOG_DBG("Rotate init OK\n");

	printk("Rotate_init done, position = %d\n", axle_position_read());
	axle_status = true;

	return 0;
}

/**
 * @brief Axle initial function
 *		  Rotate the axle to place that in position
 *
 * @return  0 OK
 *		   -1 Failed
 * */
int8_t axle_init(void)
{
	uint8_t i;
	uint32_t temp;

	axle_in_position_irq_init();

	/* Flush the axle position gpio input */
	for ( i = 0; i < 7; ++i )
	{
		gpio_comm_read(&axle_gpio_table[i], &temp);
	}

	/* Set direction gpio as output and level it up for default */
	gpio_comm_conf(&axle_gpio_table[8], GPIO_DIR_OUT | GPIO_PUD_PULL_UP);

	/* Flush output and set rotate anticlockwise */
	axle_set_rotate_direction(0);

	/* Set the break gpio as output and level it up for default */
	gpio_comm_conf(&axle_gpio_table[9], GPIO_DIR_OUT | GPIO_PUD_PULL_UP);

	/* Flush output and set the break locked */
	axle_set_rotate_lock_unlock(0);

	k_sem_init(&axle_in_position_sem, 0, 1);

	/* Current axle already in position, no need to rotate */
	if ( 0 != axle_position_read() )
	{
		printk("Current axle is in position!\n");
		axle_status = true;
		return 0;
	}
	printk("Current axle is NOT in position!\n");

	/**
	 * Rotate the axle to make the axle at one certain position
	 *
	 * If rotate anticlockwise may reach the limitation of rotate
	 * angles, then try to rotate clockwise
	 * */
	if ( 0 == axle_rotate_init(0) || 0 == axle_rotate_init(1) )
	{
		printk("Axle rotate init done!\n");
		axle_status = true;
		return 0;
	}

	printk("Can not reach the cerntain position\n");
	SYS_LOG_ERR("Can not reach the certain position");
	axle_status = false;
	return -1;
}

#ifdef CONFIG_APP_AXLE_FACTORY_TEST

int8_t axle_factory_test(void)
{
	return 0;
}

#endif /* CONFIG_APP_AXLE_FACTORY_TEST */

#ifdef CONFIG_APP_AXLE_DEBUG

void axle_debug(void)
{
	int rc;
	uint8_t i;
	printk("[%s]: Start to init...\n", __func__);
	if ( 0 != axle_init() )
	{
		printk("[%s]: Init Error!\n", __func__);
	}
	else
	{
		printk("[%s]: Init OK\n", __func__);
	}
	k_sleep(2000);
	while ( 1 )
	{
		printk("Start to rotate...\n");
		for ( i = 1; i <= 7; ++i )
		{
			printk("Start to rotate to %d...\n", i);
			rc = axle_rotate_to(i);
			if ( 0 != rc )
			{
				printk("Rotate failed, rc = %d\n", rc);
			}
			else
			{
				printk("Rotate OK.\n");
			}
			k_sleep(2000);
		}
		k_sleep(2000);
		for ( i = 7; i >= 1; --i )
		{
			printk("Start to rotate to %d...\n", i);
			rc = axle_rotate_to(i);
			if ( 0 != rc )
			{
				printk("Rotate failed, rc = %d\n", rc);
			}
			else
			{
				printk("Rotate OK.\n");
			}
			k_sleep(2000);
		}
		k_sleep(2000);
	}
	return ;
}

#endif /* CONFIG_APP_AXLE_DEBUG */

