/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @fiel mqtt.c
 *
 * @brief Communicate to x86 within the box via MQTT
 *
 * This file is a wrapper function of MQTT, include reconnect to
 * network and provide simple API to upper layers.
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 20:14:55 November 4, 2017 GTM+8
 *
 * */
#include <stdio.h>
#include <string.h>

#include <kernel.h>
#include <net/mqtt.h>

#include "config.h"

#define SYS_LOG_DOMAIN "net_mqtt"
#ifdef CONFIG_APP_MQTT_DEBUG
	#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#else
	#define SYS_LOG_LEVEL SYS_LOG_LEVEL_WARNING
#endif /* CONFIG_APP_MQTT_DEBUG */
#include <logging/sys_log.h>

static struct mqtt_ctx ctx;
static struct mqtt_connect_msg connect_msg;
static struct mqtt_publish_msg publish_msg;

#ifdef CONFIG_NET_CONTEXT_NET_PKT_POOL
NET_PKT_TX_SLAB_DEFINE(mqtt_tx_slab, 30);
NET_PKT_DATA_POOL_DEFINE(mqtt_data_pool, 15);

static struct k_mem_slab *tx_slab(void)
{
	return &mqtt_tx_slab;
}

static struct net_buf_pool *data_pool(void)
{
	return &mqtt_data_pool;
}
#endif /* CONFIG_NET_CONTEXT_NET_PKT_POOL */

/**
 * This semaphore used to protect the publish_rx_cb_thread_stack below
 * since this thread stack can ONLY be occupied by one particular thread
 * */
static K_SEM_DEFINE(publish_rx_cb_thread_stack_sem, 0, 1);

static K_THREAD_STACK_DEFINE(publish_rx_cb_thread_stack, 1024);

/**
 * @brief Data parse functino entry point
 *
 * The publish_rx_cb will be called in system net parse thread, and the callback
 * will block the system until it return. In case of blocking the system, we
 * start a new thread instead processing data in current thread.
 *
 * @param arg1 Pointer point to received data
 *
 * @param arg2 The length of received data
 *
 * @param arg3 Inner semaphore to sync arg pass
 * */
static void publish_rx_cb_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
	struct k_sem *sem = (struct k_sem*)arg3;

	/* Message length */
	uint16_t msg_len = *( (uint16_t *)arg2 );

	/* Copy all data from net stack to app */
	char *msg = k_malloc(msg_len);
	if ( NULL == msg )
	{
		/* -ENOMEM */
		k_sem_give(sem);
		return ;
	}
	memcpy(msg, arg1, msg_len);

	/* All data from net stack retrieve done, release sem */
	k_sem_give(sem);

	/* Start to parse app data */
	json_cmd_parse(msg, msg_len);

	/* App process done, release stack semaphore */
	k_sem_give(&publish_rx_cb_thread_stack_sem);
}

static int publish_tx_cb(struct mqtt_ctx *ctx, uint16_t pkt_id,
			enum mqtt_packet type)
{
	return 0;
}

/**
 * @brief MQTT message recevied callback
 *
 * Doc: Networking: IP Stack Architecture:
 * The data processing in the application callback should be done fast in order
 * not to block the system too long.
 *
 * Which means, if we want to process the data for a long time, we need start a
 * new thread to achive that rather than processing the data at current thread.
 *
 * @param ctx MQTT context
 *
 * @param msg MQTT publish message structure
 *
 * @param pkt_id MQTT protocol defined, current not used
 *
 * @param type MQTT package type, current not used
 *
 * @return Must return 0 from now
 * */
static int publish_rx_cb(struct mqtt_ctx *ctx, struct mqtt_publish_msg *msg,
			uint16_t pkt_id, enum mqtt_packet type)
{
	if ( 0 != k_sem_take(&publish_rx_cb_thread_stack_sem, K_NO_WAIT) )
	{
		/**
		 * Last command still in processing
		 * We just drop current command without any notification
		 * */
		return 0;
	}

	static struct k_thread publish_rx_cb_thread_data;
	struct k_sem sem;
	k_sem_init(&sem, 0, 1);

	k_thread_create(&publish_rx_cb_thread_data, publish_rx_cb_thread_stack,
					K_THREAD_STACK_SIZEOF(publish_rx_cb_thread_stack),
					publish_rx_cb_thread_entry_point,
					(void*)msg->msg, (void*)&msg->msg_len, (void*)&sem,
					0, 0, K_NO_WAIT);

	k_sem_take(&sem, K_FOREVER);
	return 0;
}

static int subscribe_cb(struct mqtt_ctx *ctx, uint16_t pkt_id,
			uint8_t items, enum mqtt_qos qos[])
{
	return 0;
}

static int unsubscribe_cb(struct mqtt_ctx *ctx, uint16_t pkt_id)
{
	return 0;
}

/**
 * @brief Initial stage one, reset all static/global variables
 *
 * This function will never failed.
 * */
static void init1(void)
{
#ifdef CONFIG_NET_CONTEXT_NET_PKT_POLL
	ctx.net_app_ctx.tx_slab = tx_slab;
	ctx.net_app_ctx.data_pool = data_pool;
#endif
	/* MQTT Context */
	memset(&ctx, 0x00, sizeof(ctx));

	ctx.publish_tx	= publish_tx_cb;
	ctx.publish_rx	= publish_rx_cb;
	ctx.subscribe	= subscribe_cb;
	ctx.unsubscribe	= unsubscribe_cb;

	ctx.net_init_timeout = CONFIG_APP_MQTT_INIT_TIMEOUT;
	ctx.net_timeout = CONFIG_APP_MQTT_TIMEOUT;

	ctx.peer_addr_str = CONFIG_APP_MQTT_SERVER_ADDR;
	ctx.peer_port = CONFIG_APP_MQTT_SERVER_PORT;

	mqtt_init(&ctx, MQTT_APP_PUBLISHER_SUBSCRIBER);

	/* Connect message */
	memset(&connect_msg, 0x00, sizeof(connect_msg));

	connect_msg.client_id = CONFIG_APP_MQTT_CLIENT_ID;
	connect_msg.client_id_len = strlen(CONFIG_APP_MQTT_CLIENT_ID);
	connect_msg.clean_session = 1;

	/* Publish message */
	memset(&publish_msg, 0x00, sizeof(publish_msg));

	publish_msg.qos = MQTT_QoS0;
	publish_msg.topic = CONFIG_APP_MQTT_PUBLISH_TOPIC;
	publish_msg.topic_len = strlen(CONFIG_APP_MQTT_PUBLISH_TOPIC);
}

/**
 * @brief Initial stage two, connect TCP and MQTT
 *
 * Try to connect establish TCP connection and MQTT connection.
 * This function will return with timeout error.
 *
 * @return  0, Success
 *		   <0, Failed
 * */
static int init2(void)
{
	int rc = 0, i;
	const char *topics[1] = {CONFIG_APP_MQTT_SUBSCRIBE_TOPIC};
	enum mqtt_qos topics_qos[1] = {MQTT_QoS1};

	/* Enstablish TCP connection */
	for ( i = 0; i < CONFIG_APP_MQTT_CONNECT_RETRY_TIMES; ++i )
	{
		rc = mqtt_connect(&ctx);
		if ( 0 == rc )
		{
			/* Connect success */
			SYS_LOG_DBG("TCP connect OK");
			break;
		}
		SYS_LOG_DBG("TCP connect error, return: %d, retry times: %d", rc, i + 1);
	}

	/* No need to check the TCP connected or not */

	/* Enstablish MQTT connection */
	rc = mqtt_tx_connect(&ctx, &connect_msg);
	if ( 0 != rc )
	{
		SYS_LOG_DBG("MQTT connect error, return %d", rc);
		return rc;
	}
	SYS_LOG_DBG("MQTT connect OK");

	/* Subscribe to srv/controller */
	rc = mqtt_tx_subscribe(&ctx, sys_rand32_get(), 1, topics, topics_qos);
	if ( 0 != rc )
	{
		SYS_LOG_DBG("SUB to topics error, return %d", rc);
		return rc;
	}
	SYS_LOG_DBG("SUB to topics OK")

	/* MQTT connection ok, subscribe topic ok. */
	SYS_LOG_DBG("MQTT initial OK!");
	return 0;
}

/**
 * @brief Wrapper function of init1() and init2()
 *
 * @return  0, initial success
 *			<0, initial failed
 * */
static int app_mqtt_init_inner(void)
{
	int rc = 0;

	init1();
	rc = init2();

	return rc;
}

/**
 * This is mqtt ping stack definition,
 * must be global or static in local function
 * */
K_THREAD_STACK_DEFINE(mqtt_ping_stack, CONFIG_APP_MQTT_PING_STACK_SIZE);

/**
 * @brief MQTT ping request thread function
 *
 * This function is to keep the device always connect to server
 * (aka keep-alive). This function will automatic re-connect to
 * server as long as no success ping response recived.
 * */
static void mqtt_ping_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	int rc;

	while ( 1 )
	{
		/* Sleep 60 seconds, send ping reqeust to mqtt server */
		k_sleep(1000 * 60);
		if ( 0 != mqtt_tx_pingreq(&ctx) )
		{
			printk("WARNING: Ethernet cable broken, try to reconnect...!\n");
			/* Release net buff */
			//mqtt_close(&ctx);

			/* Re-initial */
			//rc = app_mqtt_init_inner();
			//if ( 0 != rc )
			//{
			//	printk("Re initial error, return %d\n", rc);
			//}
		}
	}
}


int net_mqtt_init(void)
{
	int rc = 0;
	static struct k_thread mqtt_ping_thread_data;
	rc = app_mqtt_init_inner();
	/**
	 * Thread that send ping request all the time
	 *
	 * This thread will automatic reconnect to mqtt server
	 * once the network between device and server are not available
	 * */
	k_thread_create(&mqtt_ping_thread_data, mqtt_ping_stack,
					K_THREAD_STACK_SIZEOF(mqtt_ping_stack),
					mqtt_ping_thread_entry_point,
					NULL, NULL, NULL,
					0, 0, K_NO_WAIT);
	return rc;
}

int mqtt_msg_send(const char *buff)
{
	int rc = 0, i;

	publish_msg.msg = buff;
	publish_msg.msg_len = strlen(buff);
	publish_msg.pkt_id = sys_rand32_get();

	for ( i = 0; i < CONFIG_APP_MQTT_SEND_RETRY_TIMES; ++i )
	{
		rc = mqtt_tx_publish(&ctx, &publish_msg);
		if ( 0 == rc )
		{
			/* Send success */
			return 0;
		}
		printk("[%s][%d] Send error, rc = %d\n", __func__, __LINE__, rc);
		SYS_LOG_DBG("message send error, return: %d, retry times: %d", rc, i + 1);

		/* Release mqtt net buff */
		mqtt_close(&ctx);

		/* Re-initial */
		app_mqtt_init_inner();
	}

	SYS_LOG_DBG("reconnect and send error, rc = %d", rc);

	return rc;
}

