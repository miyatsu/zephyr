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

#define	SYS_LOG_DOMAIN	"door"
#define	SYS_LOG_LEVEL	SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>

#include "config.h"
#include "gpio_comm.h"

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
	/**
	 * motor open, morotr close,
	 * opened detector, closed detector, infrared detector
	 * */
	{
		{GPIO_GROUP_D, 10}, {GPIO_GROUP_D, 9},
		{GPIO_GROUP_D,  8}, {GPIO_GROUP_E, 15}, {GPIO_GROUP_E, 14}
	},
	{
		{GPIO_GROUP_E, 13}, {GPIO_GROUP_E, 12},
		{GPIO_GROUP_E, 11}, {GPIO_GROUP_E, 10}, {GPIO_GROUP_E,  9}
	},
	{
		{GPIO_GROUP_E,  8}, {GPIO_GROUP_E,  7},
		{GPIO_GROUP_D,  1}, {GPIO_GROUP_D,  0}, {GPIO_GROUP_D, 15}
	},
	{
		{GPIO_GROUP_D, 14}, {GPIO_GROUP_D,  7},
		{GPIO_GROUP_D,  6}, {GPIO_GROUP_D,  5}, {GPIO_GROUP_D,  4}
	}
};

/* Door functionality status */
static bool door_status[] = {false, false, false, false};

/**
 * @brief Get door status
 *
 * @return pointer point to door status array
 * */
const bool* door_get_status_array(void)
{
	return door_status;
}

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

static bool door_is_door_opened(uint8_t layer)
{
	int32_t value;

	gpio_comm_read(&door_gpio_table[layer - 1][2], &value);

	if ( 0 == value )
	{
		return true;
	}
	return false;
}

static bool door_is_door_closed(uint8_t layer)
{
	int32_t value;

	gpio_comm_read(&door_gpio_table[layer - 1][3], &value);

	if ( 0 == value )
	{
		return true;
	}
	return false;
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
 * @brief Determine which layer's gpio trigger the irq handler
 *
 * @param dev Device pointer trigger the gpio irq
 *
 * @param pins Pin mask that trigger current irq handler
 *
 * @param index 2 means open detector
 *				3 means close detector
 *				4 means on door infrared detector
 * */
static uint8_t door_irq_to_layer(struct device *dev, uint32_t pins, uint8_t index)
{
	uint8_t i;
	uint32_t pin = 0;
	struct device *dev_temp;

	/**
	 * Parse pin mask to pin number
	 *
	 * FIXME: Two or more irq comming with one callback called,
	 * The pins will not be the power of two.
	 * */
	while ( 1 != pins )
	{
		pins >>= 1;
		pin++;
	}
	for ( i = 0; i < 4; ++i )
	{
		dev_temp = device_get_binding(gpio_group_dev_name_table	\
				[door_gpio_table[i][index].gpio_group]);

		if ( dev_temp == dev && pin == door_gpio_table[i][index].gpio_pin )
		{
			return i + 1;
		}
	}

	/* No find */
	return 0;
}

static void door_comm_irq_enable(uint8_t layer, uint8_t index)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table	\
			[door_gpio_table[layer - 1][index].gpio_group]);
	gpio_pin_enable_callback(dev, door_gpio_table[layer - 1][index].gpio_pin);
}

static void door_comm_irq_disable(uint8_t layer, uint8_t index)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table	\
			[door_gpio_table[layer - 1][index].gpio_group]);
	gpio_pin_disable_callback(dev, door_gpio_table[layer - 1][index].gpio_pin);
}

static inline void door_open_in_position_irq_enable(uint8_t layer)
{
	door_comm_irq_enable(layer, 2);
}

static inline void door_open_in_position_irq_disable(uint8_t layer)
{
	door_comm_irq_disable(layer, 2);
}

static inline void door_close_in_position_irq_enable(uint8_t layer)
{
	door_comm_irq_enable(layer, 3);
}

static inline void door_close_in_position_irq_disable(uint8_t layer)
{
	door_comm_irq_disable(layer, 3);
}

static inline void door_infrared_irq_enable(uint8_t layer)
{
	door_comm_irq_enable(layer, 4);
}

static inline void door_infrared_irq_disable(uint8_t layer)
{
	door_comm_irq_disable(layer, 4);
}

/**
 * @brief Callback function, called when the door is fully opened
 *
 * @param dev Gpio device pointer
 *
 * @param gpio_cb Gpio callback structure
 *
 * @param pins Mask of pins that trigger the callback
 * */
static void door_open_in_position_irq_cb(struct device *dev,
										struct gpio_callback *gpio_cb,
										uint32_t pins)
{
	uint8_t layer = door_irq_to_layer(dev, pins, 2);

	if ( 0 == layer )
	{
		/* Error */
		return ;
	}

	/* Disable the open detector irq, in case of trigger by accident */
	door_open_in_position_irq_disable(layer);

	/* Stop openning the door */
	door_stop_write_gpio(layer);
}

/**
 * @brief Callback function, called when the door is fully closed
 *
 * @param dev Gpio device pointer
 *
 * @param gpio_cb Gpio callback structure
 *
 * @param pins Mask of pins that trigger the callback
 * */
static void door_close_in_position_irq_cb(struct device *dev,
										struct gpio_callback *gpio_cb,
										uint32_t pins)
{
	uint8_t layer = door_irq_to_layer(dev, pins, 3);

	if ( 0 == layer )
	{
		/* Error */
		return ;
	}

	/* Disable the close detector irq, in case of trigger by accident */
	door_close_in_position_irq_disable(layer);

	/* Disable the on door infrared detector */
	door_infrared_irq_disable(layer);

	/* Stop closing the door */
	door_stop_write_gpio(layer);
	//printk("%s called\n", __func__);
}

static volatile bool door_on_door_infrared_detected_flag[4] =
{
	false, false, false, false
};

/**
 * @brief Callback function, called when the on door infrared detecte
 *		  something while the door is at closing stage.
 *
 * @param dev Gpio device pointer
 *
 * @param gpio_cb Gpio callback structure
 *
 * @param pins Mask of pins that trigger the callback
 * */
static void door_infrared_irq_cb(struct device *dev,
								struct gpio_callback* gpio_cb,
								uint32_t pins)
{
	uint8_t layer = door_irq_to_layer(dev, pins, 4);

	if ( 0 == layer )
	{
		/* Error */
		return ;
	}

	/* Mark jamed onece */
	door_on_door_infrared_detected_flag[layer - 1] = true;

	/* Disable the on door infrared detector */
	door_infrared_irq_disable(layer);

	/* Disable close in position gpio irq */
	door_close_in_position_irq_disable(layer);

	/* Re-Enable open in position gpio irq */
	door_open_in_position_irq_enable(layer);

	/* Start to open, while the door could be jam by forgin matters */
	door_open_write_gpio(layer);
}

/**
 * @brief Door gpio irq initial function
 *
 * @param index IRQ type, 2 means open detector
 *						  3 means close detector
 *						  4 means on door infrared detector
 * */
static void door_comm_irq_init(uint8_t index)
{
	static struct gpio_callback gpio_cb[12];
	static uint8_t gpio_cb_number = 0;

	uint8_t i, j;
	uint32_t pin_mask;
	struct device *dev;
	bool gpio_initialized_table[4] = {false, false, false, false};

	/* Initial all 4 layer's gpio */
	for ( i = 0; i < 4; ++i )
	{
		if ( gpio_initialized_table[i] )
		{
			/* Already initialized */
			continue;
		}

		dev = device_get_binding(gpio_group_dev_name_table	\
				[door_gpio_table[i][index].gpio_group]);
		pin_mask = 0;

		/* Get the same group pins into pin_mask */
		for ( j = i; j < 4; ++j )
		{
			/**
			 * The index j's gpio name is the same as current dev,
			 * initial them at onece.
			 * */
			if ( door_gpio_table[i][index].gpio_group ==
					door_gpio_table[j][index].gpio_group )
			{
				/* Configure current gpio as interrupt input */
				gpio_comm_conf(&door_gpio_table[j][index],
						GPIO_DIR_IN | GPIO_INT | GPIO_INT_DEBOUNCE |	\
						GPIO_PUD_PULL_UP | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW);

				/* Add new pin number into pin_mask */
				pin_mask |= BIT(door_gpio_table[j][index].gpio_pin);

				/* Skip current pin on initialization */
				gpio_initialized_table[j] = true;
				k_sleep(10);
			}
		}

		/* Initial callback */
		switch ( index )
		{
			/* Open detector */
			case 2:
				gpio_init_callback(&gpio_cb[gpio_cb_number],
								   door_open_in_position_irq_cb,
								   pin_mask);
				break;
			/* Close detector */
			case 3:
				gpio_init_callback(&gpio_cb[gpio_cb_number],
								   door_close_in_position_irq_cb,
								   pin_mask);
				break;
			/* On door infrared detector */
			case 4:
				gpio_init_callback(&gpio_cb[gpio_cb_number],
								   door_infrared_irq_cb,
								   pin_mask);
				break;
			default:
				/* Error */
				break;
		}

		/* Add one particular callback structure */
		gpio_add_callback(dev, &gpio_cb[gpio_cb_number]);

		/* Increase group numbers */
		gpio_cb_number++;
	}
}

/**
 * @brief Wrapper function of door gpio irq initial
 * */
static inline void door_irq_init(void)
{
	k_sleep(10);
	door_comm_irq_init(2);

	k_sleep(10);
	door_comm_irq_init(3);

	k_sleep(10);
	door_comm_irq_init(4);
}

/**
 * @brief This function is to check the door open state, and stop opening
 *		  operation.
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return -1 Door has never open
 *			0 Door has fully opened
 *		   -2 Door open timeout, and leave it half open
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

	/* Disable open in position irq */
	door_open_in_position_irq_disable(layer);

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
			return -2;
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
	uint8_t i, wait_close_time_in_sec = CONFIG_APP_DOOR_CLOSE_TIMEOUT_IN_SEC;

	/**
	 * 1 means the door is not at close state
	 * 0 means the door is at close state
	 * */
	uint32_t door_not_in_position;

	int8_t rc;

retry:

	if ( retry_times >= 3 )
	{
		/* Stop rotate the door when timeout or fully closed */
		door_stop_write_gpio(layer);

		/* Disable door close in position irq */
		door_close_in_position_irq_disable(layer);

		/* Disable on door infrared detector irq */
		door_infrared_irq_disable(layer);

		return -2;
	}

	for ( i = 0; i < wait_close_time_in_sec * 10; ++i )
	{
		/* Door close state */
		gpio_comm_read(&door_gpio_table[layer - 1][3], &door_not_in_position);
		if ( !door_not_in_position )
		{
			/* Door fully closed */
			break ;
		}

		/* Door still not closed */

		/* Interrupt detected */
		if ( door_on_door_infrared_detected_flag[layer - 1] )
		{
			/* Clear infrared irq flag */
			door_on_door_infrared_detected_flag[layer - 1] = false;

			/**
			 * The door will automatic open and enable nessesary IRQs
			 * when the infrared irq handler is trigger by irq.
			 *
			 * Wait door fully opened
			 * */
			rc = door_wait_open(layer);
			if ( 0 != rc )
			{
				return rc;
			}

			/**
			 * Door has fully opened,
			 * wait for three seconds and retry to close the door
			 * */
			k_sleep(3000);

			retry_times++;

			/* After door has fully opened, no irq is enabled */

			/* Enable close in position irq */
			door_close_in_position_irq_enable(layer);

			/* Enable on door infrared detector irq */
			door_infrared_irq_enable(layer);

			/* Start to close */
			door_close_write_gpio(layer);

			goto retry;
		}

		/**
		 * Door not close, and no jam by other things
		 * */
		k_sleep(100);
	}

	/* Stop rotate the door when timeout or fully closed */
	door_stop_write_gpio(layer);

	/* Disable door close in position irq */
	door_close_in_position_irq_disable(layer);

	/* Disable on door infrared detector irq */
	door_infrared_irq_disable(layer);

	if ( i >= wait_close_time_in_sec * 10 )
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
 *		   -2 Door open timeout, and leave it half open
 * */
int8_t door_open(uint8_t layer)
{
	int8_t rc;

	if ( door_is_door_opened(layer) )
	{
		return 0;
	}

	/* Enable open in position irq */
	door_open_in_position_irq_enable(layer);

	/* Start to open the door */
	door_open_write_gpio(layer);
	SYS_LOG_DBG("opening layer %d, start to wait door fully opened...", layer);

	/* Wait door fully opened */
	rc = door_wait_open(layer);
	if ( 0 != rc )
	{
		SYS_LOG_ERR("open error at layer %d, return value of %d", layer, rc);
		door_status[layer - 1] = false;
	}
	else
	{
		SYS_LOG_DBG("open ok at layer %d", layer);
		door_status[layer - 1] = true;
	}
	return rc;
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
	int8_t rc;

	if ( door_is_door_closed(layer) )
	{
		return 0;
	}

	/* Enable close in position irq */
	door_close_in_position_irq_enable(layer);

	/* Enable on door infrared irq */
	door_infrared_irq_enable(layer);

	/* Start to close the door */
	door_close_write_gpio(layer);
	SYS_LOG_DBG("closing layer %d, start to wait door fully closed...", layer);

	/* Wait door fully closed */
	rc = door_wait_close(layer);
	if ( 0 != rc )
	{
		SYS_LOG_ERR("close error at layer %d, return value of %d", layer, rc);
		door_status[layer - 1] = false;
	}
	else
	{
		SYS_LOG_DBG("close ok at layer %d", layer);
		door_status[layer - 1] = true;
	}
	return rc;
}

/**
 * @brief Door open work thread function
 *
 * When administrator want to manage the machine, if we try to open
 * the door one by one, it will cost a lot of time to wait.
 *
 * We decide to make it as a single work thread, use 4 thread to do
 * the door open job.
 *
 * @param arg1, Door number's pointer, input as [1-4]
 *		  arg2, Sem use to sync thread
 *		  arg3, Return status
 * */
static void door_open_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
	int32_t *		layer	= (int32_t *)arg1;
	struct k_sem *	sem		= (struct k_sem *)arg2;
	int32_t *		rc		= (int32_t *)arg3;

	*rc = door_open(*layer);

	k_sem_give(sem);
}

/**
 * @brief Door close work thread function
 *
 * Same as above function: door_open_thread_entry_point()
 *
 * @param arg1, Door number's pointer, input as [1-4]
 *		  arg2, Sem use to sync thread
 *		  arg3, Return status
 * */
static void door_close_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
	int32_t *		layer	= (int32_t *)arg1;
	struct k_sem *	sem		= (struct k_sem *)arg2;
	int32_t *		rc		= (int32_t *)arg3;

	*rc = door_close(*layer);

	k_sem_give(sem);
}

/**
 * Thread stack definition
 *
 * This stack name of door_comm_thread_stack
 * will reuse by door_admin_open()/door_admin_close() function
 * */
static K_THREAD_STACK_ARRAY_DEFINE(door_comm_thread_stack, 4,
		CONFIG_APP_DOOR_INIT_THREAD_STACK_SIZE);

/**
 * @brief Close all four doors, and can be use as hardware initial function
 *
 * @return  0 Initial OK
 *		   <0 Initial Failed
 * */
int8_t door_admin_close(void)
{
	struct k_thread	door_init_thread[4];

	int32_t layer[4] = {1, 2, 3, 4};
	int32_t thread_rc[4] = {0xFC, 0xFC, 0xFC, 0xFC};

	int32_t rc;

	uint8_t i;

	/* Used to sync this thread and four init thread  */
	struct k_sem sem;

	k_sem_init(&sem, 0, 4);

	/* Start to init doors */
	for ( i = 0; i < 4; ++i )
	{
		k_thread_create( &door_init_thread[i],
			door_comm_thread_stack[i],
			K_THREAD_STACK_SIZEOF(door_comm_thread_stack[i]),
			door_close_thread_entry_point,
			(void *)&layer[i], (void *)&sem, (void*)&thread_rc[i],
			0, 0, K_NO_WAIT );
			/* Prio: 0, Flag: 0, Delay: No delay */
	}

	/* Wait all four initial thread return */
	for ( i = 0; i < 4; ++i )
	{
		rc = k_sem_take(&sem, K_SECONDS(CONFIG_APP_DOOR_INIT_TIMEOUT_IN_SEC));
		if ( 0 != rc )
		{
			/* Timeout or error happened */
			return -1;
		}
	}

	/* Check initial return value */
	for ( i = 0; i < 4; ++i )
	{
		if ( 0 != thread_rc[i] )
		{
			return -1;
		}
	}

	/**
	 * TODO Which door are broken ?
	 * Return to upstream by check the door_init_status
	 * ( *(int8_t *)((int32_t)&door_init_status + i) )
	 * */

	/* Initial doors ok */
	return 0;
}

/**
 * @brief Open all 4 doors
 *
 * @return  0, open success
 *		   <0, open error
 * */
int8_t door_admin_open(void)
{
	struct k_thread door_open_thread[4];

	int32_t layer[4] = {1, 2, 3, 4};
	int32_t thread_rc[4] = {0xFC, 0xFC, 0xFC, 0xFC};

	int32_t rc;

	uint8_t i;

	struct k_sem sem;

	k_sem_init(&sem, 0, 4);

	/* Start to open doors */
	for ( i = 0; i < 4; ++i )
	{
		k_thread_create( &door_open_thread[i],
			door_comm_thread_stack[i],
			K_THREAD_STACK_SIZEOF(door_comm_thread_stack[i]),
			door_open_thread_entry_point,
			(void *)&layer[i], (void *)&sem, (void*)&thread_rc[i],
			0, 0, K_NO_WAIT );
			/* Prio: 0, Flag: 0, Delay: No delay */
	}

	/* Wait door to opened */
	for ( i = 0; i < 4; ++i )
	{
		rc = k_sem_take(&sem, K_SECONDS(CONFIG_APP_DOOR_OPEN_TIMEOUT_IN_SEC));
		if ( 0 != rc )
		{
			return -1;
		}
	}

	for ( i = 0; i < 4; ++i )
	{
		if ( 0 != thread_rc[i] )
		{
			return -1;
		}
	}

	return 0;
}

/**
 * @brief Initial GPIO related functions
 * */
static void door_gpio_init(void)
{
	uint8_t i, j;
	uint32_t temp;
	for ( i = 0; i < 4; ++i )
	{
		gpio_comm_conf(&door_gpio_table[i][0], GPIO_DIR_OUT | GPIO_PUD_PULL_UP);
		gpio_comm_conf(&door_gpio_table[i][1], GPIO_DIR_OUT | GPIO_PUD_PULL_UP);
	}
	for ( i = 0; i < 4; ++i )
	{
		for ( j = 0; j < 5; ++j )
		{
			gpio_comm_read(&door_gpio_table[i][j], &temp);
		}
	}
}

/**
 * @brief Initial all initialize of door functions, include set gpio mode
 *		  and close all four doors etc.
 *
 * @return -1, initial error
 *			0, initial ok
 * */
int8_t door_init(void)
{
	door_irq_init();
	door_gpio_init();
	return door_admin_close();
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
	uint8_t i;
	printk("Door test start...\n");
	door_init();
	printk("Init done!\n");

	printk("Start to test Admin open...\n");
	if ( 0 == door_admin_open() )
	{
		printk("Admin open ok.\n");
	}
	else
	{
		printk("Admin open error.\n");
	}
	k_sleep(3000);
	while (1)
	{
		for ( i = 1; i <= 4; ++i )
		{
			k_sleep(2000);

			printk("Start to open door at layer %d\n", i);
			k_sleep(1000);
			if ( 0 == door_open(i) )
			{
				printk("Door on layer %d open ok.\n", i);
			}
			else
			{
				printk("Door on layer %d open error.\n", i);
			}

			k_sleep(2000);

			printk("Start to close door at layer %d\n", i);
			k_sleep(1000);
			if ( 0 == door_close(i) )
			{
				printk("Door on layer %d close ok.\n", i);
			}
			else
			{
				printk("Door on layer %d close error.\n", i);
			}
		}
	}
	return ;
}

#endif /* CONFIG_APP_DOOR_DEBUG */

