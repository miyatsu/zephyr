/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "version.h"

/* DFU config */
/*****************************************************/
//#define CONFIG_APP_DFU_HTTP_DEBUG

#define CONFIG_APP_DFU_HTTP_CONNECTION_TIMEOUT_IN_SEC	20
#define CONFIG_APP_DFU_HTTP_RX_TIMEOUT_IN_SEC			5
#define CONFIG_APP_DFU_HTTP_TX_TIMEOUT_IN_SEC			5

#define CONFIG_APP_DFU_HTTP_RX_BUFF_SIZE				1024
/*****************************************************/

/* LOG config */
//#define CONFIG_APP_LOG_HOOK_DEBUG
#define CONFIG_APP_LOG_HOOK_DISPATCH_THREAD_STACK_SIZE	2048
#define CONFIG_APP_LOG_HOOK_LOG_FILE_NAME				"/log.log"

/* Factory test config */
#define CONFIG_APP_FACTORY_TEST

#ifdef CONFIG_APP_FACTORY_TEST
#define CONFIG_APP_AXLE_FACTORY_TEST
#define CONFIG_APP_DOOR_FACTORY_TEST
#define CONFIG_APP_INFRARED_FACTORY_TEST
#define CONFIG_APP_HEADSET_FACTORY_TEST
#endif /* CONFIG_APP_FACTORY_TEST */

/* Axle config */
//#define CONFIG_APP_AXLE_DEBUG
#define CONFIG_APP_AXLE_PWM_DEV_NAME			CONFIG_PWM_STM32_2_DEV_NAME
#define CONFIG_APP_AXLE_PWM_OUTPUT_CHANNEL		2
#define CONFIG_APP_AXLE_PWM_PERIOD				5000
#define CONFIG_APP_AXLE_ROTATE_TIMEOUT_IN_SEC	10

/* Door config */
//#define CONFIG_APP_DOOR_DEBUG
#define CONFIG_APP_DOOR_INIT_THREAD_STACK_SIZE	1024
#define CONFIG_APP_DOOR_INIT_TIMEOUT_IN_SEC		CONFIG_APP_DOOR_CLOSE_TIMEOUT_IN_SEC
#define CONFIG_APP_DOOR_OPEN_TIMEOUT_IN_SEC		30
#define CONFIG_APP_DOOR_CLOSE_TIMEOUT_IN_SEC	30

/* Infrared debug */
//#define CONFIG_APP_INFRARED_DEBUG

/* Headset config */
//#define CONFIG_APP_HEADSET_DEBUG
#define CONFIG_APP_HEADSET_ROTATE_TIMEOUT_IN_SEC			5
#define CONFIG_APP_HEADSET_HANDSPIKE_PUSH_WAIT_TIME_IN_SEC	2
#define CONFIG_APP_HEADSET_HANDSPIKE_PULL_WAIT_TIME_IN_SEC	2
#define CONFIG_APP_HEADSET_CELL_DIFF						3

/* Service config */
//#define CONFIG_APP_SERVICE_DEBUG

/* Main function config */
//#define CONFIG_APP_MAIN_DEBUG

/* MQTT config */
/*****************************************************/
//#define CONFIG_APP_MQTT_DEBUG
#define CONFIG_APP_MQTT_INIT_TIMEOUT	( 10 * 1000 )
#define CONFIG_APP_MQTT_TIMEOUT			( 10 * 1000 )

#define CONFIG_APP_MQTT_DISPATCH_THREAD_STACK_SIZE		4096

#define CONFIG_APP_MQTT_SERVER_ADDR		"172.16.0.1"
#define CONFIG_APP_MQTT_SERVER_PORT		1883
#define CONFIG_APP_MQTT_CLIENT_ID		"dev/rtos"
#define CONFIG_APP_MQTT_SUBSCRIBE_TOPIC	"dev/rtos"
#define CONFIG_APP_MQTT_PUBLISH_TOPIC	"srv/controller"

#define CONFIG_APP_MQTT_CONNECT_RETRY_TIMES	5
#define CONFIG_APP_MQTT_SEND_RETRY_TIMES	5

#define CONFIG_APP_MQTT_PING_STACK_SIZE		512

#define MQTT_CLIENTID			"dev/rtos"
#define MQTT_SUBSCRIBE_TOPIC	"dev/rtos"
#define MQTT_PUBLISH_TOPIC		"srv/controller"
/*****************************************************/

#endif /* CONFIG_H */

