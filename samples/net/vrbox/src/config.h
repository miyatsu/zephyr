/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef CONFIG_NET_APP_SETTINGS
#ifdef CONFIG_NET_IPV6
#define ZEPHYR_ADDR		CONFIG_NET_APP_MY_IPV6_ADDR
#define SERVER_ADDR		CONFIG_NET_APP_PEER_IPV6_ADDR
#else
#define ZEPHYR_ADDR		CONFIG_NET_APP_MY_IPV4_ADDR
#define SERVER_ADDR		CONFIG_NET_APP_PEER_IPV4_ADDR
#endif
#else
#ifdef CONFIG_NET_IPV6
#define ZEPHYR_ADDR		"2001:db8::1"
#define SERVER_ADDR		"2001:db8::2"
#else
#define ZEPHYR_ADDR		"172.16.0.4"
#define SERVER_ADDR		"172.16.0.1"
#endif
#endif


#define SERVER_PORT		1883


#define APP_SLEEP_MSECS		500
#define APP_TX_RX_TIMEOUT       300
#define APP_NET_INIT_TIMEOUT    10000




/* Factory test config */
#undef CONFIG_APP_FACTORY_TEST
#ifdef CONFIG_APP_FACTORY_TEST
#define CONFIG_APP_AXLE_FACTORY_TEST
#define CONFIG_APP_DOOR_FACTORY_TEST
#endif /* CONFIG_APP_FACTORY_TEST */

/* Axle config */
//#define CONFIG_APP_AXLE_DEBUG
#define CONFIG_APP_AXLE_PWM_DEV_NAME			CONFIG_PWM_STM32_2_DEV_NAME
#define CONFIG_APP_AXLE_PWM_OUTPUT_CHANNEL		2
#define CONFIG_APP_AXLE_PWM_PERIOD				5000
#define CONFIG_APP_AXLE_ROTATE_TIMEOUT_IN_SEC	10

/* Door config */
//#define CONFIG_APP_DOOR_DEBUG
#define CONFIG_APP_DOOR_OPEN_TIMEOUT_IN_SEC		5
#define CONFIG_APP_DOOR_CLOSE_TIMEOUT_IN_SEC	5




#define MQTT_CLIENTID			"dev/rtos"
#define MQTT_SUBSCRIBE_TOPIC	"dev/rtos"
#define MQTT_PUBLISH_TOPIC		"srv/controller"

/* Set the following to 1 to enable the Bluemix topic format */
#define APP_BLUEMIX_TOPIC	0

/* These are the parameters for the Bluemix topic format */
#if APP_BLUEMIX_TOPIC
#define BLUEMIX_DEVTYPE		"sensor"
#define BLUEMIX_DEVID		"carbon"
#define BLUEMIX_EVENT		"status"
#define BLUEMIX_FORMAT		"json"
#endif

#endif

