#include <stdint.h>
#include <stdbool.h>

#include <kernel.h>
#include <gpio.h>

#ifdef CONFIG_APP_HEADSET_DEBUG
#include <misc/printk.h>
#endif /* CONFIG_APP_HEADSET_DEBUG */

#include "config.h"
#include "gpio_comm.h"

static struct gpio_group_pin_t headset_gpio_table[] =
{
	/* Dial motor, dial position detector, infrared detector, handspike push pull */
	{GPIO_GROUP_B, 4}, {GPIO_GROUP_B, 5}, {GPIO_GROUP_B, 6}, {GPIO_GROUP_B, 7}
};

/* Stock of headset, -1 means some mechanical error happened */
static int8_t headset_stock = 0;

/**
 * @brief Get the headset stock
 *
 * @return -1, Some error happened
 *			0, Out of stock
 *		   >0, Every thing is ok
 * */
int8_t headset_get_stock(void)
{
	return headset_stock;
}

/**
 * @brief Set the headset dial to rotate or stop rotate
 *
 * @param enable 1 Enable rotate
 *				 0 Disable rotate
 * */
static void headset_dial_rotate_enable_disable(uint8_t enable)
{
	gpio_comm_write(&headset_gpio_table[0], !enable);
}

/**
 * @brief Check if the headset dial is at pushable position
 *
 * With rotating of headset dial, there is some gap period of
 * time. In this period of time, the handspike may not straight
 * point to the headset box. This function is to check the
 * dial is rotate to the correct position which the handspike
 * is straight point to the headset box.
 *
 * @return true, Dial is in position
 *		   false, Dial is not in position
 * */
static bool headset_is_dial_in_position(void)
{
	uint32_t value;
	gpio_comm_read(&headset_gpio_table[1], &value);
	return !value;
}

/**
 * @brief Check if there is headset can be push out at the port or not
 *
 * @return true, Headset in position, can push it out
 *		   false, No headset can be push out
 * */
static bool headset_is_headset_in_position(void)
{
	uint32_t value;
	gpio_comm_read(&headset_gpio_table[2], &value);
	return !value;
}

/**
 * @brief Set the handspike to push or pull
 *
 * @param push 1 Push the headset out
 *			   0 Pull back the handspike
 * */
static void headset_handspike_push_pull(uint8_t push)
{
	gpio_comm_write(&headset_gpio_table[3], !push);
}

/**
 * @brief Callback function when headset dial rotate to certain position
 *
 * @param dev Gpio device pointer
 *
 * @param gpio_cb Gpio callback structure
 *
 * @param pins Mask of pins that trigger the callback
 * */
static void headset_dial_in_position_irq_cb(struct device *dev,
										struct gpio_callback *gpio_cb,
										uint32_t pins)
{
	static int i = 0;
	int status;
	k_sleep(10);
	gpio_comm_read(&headset_gpio_table[1], &status);
	status = status ? 1 : 0;
	printk("[%s] called, i = %d, status = %d\n", __func__, i++, status);
	headset_handspike_push_pull(status);
	return ;
	uint32_t pin = 0;

	if ( !headset_is_headset_in_position() )
	{
		/* No headset at current position, continue to rotate */
		return ;
	}

	/**
	 * At this point, the dial is rotate just in position,
	 * and we have a headset detected at the current position
	 * that can be sell.
	 *
	 * Stop the rotate, and to let the polling side to do
	 * sell operation.
	 * */

	/* Parse pin mask to pin number */
	while ( 1 != pins )
	{
		pins >>= 1;
		pin++;
	}

	/* Disable gpio interrupt */
	gpio_pin_disable_callback(dev, pin);

	/* Stop rotate */
	headset_dial_rotate_enable_disable(0);
}

/**
 * @brief Enable dial position's gpio interrupt
 * */
static void headset_dial_in_position_irq_enable(void)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table[headset_gpio_table[1].gpio_group]);
	gpio_pin_enable_callback(dev, headset_gpio_table[1].gpio_pin);
}

/**
 * @brief Disable dial position's gpio interrupt
 * */
static void headset_dial_in_position_irq_disable(void)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table[headset_gpio_table[1].gpio_group]);
	gpio_pin_disable_callback(dev, headset_gpio_table[1].gpio_pin);
}

/**
 * @brief Initial headset dial in position detector interrupt
 *
 * This function will be called by headset_init()
 * */
static void headset_dial_in_position_irq_init(void)
{
	/**
	 * "struct gpio_callback" CAN NOT alloc on heap memory
	 * so we declare it as static
	 * */
	static struct gpio_callback gpio_cb;

	/* Set in position gpio pin as interrupt mode */
	gpio_comm_conf(&headset_gpio_table[1], GPIO_DIR_IN | GPIO_INT | GPIO_INT_DEBOUNCE | GPIO_PUD_PULL_UP | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW);

	struct device *dev = device_get_binding(gpio_group_dev_name_table[headset_gpio_table[1].gpio_group]);

	/* Initial gpio callback */
	gpio_init_callback(&gpio_cb, headset_dial_in_position_irq_cb, BIT(headset_gpio_table[1].gpio_pin));

	/* Add this current dev into callback strucure */
	gpio_add_callback(dev, &gpio_cb);
}

/**
 * @brief Buy a headset
 *
 * @return  0 Buy success
 *		   -1 Some error happened
 * */
int8_t headset_buy(void)
{
	uint16_t i;
	uint16_t rotate_wait_time_in_sec =
				CONFIG_APP_HEADSET_ROTATE_TIMEOUT_IN_SEC;

	uint8_t handspike_push_wait_time_in_sec =
				CONFIG_APP_HEADSET_HANDSPIKE_PUSH_WAIT_TIME_IN_SEC;
	uint8_t handspike_pull_wait_time_in_sec =
				CONFIG_APP_HEADSET_HANDSPIKE_PULL_WAIT_TIME_IN_SEC;

	if ( headset_stock <= 0 )
	{
		/**
		 * Out of stock or hardware error
		 * This state can not sell
		 * */
		goto error;
	}

	/* Enable dial rotate */
	headset_dial_rotate_enable_disable(1);

	/* Enable in position gpio irq */
	headset_dial_in_position_irq_enable();

	/* Polling gpio status */
	for ( i = 0; i < 60 * rotate_wait_time_in_sec * 10; ++i )
	{
		/**
		 * Dial in position detected AND
		 * Current position have a headset can be sell
		 * */
		if ( headset_is_dial_in_position() && headset_is_headset_in_position())
		{
			break;
		}

		/**
		 * Dial not in position or no headset can be sell at current position
		 * Wait for more 100ms
		 * */
		k_sleep(100);
	}

	/* Disable irq just in case of free trigger */
	headset_dial_in_position_irq_disable();

	/* Stop rotate */
	headset_dial_rotate_enable_disable(0);

	/* Check for timeout */
	if ( i >=  60 * rotate_wait_time_in_sec * 10 )
	{
		goto error;
	}
	else
	{
		goto push;
	}

/**
 * Headset detected, and at the hendspike straight ahead position.
 * Start to push the headset out.
 * */
push:
	/* Stop rotate */
	headset_dial_rotate_enable_disable(0);

	/* Push out headset */
	headset_handspike_push_pull(1);

	/* Wait until its fully push out */
	k_sleep(handspike_push_wait_time_in_sec * 1000);

	/* Pull back handspike */
	headset_handspike_push_pull(0);

	/* Wait until handspike fully pulled back */
	k_sleep(handspike_pull_wait_time_in_sec * 1000);

	/* Check the headset realy pushed out */
	if ( headset_is_headset_in_position() )
	{
		goto error;
	}

	/* Every thing fine */
	headset_stock--;
	return 0;

error:
	/* Some error happened */
	headset_stock = -1;
	return -1;
}


int8_t headset_stock_init(void)
{
	uint16_t i, j;
	uint16_t rotate_wait_time_in_sec =
				CONFIG_APP_HEADSET_ROTATE_TIMEOUT_IN_SEC;

	headset_dial_rotate_enable_disable(1);
	for ( i = 0; i < 60; ++i )
	{
		for ( j = 0; j < rotate_wait_time_in_sec * 10; ++j )
		{
			if ( headset_is_dial_in_position() )
			{
				/* Dial in potition detected */
				break;
			}
			/* Dial not in position, wait for more 100ms */
			k_sleep(100);
		}

		if ( j >= rotate_wait_time_in_sec * 10 )
		{
			/* Rotate dial in position timeout */
			headset_stock = -1;
			return -1;
		}

		/* Now the dial is in position */
		if ( headset_is_headset_in_position() )
		{
			/* Box not empty */
			headset_stock++;
		}
	}

	/* Dial rotate one lap, stock counting done. */
	return 0;
}

static void headset_gpio_init(void)
{
	uint32_t temp;
	uint8_t i;

	gpio_comm_conf(&headset_gpio_table[0], GPIO_DIR_OUT | GPIO_PUD_PULL_UP);
	gpio_comm_conf(&headset_gpio_table[2], GPIO_DIR_IN  | GPIO_PUD_PULL_UP);
	gpio_comm_conf(&headset_gpio_table[3], GPIO_DIR_OUT | GPIO_PUD_PULL_UP);

	for ( i = 0; i < 4; ++i )
	{
		gpio_comm_read(&headset_gpio_table[i], &temp);
	}
}

int8_t headset_init(void)
{
	headset_dial_in_position_irq_init();
	headset_gpio_init();
	headset_stock_init();
	return 0;
}

#ifdef CONFIG_APP_HEADSET_FACTORY_TEST

int8_t headset_factory_test(void)
{
	return 0;
}

#endif /* CONFIG_APP_HEADSET_FACTORY_TEST */

#ifdef CONFIG_APP_HEADSET_DEBUG

void headset_debug(void)
{
	headset_dial_in_position_irq_init();
	headset_gpio_init();
	headset_dial_in_position_irq_enable();
	headset_handspike_push_pull(0);
	while ( 1 )
	{
		k_sleep(1000);
		/*
		printk("Start to push...\n");
		k_sleep(1000);
		headset_handspike_push_pull(1);
		printk("Start to pull back...\n");
		k_sleep(1000);
		headset_handspike_push_pull(0);*/
	}
	while (1)
	{
		headset_axle_rotate_enable();
		while ( headset_is_in_position() )
		{
			headset_axle_rotate_disable();
			k_sleep(2000);
		}
	}
	return ;
}

#endif /* CONFIG_APP_HEADSET_DEBUG */

