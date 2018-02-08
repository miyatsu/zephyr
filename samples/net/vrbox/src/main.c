/**
 * Copyright (c) 2017-2018 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file main.c
 *
 * @brief Application code entry point
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 17:48:39 January 5, 2018 GTM+8
 *
 * */
#include <stdint.h>

#include <kernel.h>
#include <misc/__assert.h>
#include <watchdog.h>

#include "config.h"
#include "mqtt.h"
#include "dfu_http.h"
#include "log_hook.h"
#include "axle.h"
#include "door.h"
#include "infrared.h"
#include "headset.h"

#define SYS_LOG_DOMAIN	"main"
#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>

int hardware_init(void)
{
	int rc = 0;

	rc = axle_init();
	if ( 0 != rc)
	{
		SYS_LOG_ERR("axle_init error");
	}

	rc = door_init();
	if ( 0 != rc )
	{
		SYS_LOG_ERR("door_init error");
	}

	rc = infrared_init();
	if ( 0 != rc )
	{
		SYS_LOG_ERR("infrared_init error");
	}

	rc = headset_init();
	if ( 0 != rc )
	{
		SYS_LOG_ERR("headset_init error");
	}

	return rc;
}

#ifdef CONFIG_WATCHDOG

void wdt_entry_point(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	struct device *dev = device_get_binding("IWDG");
	wdt_enable(dev);
	while (1)
	{
		wdt_reload(dev);
		k_sleep(100);
	}
}

void wdt_init(void)
{
	static K_THREAD_STACK_DEFINE(wdt_stack_area, 128);
	static struct k_thread wdt_thread_date;

	k_thread_create(&wdt_thread_date, wdt_stack_area,
					K_THREAD_STACK_SIZEOF(wdt_stack_area),
					wdt_entry_point,
					NULL, NULL, NULL,
					0, 0, K_NO_WAIT);
}

#endif /* CONFIG_WATCHDOG */

int main(void)
{
	printk("System boot success!\n");
#ifdef CONFIG_WATCHDOG
	wdt_init();
#endif /* CONFIG_WATCHDOG */
	net_mqtt_init();

#ifdef CONFIG_SYS_LOG_EXT_HOOK
	app_log_hook_init();
#endif /* CONFIG_SYS_LOG_EXT_HOOK */

#ifdef CONFIG_IMG_MANAGER
	dfu_init();
#endif /* CONFIG_IMG_MANAGER */

	hardware_init();

	while ( 1 )
	{
		k_sleep(1000);
	}
}

#ifdef CONFIG_APP_MAIN_DEBUG


#endif /* CONFIG_APP_MAIN_DEBUG */

