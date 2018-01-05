/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @fiel headset.c
 *
 * @brief Headset sell driver
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 11:45:56 November 10, 2017 GTM+8
 *
 * */
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include <kernel.h>
#include <gpio.h>

#include "config.h"
#include "gpio_comm.h"

#define SYS_LOG_DOMAIN "headset"
#ifdef CONFIG_APP_HEADSET_DEBUG
	#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#else
	#define SYS_LOG_LEVEL SYS_LOG_LEVEL_ERR
#endif /* CONFIG_APP_HEADSET_DEBUG */
#include <logging/sys_log.h>

static struct gpio_group_pin_t headset_gpio_table[] =
{
	/* Dial motor, dial position detector, infrared detector, handspike push pull */
	{GPIO_GROUP_B, 4}, {GPIO_GROUP_B, 5}, {GPIO_GROUP_B, 6}, {GPIO_GROUP_B, 7}
};

/**
 * Stock of headset, -1 means some mechanical error happened
 * -2 means the headset is not initialized
 * */
static int8_t headset_stock = -2;

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
	uint32_t value = 0;
	gpio_comm_read(&headset_gpio_table[1], &value);
	return !value;
}

/**
 * @brief Check if there is headset can be push out at the port or not
 *
 * @return true, Headset in position, can push it out
 *		   false, No headset can not be push out
 * */
static bool headset_is_headset_in_position(void)
{
	uint32_t value = 0;
	gpio_comm_read(&headset_gpio_table[2], &value);
	return value;
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
 * Due to asynchronous execution on main thread and dial in position irq,
 * we use a semaphore to synchronous two thread in order to execute next
 * instruction in main thread.
 * */
static struct k_sem headset_dial_in_position_sem;

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
	uint32_t pin = 0;

	/* Trigged at rising edge only, ignore trigged at falling edge */
	if ( !headset_is_dial_in_position() )
	{
		return ;
	}

	SYS_LOG_DBG("IRQ trigged");

	/**
	 * At this point, the dial is rotate just in position,
	 * and we have to check if there is a headset at the
	 * current position that can be sell.
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

	/* Mark current dial is in/pass the certain position */
	k_sem_give(&headset_dial_in_position_sem);
}

/**
 * @brief Enable dial position's gpio interrupt
 * */
static void headset_dial_in_position_irq_enable(void)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table	\
			[headset_gpio_table[1].gpio_group]);
	gpio_pin_enable_callback(dev, headset_gpio_table[1].gpio_pin);
}

/**
 * @brief Disable dial position's gpio interrupt
 * */
static void headset_dial_in_position_irq_disable(void)
{
	struct device *dev = device_get_binding(gpio_group_dev_name_table	\
			[headset_gpio_table[1].gpio_group]);
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
	gpio_comm_conf(&headset_gpio_table[1], GPIO_DIR_IN | GPIO_INT |	\
			GPIO_INT_DEBOUNCE | GPIO_PUD_PULL_UP | GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW);

	struct device *dev = device_get_binding(gpio_group_dev_name_table	\
			[headset_gpio_table[1].gpio_group]);

	/* Initial gpio callback */
	gpio_init_callback(&gpio_cb, headset_dial_in_position_irq_cb,
			BIT(headset_gpio_table[1].gpio_pin));

	/* Add this current dev into callback strucure */
	gpio_add_callback(dev, &gpio_cb);
}

/**
 * @brief This function is to move the dial to none certain position
 *
 * Which make the irq can not be trigged at condition of ACTIVE_HIGH,
 * because we are never reach a rising edge after call this function.
 *
 * @return 0, move ok
 *		  -1, some error happened
 * */
static int headset_move_dial_off_grid(void)
{
	int i, rc = 0;
	/* Disable irq to privent off position callback */
	headset_dial_in_position_irq_disable();

	/* Start to rotate */
	headset_dial_rotate_enable_disable(1);

	SYS_LOG_DBG("Start to polling...\n");
	/* Polling read gpio */
	for ( i = 0; i < CONFIG_APP_HEADSET_ROTATE_TIMEOUT_IN_SEC * 10; ++i )
	{
		/* Dial not in position */
		if ( !headset_is_dial_in_position() )
		{
			SYS_LOG_DBG("Off grid detected!\n");
			break;
		}

		/* Still in position, wait more 100ms */
		k_sleep(100);
	}

	/* Stop rotate */
	headset_dial_rotate_enable_disable(0);

	/* Check for timeout */
	if ( i >= CONFIG_APP_HEADSET_ROTATE_TIMEOUT_IN_SEC * 10 )
	{
		/* Wait timeout */
		headset_stock = -1;
		SYS_LOG_ERR("Rotate timedout");
		rc = -ETIMEDOUT;
	}

	return rc;
}

/**
 * @brief Buy a headset
 *
 * @return  0 Buy success
 *		   <0 Some error happened
 * */
int8_t headset_buy(void)
{
	uint16_t i;
	int rc = 0;

	if ( headset_stock <= 0 )
	{
		/**
		 * Out of stock or hardware error
		 * This state can not sell
		 * */
		rc = -1;
		SYS_LOG_ERR("headset_stock = %d", headset_stock);
		goto error;
	}

	rc = headset_move_dial_off_grid();
	if ( 0 != rc )
	{
		headset_stock = -1;
		SYS_LOG_ERR("Error at line: %d", __LINE__);
		goto error;
	}

	/**
	 * Now the dial is not in position, we can enable the irq
	 * */


	/* Polling gpio status */
	for ( i = 0; i < 60; ++i )
	{
		/* Enable dial rotate */
		headset_dial_rotate_enable_disable(1);

		/* Enable in position gpio irq */
		headset_dial_in_position_irq_enable();
		k_sleep(50);

		rc = k_sem_take(&headset_dial_in_position_sem,
				CONFIG_APP_HEADSET_ROTATE_TIMEOUT_IN_SEC * 1000);
		if ( 0 != rc )
		{
			headset_stock = -1;
			SYS_LOG_ERR("Timedout at line: %d", __LINE__);
			goto error;
		}

		/* IRQ trigged */

		/* Check if there is a headset in position */
		if ( headset_is_headset_in_position() )
		{
			/* Stop rotate */
			headset_dial_rotate_enable_disable(0);

			/* Wait the dial fully stoped */
			k_sleep(1000);

			goto push;
		}
 
		/**
		 * No headset in current position,
		 * wait the dial rotate out of position
		 * */
		rc = headset_move_dial_off_grid();
		if ( 0 != rc )
		{
			headset_stock = -1;
			SYS_LOG_ERR("Error at line: %d", __LINE__);
			goto error;
		}
	}

	/**
	 * All 60 position are empty, this will never happend.
	 *
	 * Set stock to zero, and return;
	 * */
	headset_dial_in_position_irq_disable();

	headset_dial_rotate_enable_disable(0);

	headset_stock = 0;

	SYS_LOG_DBG("Headset sold out");

	return -1;

/**
 * Headset detected, and at the hendspike straight ahead position.
 * Start to push the headset out.
 * */
push:
	/* Disable irq */
	headset_dial_in_position_irq_disable();

	/* Stop rotate */
	headset_dial_rotate_enable_disable(0);

	/* Push out headset */
	headset_handspike_push_pull(1);

	/* Wait until its fully push out */
	k_sleep(200);

	/* Pull back handspike */
	headset_handspike_push_pull(0);

	/* Wait until handspike fully pulled back */
	k_sleep(1000);

	/* Check the headset realy pushed out */
	if ( headset_is_headset_in_position() )
	{
		/**
		 * Note: Once the headset is not sell out, we can NOT determine
		 * the machine is broken. May be the accuracy is not enough on
		 * this position, we can make an another push by clould.
		 * */
		rc = -1;
		SYS_LOG_ERR("Handspike error");
		return rc;
	}

	/* Every thing is fine */
	SYS_LOG_DBG("OK");
	headset_stock--;
	return 0;

error:
	/* Some error happened, timedout for most cases */
	headset_stock = -1;

	return rc;
}

/**
 * @brief Add headset into box
 *
 * In ID(Instructure Design) phase, we set a manual opened door at the back
 * of the box. When administrator want to add more headset into this box,
 * we need move three or four positoin to make the dial dead against this
 * manual door so the administrator can insert three or four headset into
 * boxes.
 *
 * @return 0, rotate ok
 *		  <0, some error happened
 * */
int headset_add(void)
{
	int i, rc = 0;
	rc = headset_move_dial_off_grid();
	if ( 0 != rc )
	{
		SYS_LOG_ERR("Error at line: %d", __LINE__);
		headset_stock = -1;
		goto out;
	}

	for ( i = 0; i < 3; ++i )
	{
		/* Enable irq */
		headset_dial_in_position_irq_enable();

		/* Start to rotate */
		headset_dial_rotate_enable_disable(1);
		k_sleep(50);

		/* Wait irq been trigged */
		rc = k_sem_take(&headset_dial_in_position_sem, 
				CONFIG_APP_HEADSET_ROTATE_TIMEOUT_IN_SEC * 1000);
		if ( 0 != rc )
		{
			SYS_LOG_DBG("k_sem_take error, rc = %d", rc);
			headset_stock = -1;
			goto out;
		}

		/* IRQ trigged */

		/* Do nothing */

		/* Move off grid, to avoid FALLING EDGE trigge of irq */
		rc = headset_move_dial_off_grid();
		if ( 0 != rc )
		{
			headset_stock = -1;
			SYS_LOG_DBG("Error at line: %d", __LINE__);
			goto out;
		}
	}
out:
	return rc;
}

/**
 * @brief Read the number of the headset
 *
 * After power up, the box did not know how many headset is in it, so
 * we must initial it by polling all possibile position and check if
 * it have a headset at that position.
 *
 * @return 0, initial ok
 *		  <0, some error happened
 * */
int8_t headset_stock_init(void)
{
	uint16_t i;
	int rc = 0;

	/* Check dial position */
	if ( headset_is_dial_in_position() )
	{
		/**
		 * Current dial is in position,
		 * to privent unnessary irq, move the dial off certain position
		 * */
		rc = headset_move_dial_off_grid();
		if ( 0 != rc )
		{
			headset_stock = -1;
			goto out;
		} 
	}

	/* Now, the dial is not in position, start to count */
	SYS_LOG_DBG("Start to counting...\n");
	for ( i = 0; i < 60; ++i )
	{
		/* Enable irq */
		headset_dial_in_position_irq_enable();

		/* Start to rotate */
		headset_dial_rotate_enable_disable(1);
		k_sleep(50);

		/* Wait irq been trigged */
		rc = k_sem_take(&headset_dial_in_position_sem,
				CONFIG_APP_HEADSET_ROTATE_TIMEOUT_IN_SEC * 1000);
		if ( 0 != rc )
		{
			SYS_LOG_DBG("k_sem_take error, rc = %d", rc);
			headset_stock = -1;
			goto out;
		}

		/* IRQ trigged, check if there is a headset in position */
		if ( headset_is_headset_in_position() )
		{
			SYS_LOG_DBG("count++\n");
			headset_stock++;
		}

		rc = headset_move_dial_off_grid();
		if ( 0 != rc )
		{
			headset_stock = -1;
			SYS_LOG_ERR("Error at line: %d", __LINE__);
			goto out;
		}
	}
out:
	/* Disable irq */
	headset_dial_in_position_irq_disable();

	/* Stop rotate dial */
	headset_dial_rotate_enable_disable(0);

	/* stock counting done. */
	SYS_LOG_DBG("Init done, count: %d\n", headset_stock);
	return rc;
}

/**
 * @brief Initial all GPIOs that the headset may needed
 * */
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

	gpio_comm_write(&headset_gpio_table[0], 1);
	gpio_comm_write(&headset_gpio_table[3], 1);
}

/**
 * @brief Initial wrapper function of headset
 *
 * @return Always return 0
 * */
int8_t headset_init(void)
{
	headset_dial_in_position_irq_init();
	headset_gpio_init();

	k_sem_init(&headset_dial_in_position_sem, 0, 1);

	return 0;
}

#ifdef CONFIG_APP_HEADSET_FACTORY_TEST

int headset_ft_rotate(void)
{
	headset_dial_rotate_enable_disable(1);
	return 0;
}

int headset_ft_stop(void)
{
	headset_dial_rotate_enable_disable(0);
	return 0;
}

int headset_ft_push(void)
{
	headset_handspike_push_pull(1);
	k_sleep(200);
	headset_handspike_push_pull(0);
	return 0;
}

int headset_ft_infrared(void)
{
	return headset_is_headset_in_position() ? 1 : 0;
}

int headset_ft_accuracy(void)
{
	int i, rc = 0;

	/* Make the headset default in output window */
	gpio_comm_conf(&headset_gpio_table[2], GPIO_DIR_OUT | GPIO_PUD_PULL_DOWN);
	gpio_comm_write(&headset_gpio_table[2], 0);

	for ( i = 0; i < 60; ++i )
	{
		rc = headset_buy();
		if ( 0 != rc )
		{
			break;
		}
	}

	/* Re-initial infrared detector pin */
	gpio_comm_conf(&headset_gpio_table[2], GPIO_DIR_IN | GPIO_PUD_PULL_UP);
	gpio_comm_write(&headset_gpio_table[2], 1);

	return rc;
}

#endif /* CONFIG_APP_HEADSET_FACTORY_TEST */

#ifdef CONFIG_APP_HEADSET_DEBUG

void headset_debug(void)
{
	SYS_LOG_DBG("Start to run debug...");
	headset_init();
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
		/*
		headset_axle_rotate_enable(1);
		while ( headset_is_in_position() )
		{
			headset_axle_rotate_disable();
			k_sleep(2000);
		}
		*/
	}
	return ;
}

#endif /* CONFIG_APP_HEADSET_DEBUG */

