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
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <kernel.h>
#include <gpio.h>

#include "config.h"
#include "gpio_comm.h"

#define	SYS_LOG_DOMAIN	"door"
#ifdef CONFIG_APP_DOOR_DEBUG
	#define	SYS_LOG_LEVEL	SYS_LOG_LEVEL_DEBUG
#else
	#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_ERROR
#endif
#include <logging/sys_log.h>

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

/**
 * This is door related gpio table, struct gpio_group_pin_t is defined
 * in file gpio_comm.h.
 *
 * Each layer need five gpio to control the door:
 *
 * Motor open, Motor close,
 * Opened detector, Closed detector, Infrared detector
 *
 * Door will open when Motor open is active high and Motor close is active low
 * Door will close when Motor open is active low and Motor close is active high
 * Door will stop when Motor open and Motor close both active high/low
 *
 * All 20 gpios are lay down below, DO NOT edit it if it really have to.
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
		{GPIO_GROUP_D, 14}, {GPIO_GROUP_D,  4},
		{GPIO_GROUP_D,  5}, {GPIO_GROUP_D,  6}, {GPIO_GROUP_D,  7}
	}
};

/**
 * Door functionality status.
 * Flase means the door is broken, true means the door is fine
 *
 * This status will send to clould when received a command each time
 * Clould will use this status to determine if the box is OK or NOT
 *
 * Note: Here we use __attribute__((aligned (4))) to force every one
 * of the bool item occupied 4 byte in case of compiler optimize it
 * into one byte. If each one of the bool item occupied 1 byte, in
 * 32-bit arm machine, access one item in multi-thread MAY cause
 * unpredictable behavior.
 * */
static volatile bool door_status[4]
	__attribute__((aligned (4))) = {false, false, false, false};

/**
 * @brief Get door status array pointer
 *
 * @return pointer point to door status array, constant variable, read only
 * */
const volatile bool* door_get_status_array(void)
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

typedef enum door_state_e
{
	DOOR_STATE_OPENED,
	DOOR_STATE_CLOSED,
	DOOR_STATE_HALF,
	DOOR_STATE_INVALID,
}door_state_t;

/**
 * @brief
 *
 * @param layer [1-4] is expected
 *
 * @return one of the four state defined above
 * */
static door_state_t door_get_door_state(uint8_t layer)
{
	int32_t open_pin_value, close_pin_value;

	gpio_comm_read(&door_gpio_table[layer - 1][2], &open_pin_value);
	gpio_comm_read(&door_gpio_table[layer - 1][3], &close_pin_value);

	/**
	 * Open switch active low (Triggered)
	 * Close switch active high (Not triggered)
	 * */
	if ( 0 == open_pin_value && 0 != close_pin_value )
	{
		/* Opened */
		//SYS_LOG_DBG("Opened state detected!");
		return DOOR_STATE_OPENED;
	}
	/**
	 * Open switch active high (Not triggered)
	 * Close switch active low (Triggered)
	 * */
	else if ( 0 != open_pin_value && 0 == close_pin_value )
	{
		/* Closed */
		//SYS_LOG_DBG("Closed state detected!");
		return DOOR_STATE_CLOSED;
	}
	/**
	 * Open switch active high (Not triggered)
	 * Close switch active high (Not triggered)
	 * */
	else if ( 0 != open_pin_value && 0 != close_pin_value )
	{
		/* Half open/close */
		//SYS_LOG_DBG("Half open/close state detected!");
		return DOOR_STATE_HALF;
	}
	/**
	 * Open switch active low (Triggered)
	 * Close switch active low (Triggered)
	 * */
	else
	{
		/* Impossibile */
		//SYS_LOG_ERR("Impossible door state detected!");
		return DOOR_STATE_INVALID;
	}
}

static bool door_is_infrared_detected(uint8_t layer)
{
	uint32_t infrared_pin_value;
	gpio_comm_read(&door_gpio_table[layer - 1][4], &infrared_pin_value);
	return !infrared_pin_value;
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
 *
 * @return The layer number, [1-4] is expected
 *		   Any other value except [1-4] is invalid
 * */
static uint8_t door_irq_to_layer(struct device *dev, uint32_t pins, uint8_t index)
{
	uint8_t i = 0;
	uint32_t pin = 0;
	struct device *dev_temp = NULL;

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

/* Used to sync main thread and irq */
static struct k_sem door_open_in_position_irq_sem[4];
static struct k_sem door_close_in_position_irq_sem[4];

static void door_irq_sem_init(void)
{
	int i;
	for ( i = 0; i < 4; ++i )
	{
		k_sem_init(&door_open_in_position_irq_sem[i], 0, 1);
		k_sem_init(&door_close_in_position_irq_sem[i], 0, 2);
	}
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
		/* Error, never expected */
		SYS_LOG_DBG("Parse layer error, pins = %d", pins);
	}

	/* Disable the open detector irq, in case of trigger by accident */
	door_open_in_position_irq_disable(layer);

	/* Stop openning the door */
	door_stop_write_gpio(layer);

	/* Give semaphore */
	k_sem_give(&door_open_in_position_irq_sem[layer - 1]);
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
		/* Error, never expected */
		SYS_LOG_DBG("Parse layer error, pins = %d", pins);
	}

	/* Disable the close detector irq, in case of trigger by accident */
	door_close_in_position_irq_disable(layer);

	/* Disable the on door infrared detector */
	door_infrared_irq_disable(layer);

	/* Stop closing the door */
	door_stop_write_gpio(layer);

	/* Give semaphore */
	k_sem_give(&door_close_in_position_irq_sem[layer - 1]);
}

static volatile bool door_on_door_infrared_detected_flag[4]
	__attribute__((aligned(4))) = {false, false, false, false};

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
		/* Error, never expected */
		SYS_LOG_DBG("Parse layer error, pins = %d", pins);
	}

	/* Mark current door jamed once */
	door_on_door_infrared_detected_flag[layer - 1] = true;

	/* Disable the on door infrared detector */
	door_infrared_irq_disable(layer);

	/* Disable close in position gpio irq */
	door_close_in_position_irq_disable(layer);

	/* Stop closing the door */
	door_stop_write_gpio(layer);

	/* Give semaphore */
	k_sem_give(&door_close_in_position_irq_sem[layer - 1]);
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
	door_comm_irq_init(2);

	door_comm_irq_init(3);

	door_comm_irq_init(4);
}

/**
 * @brief Public wrapper function of door open
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return	 0 Door has fully opened
 *			-1 Door open timeout
 *			-2 Door never opened
 *			-3 Door half opened
 *			-4 Door is at opened AND closed state, impossible
 * */
int door_open(uint8_t layer)
{
	int i, rc = 0;
	door_state_t door_state;

	door_state = door_get_door_state(layer);
	if ( DOOR_STATE_OPENED == door_state )
	{
		goto out;
	}

	/* Reset semaphore before enable irq */
	k_sem_reset(&door_open_in_position_irq_sem[layer - 1]);

	/* Enable open in position irq */
	door_open_in_position_irq_enable(layer);

	/* Start to open the door */
	door_open_write_gpio(layer);

	SYS_LOG_DBG("opening layer %d, start to wait door fully opened...", layer);

	/* Wait irq and polling the GPIO status */
	for ( i = 0; i < CONFIG_APP_DOOR_OPEN_TIMEOUT_IN_SEC * 10; ++i )
	{
		/* Wait irq triggered */
		rc = k_sem_take(&door_open_in_position_irq_sem[layer - 1], 100);
		if ( 0 == rc )
		{
			/* Triggered */
			SYS_LOG_DBG("IRQ triggered, layer = %d, i = %d", layer, i);
			break;
		}

		/* IRQ not triggered, polling the GPIO status */

		/**
		 * Open swtich pin will active low when the door is at
		 * OPENED or INVALID state
		 * */
		door_state_t door_state = door_get_door_state(layer);
		if ( DOOR_STATE_OPENED == door_state ||
			 DOOR_STATE_INVALID == door_state )
		{
			rc = 0;
			break;
		}
	}

	/**
	 * IRQ triggered or timeout on semaphore take,
	 * run clean process stop motor and disable irq.
	 *
	 * Actually the stop op and disable irq op, we can check the return
	 * value of k_sem_take() determine wether we already done that or
	 * not. For safty consideration, we still do that agine regardless
	 * of return code of k_sem_take().
	 * */

	/* Stop motor */
	door_stop_write_gpio(layer);

	/* Disable open in position irq */
	door_open_in_position_irq_disable(layer);

	if ( i >= CONFIG_APP_DOOR_OPEN_TIMEOUT_IN_SEC * 10 )
	{
		/* Wait GPIO timeout */
		rc = -1;
		goto out;
	}

	/* Wait GPIO flush its status */
	k_sleep(10);

	/* Check door state twice before return */
	door_state = door_get_door_state(layer);
	if ( DOOR_STATE_CLOSED == door_state )
	{
		/* Door never opened */
		rc = -2;
	}
	else if ( DOOR_STATE_HALF == door_state )
	{
		/* Door opened, but not fully opened */
		rc = -3;
	}
	else if ( DOOR_STATE_INVALID == door_state )
	{
		/* Both state of opened and closed, impossible */
		rc = -4;
	}

out:
	if ( 0 == rc )
	{
		door_status[layer - 1] = true;
		SYS_LOG_DBG("open ok at layer %d", layer);
	}
	else
	{
		door_status[layer - 1] = false;
		SYS_LOG_ERR("open error at layer %d, return value of %d", layer, rc);
	}

	return rc;
}

/**
 * @brief Public wrapper function of door close
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return	 1 On door infrared detected
 *			 0 Door has fully closed
 *			-1 Door close timeout
 *			-2 Door never closed
 *			-3 Door half closed
 *			-4 Door is at opened AND closed state, impossible
 * */
int door_close(uint8_t layer)
{
	int i, rc = 0;
	door_state_t door_state;

	door_state = door_get_door_state(layer);
	if ( DOOR_STATE_CLOSED == door_state )
	{
		goto out;
	}

	/* Reset semaphore before enable irq */
	k_sem_reset(&door_close_in_position_irq_sem[layer - 1]);

	/* Enable close in position irq */
	door_close_in_position_irq_enable(layer);

	/* Enable on door infrared irq */
	door_infrared_irq_enable(layer);

	/* Start to close the door */
	door_close_write_gpio(layer);

	SYS_LOG_DBG("closing layer %d, start to wait door fully closed...", layer);

	/* Wait irq and polling GPIO status */
	for ( i = 0; i < CONFIG_APP_DOOR_CLOSE_TIMEOUT_IN_SEC * 10; ++i )
	{
		/* Wait irq triggered */
		rc = k_sem_take(&door_close_in_position_irq_sem[layer - 1], 100);
		if ( 0 == rc )
		{
			/* Triggered */
			SYS_LOG_DBG("IRQ triggered, layer = %d, i = %d", layer, i);
			break;
		}

		/* IRQ not triggered, polling the GPIO status */

		/* Polling infrared detector status */
		if ( door_is_infrared_detected(layer) )
		{
			/* Infrared detected */

			/**
			 * IRQ will put an true into this array, but puting the same
			 * value into this array will not cause memory consistancy problem
			 * */
			door_on_door_infrared_detected_flag[layer - 1] = true;
			break;
		}

		/**
		 * Polling the close detector switch status,
		 * close swtich pin will active low when the door is at
		 * CLOSED or INVALID state
		 * */
		if ( DOOR_STATE_CLOSED == door_get_door_state(layer) ||
			 DOOR_STATE_INVALID == door_get_door_state(layer) )
		{
			rc = 0;
			break;
		}
	}

	/**
	 * IRQ triggered or semaphore take error, just like door_open,
	 * do some nessesary clean up
	 * */

	/* Stop motor */
	door_stop_write_gpio(layer);

	/* Disable close in position irq */
	door_close_in_position_irq_disable(layer);

	/* Disable on door infrared irq */
	door_infrared_irq_disable(layer);

	/**
	 * At this point, we are fully stop the door.
	 * Check infrared first, then check the door status.
	 * */

	/* Wait IRQ or Polling timeout */
	if ( i >= CONFIG_APP_DOOR_CLOSE_TIMEOUT_IN_SEC * 10 )
	{
		rc = -1;
		goto out;
	}

	/* Infrared detected */
	if ( door_on_door_infrared_detected_flag[layer - 1] )
	{
		/* Reset flag, for next trigger */
		door_on_door_infrared_detected_flag[layer - 1] = false;

		/**
		 * Mark as error, k_sem_take will return negtive number -11 and -16,
		 * use positive number 1 should be distingsuish them
		 * */
		rc = 1;
		goto out;
	}

	/* Wait GPIO flush its status */
	k_sleep(10);

	/* Check door state twice before return */
	door_state = door_get_door_state(layer);
	if ( DOOR_STATE_OPENED == door_state )
	{
		/* Door never closed */
		rc = -2;
	}
	else if ( DOOR_STATE_HALF == door_state )
	{
		/* Door closed, but not fully closed */
		rc = -3;
	}
	else if ( DOOR_STATE_INVALID == door_state )
	{
		/* Both state of opened and closed, impossible */
		rc = -4;
	}

out:
	if ( 0 == rc )
	{
		/* Door close ok */
		door_status[layer - 1] = true;
		SYS_LOG_DBG("close ok at layer %d", layer);
	}
	else if ( 1 == rc )
	{
		/* On door infrared detected */
		SYS_LOG_DBG("On door infrared detected");
	}
	else if ( rc < 0 )
	{
		/* Door close error */
		door_status[layer - 1] = false;
		SYS_LOG_ERR("close error at layer %d, return value of %d", layer, rc);
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
	int *			layer	= (int *)arg1;
	struct k_sem *	sem		= (struct k_sem *)arg2;
	int *			rc		= (int *)arg3;

	*rc = door_open(*layer);
	/* TODO Avoid hand pitch */

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
	int *			layer	= (int *)arg1;
	struct k_sem *	sem		= (struct k_sem *)arg2;
	int *			rc		= (int *)arg3;

	*rc = door_close(*layer);
	/* TODO Avoid hand pitch */

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

	int layer[4] = {1, 2, 3, 4};
	int thread_rc[4] = {0xFC, 0xFC, 0xFC, 0xFC};
	struct k_sem thread_sem[4];

	int i, rc = 0;

	memset(door_init_thread, 0x00, sizeof(struct k_thread) * 4);

	/* Start to init doors */
	for ( i = 0; i < 4; ++i )
	{
		k_sem_init(&thread_sem[i], 0, 1);

		k_thread_create( &door_init_thread[i],
			door_comm_thread_stack[i],
			K_THREAD_STACK_SIZEOF(door_comm_thread_stack[i]),
			door_close_thread_entry_point,
			(void *)&layer[i], (void *)&thread_sem[i], (void*)&thread_rc[i],
			0, 0, K_NO_WAIT );
			/* Prio: 0, Flag: 0, Delay: No delay */
	}

	/* Wait all four initial thread return */
	for ( i = 0; i < 4; ++i )
	{
		rc = k_sem_take(&thread_sem[i], K_FOREVER);
		if ( 0 != rc )
		{
			/* Timeout or error happened */
			SYS_LOG_ERR("Sem take error at layer %d, return %d", i + 1, rc);
			return -1;
		}
	}

	rc = (thread_rc[3] << 24) |
		 (thread_rc[2] << 16) |
		 (thread_rc[1] << 8) |
		 (thread_rc[0]);

	/* Check initial return value */
	for ( i = 0; i < 4; ++i )
	{
		if ( 0 != thread_rc[i] )
		{
			SYS_LOG_ERR("Close error at layer %d, return: %d", i + 1, thread_rc[i]);
		}
	}

	if ( 0 != rc )
	{
		SYS_LOG_ERR("Door close error, rc = %d", rc);
	}

	return rc;
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

	int layer[4] = {1, 2, 3, 4};
	int thread_rc[4] = {0xFC, 0xFC, 0xFC, 0xFC};
	struct k_sem thread_sem[4];

	int i, rc = 0;

	memset(door_open_thread, 0x00, sizeof(struct k_thread) * 4);

	/* Start to open doors */
	for ( i = 0; i < 4; ++i )
	{
		k_sem_init(&thread_sem[i], 0, 1);

		k_thread_create( &door_open_thread[i],
			door_comm_thread_stack[i],
			K_THREAD_STACK_SIZEOF(door_comm_thread_stack[i]),
			door_open_thread_entry_point,
			(void *)&layer[i], (void *)&thread_sem[i], (void*)&thread_rc[i],
			0, 0, K_NO_WAIT );
			/* Prio: 0, Flag: 0, Delay: No delay */
	}

	/* Wait door to opened */
	for ( i = 0; i < 4; ++i )
	{
		rc = k_sem_take(&thread_sem[i], K_FOREVER);
		if ( 0 != rc )
		{
			SYS_LOG_ERR("Sem take error at layer %d, return %d", i + 1, rc);
			/**
			 * XXX TODO FIXME  No return !? May cause the one of the four
			 * running thread access into released stack when this function
			 * is returned. */
			return -1;
		}
	}

	rc = (-thread_rc[3] << 24) |
		 (-thread_rc[2] << 16) |
		 (-thread_rc[1] << 8) |
		 (-thread_rc[0]);

	/* Check initial return value */
	for ( i = 0; i < 4; ++i )
	{
		if ( 0 != thread_rc[i] )
		{
			SYS_LOG_ERR("Open error at layer %d, return: %d", i + 1, thread_rc[i]);
		}
	}

	if ( 0 != rc )
	{
		SYS_LOG_ERR("Door initial error, rc = %d", rc);
	}

	return rc;
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
	door_irq_sem_init();
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

