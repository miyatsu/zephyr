/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <net/mqtt.h>

#include <net/net_context.h>

#include <misc/printk.h>
#include <string.h>
#include <errno.h>

#include "config.h"


/* Container for some structures used by the MQTT publisher app. */
struct mqtt_client_ctx {
	/**
	 * The connect message structure is only used during the connect
	 * stage. Developers must set some msg properties before calling the
	 * mqtt_tx_connect routine. See below.
	 */
	struct mqtt_connect_msg connect_msg;
	/**
	 * This is the message that will be received by the server
	 * (MQTT broker).
	 */
	struct mqtt_publish_msg pub_msg;

	/**
	 * This is the MQTT application context variable.
	 */
	struct mqtt_ctx mqtt_ctx;

	/**
	 * This variable will be passed to the connect callback, declared inside
	 * the mqtt context struct. If not used, it could be set to NULL.
	 */
	void *connect_data;

	/**
	 * This variable will be passed to the disconnect callback, declared
	 * inside the mqtt context struct. If not used, it could be set to NULL.
	 */
	void *disconnect_data;

	/**
	 * This variable will be passed to the publish_tx callback, declared
	 * inside the mqtt context struct. If not used, it could be set to NULL.
	 */
	void *publish_data;
};

/* The mqtt client struct */
static struct mqtt_client_ctx client_ctx;


/* The signature of this routine must match the connect callback declared at
 * the mqtt.h header.
 */
static void connect_cb(struct mqtt_ctx *mqtt_ctx)
{
	struct mqtt_client_ctx *client_ctx;

	client_ctx = CONTAINER_OF(mqtt_ctx, struct mqtt_client_ctx, mqtt_ctx);

	printk("[%s:%d]", __func__, __LINE__);

	if (client_ctx->connect_data) {
		printk(" user_data: %s",
		       (const char *)client_ctx->connect_data);
	}

	printk("\n");
}

/* The signature of this routine must match the disconnect callback declared at
 * the mqtt.h header.
 */
static void disconnect_cb(struct mqtt_ctx *mqtt_ctx)
{
	struct mqtt_client_ctx *client_ctx;

	client_ctx = CONTAINER_OF(mqtt_ctx, struct mqtt_client_ctx, mqtt_ctx);

	printk("[%s:%d]", __func__, __LINE__);

	if (client_ctx->disconnect_data) {
		printk(" user_data: %s",
		       (const char *)client_ctx->disconnect_data);
	}

	printk("\n");
	printk("start to reconnect...\n");
	void vrbox_mqtt_init(void);
	vrbox_mqtt_init();
}

/**
 * The signature of this routine must match the publish_tx callback declared at
 * the mqtt.h header.
 *
 * NOTE: we have two callbacks for MQTT Publish related stuff:
 *	- publish_tx, for publishers
 *	- publish_rx, for subscribers
 *
 * Applications must keep a "message database" with pkt_id's. So far, this is
 * not implemented here. For example, if we receive a PUBREC message with an
 * unknown pkt_id, this routine must return an error, for example -EINVAL or
 * any negative value.
 */
static int publish_tx_cb(struct mqtt_ctx *mqtt_ctx, u16_t pkt_id,
				enum mqtt_packet type)
{
	struct mqtt_client_ctx *client_ctx;
	const char *str;
	int rc = 0;

	client_ctx = CONTAINER_OF(mqtt_ctx, struct mqtt_client_ctx, mqtt_ctx);

	switch (type) {
	case MQTT_PUBACK:
		str = "MQTT_PUBACK";
		break;
	case MQTT_PUBCOMP:
		str = "MQTT_PUBCOMP";
		break;
	case MQTT_PUBREC:
		str = "MQTT_PUBREC";
		break;
	default:
		rc = -EINVAL;
		str = "Invalid MQTT packet";
	}

	printk("[%s:%d] <%s> packet id: %u", __func__, __LINE__, str, pkt_id);

	if (client_ctx->publish_data) {
		printk(", user_data: %s",
		       (const char *)client_ctx->publish_data);
	}

	printk("\n");

	return rc;
}

static int publish_rx_cb(struct mqtt_ctx *ctx, struct mqtt_publish_msg *msg,
				u16_t pkt_id, enum mqtt_packet type)
{
	// FIXME: Check for topic valiation.
	json_cmd_parse(msg->msg, msg->msg_len);
	return 0;
}


/**
 * The signature of this routine must match the malformed callback declared at
 * the mqtt.h header.
 */
static void malformed_cb(struct mqtt_ctx *mqtt_ctx, u16_t pkt_type)
{
	//TODO: Reconnect to MQTT server.
	return ;
}

static int subscribe_cb(struct mqtt_ctx *ctx, u16_t pkt_id,
				u8_t items, enum mqtt_qos qos[])
{
	printk("[%s]: %d called.\n", __func__, __LINE__);
	return 0;
}

static int unsubscribe_cb(struct mqtt_ctx *ctx, u16_t pkt_id)
{
	return 0;
}

int mqtt_msg_send(char *buff)
{
	int rc;
	client_ctx.pub_msg.msg = buff;
	client_ctx.pub_msg.msg_len = strlen(buff);
	/**
	 * Those three can be constant, initial at vrbox_init()
	 *
	 * client_ctx.pub_msg.qos = MQTT_QoS1;
	 * client_ctx.pub_msg.topic = "srv/controller";
	 * client_ctx.pub_msg.topic_len = strlen("srv/controller");
	 */
	client_ctx.pub_msg.pkt_id = sys_rand32_get();

	rc = mqtt_tx_publish(&client_ctx.mqtt_ctx, &client_ctx.pub_msg);
	k_sleep(100);

	return rc;
}

#define RC_STR(rc)	((rc) == 0 ? "OK" : "ERROR")

#define PRINT_RESULT(func, rc)	\
	printk("[%s:%d] %s: %d <%s>\n", __func__, __LINE__, \
	       (func), rc, RC_STR(rc))


void vrbox_mqtt_init(void)
{
	int rc;

	/* Set everything to 0 and later just assign the required fields. */
	memset(&client_ctx, 0x00, sizeof(client_ctx));

	/* connect, disconnect and malformed may be set to NULL */
	client_ctx.mqtt_ctx.connect = connect_cb;

	client_ctx.mqtt_ctx.disconnect = disconnect_cb;
	client_ctx.mqtt_ctx.malformed = malformed_cb;

	client_ctx.mqtt_ctx.net_init_timeout = APP_NET_INIT_TIMEOUT;
	client_ctx.mqtt_ctx.net_timeout = APP_TX_RX_TIMEOUT;

	client_ctx.mqtt_ctx.peer_addr_str = SERVER_ADDR;
	client_ctx.mqtt_ctx.peer_port = SERVER_PORT;


	/* Publisher apps TX the MQTT PUBLISH msg */
	client_ctx.mqtt_ctx.publish_tx = publish_tx_cb;
	client_ctx.mqtt_ctx.publish_rx = publish_rx_cb;
	client_ctx.mqtt_ctx.subscribe = subscribe_cb;
	client_ctx.mqtt_ctx.unsubscribe = unsubscribe_cb;

	/* Publish masssage config */
	client_ctx.pub_msg.qos = MQTT_QoS1;
	client_ctx.pub_msg.topic = MQTT_PUBLISH_TOPIC;
	client_ctx.pub_msg.topic_len = strlen(MQTT_PUBLISH_TOPIC);

	/* The connect message will be sent to the MQTT server (broker).
	 * If clean_session here is 0, the mqtt_ctx clean_session variable
	 * will be set to 0 also. Please don't do that, set always to 1.
	 * Clean session = 0 is not yet supported.
	 */
	client_ctx.connect_msg.client_id = MQTT_CLIENTID;
	client_ctx.connect_msg.client_id_len = strlen(MQTT_CLIENTID);
	client_ctx.connect_msg.clean_session = 1;

	/* Unused */
	client_ctx.connect_data = "CONNECTED";
	client_ctx.disconnect_data = "DISCONNECTED";
	client_ctx.publish_data = "PUBLISH";


	printk("Start to init mqtt\n");
	rc = mqtt_init(&client_ctx.mqtt_ctx, MQTT_APP_PUBLISHER_SUBSCRIBER);
	PRINT_RESULT("mqtt_init", rc);
	if (rc != 0) {
		return;
	}

	printk("start to connect tcp...\n");
	/* No return until TCP is connected */
	while ( 0 != mqtt_connect(&client_ctx.mqtt_ctx) );
	printk("tcp server connected.\n");

	printk("start to connect mqtt...\n");
	/* No return until MQTT is connected */
	while ( (!client_ctx.mqtt_ctx.connected) &&
			(0 != mqtt_tx_connect(&client_ctx.mqtt_ctx, 
								  &client_ctx.connect_msg)));
	// FIXME: Power up with no ethernet cable in,
	// may will cause uninitialized of network stack.
	//
	// FIX_PLAN: auto reconnect.

	/* Start to subscribe topics */
	const char *topics[1] = {MQTT_SUBSCRIBE_TOPIC};
	enum mqtt_qos topics_qos[1] = {MQTT_QoS1};
	rc = mqtt_tx_subscribe(&client_ctx.mqtt_ctx,
							sys_rand32_get(),
							1,
							topics,
							topics_qos);

	return ;
}

