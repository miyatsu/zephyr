#include <stdint.h>
#include <string.h>

#include <kernel.h>

#ifdef CONFIG_APP_JSON_DEBUG
#include <misc/printk.h>
#endif /* CONFIG_APP_JSON_DEBUG */

#define SYS_LOG_DOMAIN	"json"
#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>

#include "config.h"
#include "parson.h"

#include "json.h"

#include "axle.h"
#include "door.h"
#include "infrared.h"
#include "headset.h"

static const char *cmd_table[] =
{
	/* in cmd */
	"get_status",
	"open"
	"admin_fetch",
	"admin_rotate",
	"admin_close",
	"headset_buy",
	"dfu",

	/* out cmd */
	"status",

	"open_ok",
	"open_error",

	"admin_fetch_ok"
	"admin_fetch_error",
	"admin_rotate_ok",
	"admin_rotate_error",
	"admin_close_ok",
	"admin_close_error",

	"headset_buy_ok",
	"headset_buy_error",

	/* Unexpected */
	"error_log",

	NULL,
};

/**
 * @brief Add the common json field into json value
 *
 * They are include axle status, door status, cabinet status and
 * headset stock.
 *
 * @param root_out Json value pointer that wait to be send out
 * */
static void out_json_add_status_field(JSON_Value *root_out)
{
	uint8_t i, j;

	bool axle_status			= axle_get_status();
	bool *door_status_array		= door_get_status_array();
	uint8_t *box_status_array	= infrared_get_status_array();
	int8_t headset_stock		= headset_get_stock();

	JSON_Value *door			= json_value_init_array();
	JSON_Value *box_out			= json_value_init_array();
	JSON_Value *box_in[4]		= {NULL, NULL, NULL, NULL};

	/**
	 * JSON format:
	 * {
	 *		"axle": 0
	 *		"door": [0, 0, 0, 0];
	 *		"cabinets"[
	 *			[0, 0, 0, 0, 0, 0, 0],
	 *			[0, 0, 0, 0, 0, 0, 0],
	 *			[0, 0, 0, 0, 0, 0, 0],
	 *			[0, 0, 0, 0, 0, 0, 0]
	 *		]
	 * }
	 * */

	if ( NULL == door || NULL == box_out )
	{
		return ;
	}

	/* Add "axle" field into json */
	json_object_set_number(json_object(root_out), "axle", axle_status ? 1 : 0);

	for ( i = 0; i < 4; ++i )
	{
		json_array_append_number(json_array(door), door_status_array[i] ? 1 : 0);
	}
	/* Add "door" field into json */
	json_object_set_value(json_object(root_out), "doors", door);

	for ( i = 0; i < 4; ++i )
	{
		box_in[i] = json_value_init_array();
		if ( NULL == box_in[i] )
		{
			return ;
		}

		for ( j = 0; j < 7; ++j )
		{
			json_array_append_number(json_array(box_in[i]), box_status_array[i * 7 + j]);
		}
		json_array_append_value(json_array(box_out), box_in[i]);
	}
	/* Add "cabinets" field into json */
	json_object_set_value(json_object(root_out), "cabinets", box_out);

	/* Add "headset_stock" field into json */
	json_object_set_number(json_object(root_out), "headset_stock", headset_stock);
}

/**
 * @brief Json send out function
 *
 * This function will serialize the json value, and send out though
 * mqtt. Then release the memory include serialized string, send out
 * json value and receved json value.
 *
 * @param root_in Parse result of receved json string
 *		  root_out Json value wait to be send out
 * */
static void out_json_comm(JSON_Value *root_in, JSON_Value *root_out)
{
	/* "ext" field need be the same as the in json value */
	const char *ext = json_object_get_string(json_object(root_in), "ext");
	json_object_set_string(json_object(root_out), "ext", ext);

	/* Put the rest of field into json value */
	out_json_add_status_field(root_out);

	/* Make string to send */
	char *json = json_serialize_to_string(root_out);

	/* Send json to controller */
	mqtt_msg_send(json);

	/* Release all json related memory */
	json_free_serialized_string(json);
	json_value_free(root_out);
	json_value_free(root_in);
}

static void run_cmd_get_status(JSON_Value *root_in, JSON_Value *root_out)
{
	/* Add "cmd" field */
	json_object_set_string(json_object(root_out), "cmd", "status");
	out_json_comm(root_in, root_out);
}

int8_t do_cmd_open(uint8_t layer, uint8_t position)
{
	int8_t rc;

	/* Rotate to correct position */
	rc = axle_rotate_to(position);
	if ( 0 != rc )
	{
		return rc;
	}

	/* Open the door */
	rc = door_open(layer);
	if ( 0 != rc )
	{
		return rc;
	}

	/* Wait use take out or put in the cargo */
	k_sleep(5000);

	/* Close the door */
	rc = door_close(layer);
	if ( 0 != rc )
	{
		return rc;
	}

	/* Every things fine */
	return 0;
}

static void run_cmd_open(JSON_Value *root_in, JSON_Value *root_out)
{
	uint8_t position, layer;

	position = json_object_get_number(json_object(root_in), "position");
	if ( !(position >= 1 && position <= 7) )
	{
		/* Error handling */
		return ;
	}

	layer = json_object_get_number(json_object(root_in), "layer");
	if ( !(layer >= 1 && layer <= 4) )
	{
		/* Error handling */
		return ;
	}

	if ( 0 == do_cmd_open(layer, position) )
	{
		json_object_set_string(json_object(root_out), "cmd", "open_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "open_error");
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_admin_fetch(JSON_Value *root_in, JSON_Value *root_out)
{
	if ( 0 == axle_rotate_to(1) && 0 == door_admin_open() )
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_fetch_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_fetch_error");
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_admin_rotate(JSON_Value *root_in, JSON_Value *root_out)
{
	if ( 0 == axle_rotate_to_next() )
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_rotate_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_rotate_error");
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_admin_close(JSON_Value *root_in, JSON_Value *root_out)
{
	if ( 0 == door_admin_close() )
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_close_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_close_error");
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_headset_buy(JSON_Value *root_in, JSON_Value *root_out)
{
	if ( 0 == headset_buy() )
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_buy_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_buy_error");
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_dfu(JSON_Value *root_in, JSON_Value *root_out)
{
	return ;
}

/* Json string buffer */
static uint8_t buff[2048];

void json_cmd_parse(uint8_t *msg, uint16_t msg_len)
{
	char *cmd = NULL;
	enum cmd_table_index_e index;

	/* Copy net buff to local buff */
	memcpy(buff, msg, msg_len);
	buff[msg_len] = 0;

	SYS_LOG_DBG("Json recv: %s", buff);

	/* Start to parse json, the memory will free at each switch function */
	JSON_Value *root = json_parse_string(buff);
	if ( NULL == root )
	{
		/* Json parse error */
		goto error;
	}

	/* Get "cmd" field string value */
	cmd = json_object_get_string(json_object(root), "cmd");

	/* Ergodic every possibile cmd */
	for ( index = CMD_IN_START; index <= CMD_IN_END; ++index )
	{
		if ( 0 == strcmp(cmd_table[index], cmd) )
		{
			/* Cmd matched */
			break;
		}
	}

	/* Is the cmd match the cmd table ? */
	if ( index > CMD_IN_END )
	{
		json_value_free(root);
		goto error;
	}

	/* Out cmd json */
	JSON_Value *root_out = json_value_init_object();
	if ( NULL == root_out )
	{
		goto error;
	}

	/* Start to run specific cmd */
	switch ( index )
	{
		case CMD_GET_STATUS:
			run_cmd_get_status(root, root_out);
			break;
		case CMD_OPEN:
			run_cmd_open(root, root_out);
			break;
		case CMD_ADMIN_OPEN:
			run_cmd_admin_fetch(root, root_out);
			break;
		case CMD_ADMIN_ROTATE:
			run_cmd_admin_rotate(root, root_out);
			break;
		case CMD_ADMIN_CLOSE:
			run_cmd_admin_close(root, root_out);
			break;
		case CMD_HEADSET_BUY:
			run_cmd_headset_buy(root, root_out);
			break;
		case CMD_DFU:
			run_cmd_dfu(root, root_out);
			break;
		default:
			json_value_free(root);
			json_value_free(root_out);
			goto error;
	}
	return ;
error:
	return ;
}

#ifdef CONFIG_APP_JSON_DEBUG

void json_debug(void)
{
	const char json[] = "{\"cmd\": \"get_status\", \"ext\": \"12345\"}";
	json_cmd_parse((uint8_t *)json, sizeof(json));
}

void json_debug_11(void)
{
	JSON_Value *root = json_value_init_object();

/*
	JSON_Value *box = json_parse_string("["
			"[true, true, true, true],"
			"[true, true, true, true],"
			"[true, true, true, true],"
			"[true, true, true, true]"
			"]");
*/
	JSON_Value *box_in[4];
	JSON_Value *box_out = json_value_init_array();
	for ( uint8_t i = 0; i < 4; ++i )
	{
		box_in[i] = json_value_init_array();
		for ( uint8_t j = 0; j < 7; ++j )
		{
			json_array_append_boolean(json_array(box_in[i]), true);
		}
		json_array_append_value(json_array(box_out), box_in[i]);
	}

	json_object_set_string(json_object(root), "cmd", "open");
	json_object_set_number(json_object(root), "position", 4);
	json_object_set_number(json_object(root), "layer", 3);
	json_object_set_value(json_object(root), "box", box_out);

	char *json = json_serialize_to_string(root);
	printk("%s\n", json);
	json_free_serialized_string(json);
	json_value_free(root);
	return ;
}

void json_debug_(void)
{
	char json_cmd_open[] = "{\"cmd\": \"borrow\", \"round\": 1, \"number\": 1}";
	JSON_Value *root_value = NULL;
	char *cmd = NULL;

	root_value = json_parse_string(json_cmd_open);
	if ( NULL == root_value )
	{
		printk("root_value == NULL!\n");
	}
	else
	{
		printk("json parse ok ??\n");
	}

	cmd = json_object_get_string(json_object(root_value), "cmd");
	cmd == NULL ? 1 : printk("%s\n", cmd);

	double number = json_object_get_number(json_object(root_value), "round");
	printk("%d\n", (int)number);

	json_value_free(root_value);

	return ;
}

#endif /* CONFIG_APP_JSON_DEBUG */

