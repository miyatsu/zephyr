/**
 * Copyright (c) 2017-2018 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @fiel service.c
 *
 * @brief All API expose to upper machine
 *
 * This is the implementation of all APIs that expose to upper machine, include
 * dfu, error_log, admin relative command, and so on.
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 20:58:18 January 4, 2018 GTM+8
 *
 * */
#include <stdint.h>
#include <string.h>

#include <kernel.h>

#include <dfu/mcuboot.h>

#include <misc/reboot.h>
#include <misc/printk.h>

#include "config.h"
#include "parson.h"
#include "dfu_http.h"
#include "mqtt.h"

#include "service.h"

#include "axle.h"
#include "door.h"
#include "infrared.h"
#include "headset.h"

#define SYS_LOG_DOMAIN	"service"
#ifdef CONFIG_APP_JSON_DEBUG
	#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_DEBUG
#else
	#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_ERROR
#endif /* CONFIG_APP_JSON_DEBUG */
#include <logging/sys_log.h>

enum cmd_table_index_e
{
	CMD_START = 0,
	/* in cmd */
	CMD_IN_START = CMD_START,
	CMD_GET_STATUS = CMD_IN_START,
	CMD_OPEN,
	CMD_CLOSE,
	CMD_ADMIN_FETCH,
	CMD_ADMIN_ROTATE,
	CMD_ADMIN_CLOSE,
	CMD_HEADSET_BUY,
	CMD_HEADSET_ADD,
	CMD_HEADSET_RECOUNT,
	CMD_DFU,
#ifdef CONFIG_APP_FACTORY_TEST
	CMD_FACTORY_TEST,
	CMD_IN_END = CMD_FACTORY_TEST,
#else
	CMD_IN_END = CMD_DFU,
#endif /* CONFIG_APP_FACTORY_TEST */

	/* out cmd */
	CMD_OUT_START,
	CMD_STATUS = CMD_OUT_START,

	CMD_OPEN_OK,
	CMD_OPEN_ERROR,

	CMD_ADMIN_FETCH_OK,
	CMD_ADMIN_FETCH_ERROR,
	CMD_ADMIN_ROTATE_OK,
	CMD_ADMIN_ROTATE_ERROR,
	CMD_ADMIN_CLOSE_OK,
	CMD_ADMIN_CLOSE_ERROR,

	CMD_HEADSET_BUY_OK,
	CMD_HEADSET_BUY_ERROR,
	CMD_HEADSET_ADD_OK,
	CMD_HEADSET_ADD_ERROR,
	CMD_HEADSET_RECOUNT_OK,
	CMD_HEADSET_RECOUNT_ERROR,

	CMD_ERROR_LOG,

	CMD_OUT_END = CMD_ERROR_LOG,
	CMD_END = CMD_OUT_END,

	CMD_NULL,
};

static const char *cmd_table[] =
{
	/* in cmd */
	"get_status",
	"open",
	"close",
	"admin_fetch",
	"admin_rotate",
	"admin_close",
	"headset_buy",
	"headset_add",
	"headset_recount",
	"dfu",
#ifdef CONFIG_APP_FACTORY_TEST
	"factory_test",
#endif /* CONFIG_APP_FACTORY_TEST */

	/* out cmd */
	"status",

	"open_ok",
	"open_error",

	"admin_fetch_ok",
	"admin_fetch_error",
	"admin_rotate_ok",
	"admin_rotate_error",
	"admin_close_ok",
	"admin_close_error",

	"headset_buy_ok",
	"headset_buy_error",
	"headset_add_ok",
	"headset_add_error",
	"headset_recount_ok",
	"headset_recount_error",

	"error_log",

	/* Unexpected */
	NULL,
};

static int service_dfu(JSON_Value *root_in)
{
	int rc = 0;

	JSON_Value *root_out	= NULL;
	const char *sub_cmd		= NULL;

	/* Check incoming json param */
	if ( NULL == root_in )
	{
		SYS_LOG_ERR("No root_in JSON_Value find!");
		rc = -1;
		goto out;
	}

	/* Check sub command */
	sub_cmd = json_object_get_string(json_object(root_in), "sub_cmd");
	if ( NULL == sub_cmd )
	{
		SYS_LOG_ERR("No sub_cmd field finded.");
		rc = -1;
		goto out;
	}

	/* Create a new json_object to response */
	root_out = json_value_init_object();
	if ( NULL == root_out )
	{
		SYS_LOG_ERR("No memory at line: %d", __LINE__);
		rc = -ENOMEM;
		goto out;
	}
	json_object_set_string(json_object(root_out), "cmd", "dfu");
	json_object_set_string(json_object(root_out), "sub_cmd", sub_cmd);

	/**
	 * Upgrade command need to sendout status and reboot,
	 * seperate it with other command
	 * */
	if ( 0 == strcmp(sub_cmd, "upgrade") )
	{
		/* Local variables in "if" code block */
		const char *url		= NULL;
		const char *md5		= NULL;
		int size			= 0;

		char *message		= NULL;

		/**
		 * Use rc as error_code
		 *
		 * ---------------------------------
		 * | rc  | Detial                  |
		 * ======|==========================
		 * |  0  | OK, reboot to upgrade   |
		 * ---------------------------------
		 * | -1  | Message format error    |
		 * ---------------------------------
		 * | -2  | Firmware download error |
		 * ---------------------------------
		 * | -3  | MD5 Check error         |
		 * ---------------------------------
		 * | -4  | Upgrade error           |
		 * ---------------------------------
		 *
		 * */

		url = json_object_get_string(json_object(root_in), "url");
		if ( NULL == url )
		{
			SYS_LOG_ERR("No url field find.");
			rc = -1;
			goto upgrade_out;
		}

		md5 = json_object_get_string(json_object(root_in), "md5");
		if ( NULL == url )
		{
			SYS_LOG_ERR("No md5 field find.");
			rc = -1;
			goto upgrade_out;
		}

		size = json_object_get_number(json_object(root_in), "size");
		if ( 0 == size )
		{
			SYS_LOG_ERR("No size field find.");
			rc = -1;
			goto upgrade_out;
		}

		/* Start to download */
		rc = dfu_http_download(url, strlen(url));
		if ( 0 != rc )
		{
			SYS_LOG_ERR("Firmware download failed, rc = %d", rc);
			rc = -2;
			goto upgrade_out;
		}

		/* Download complete, check md5 */
		rc = dfu_md5_check(size, md5);
		if ( 0 != rc )
		{
			SYS_LOG_ERR("Firmware md5 check failed.");
			rc = -3;
			goto upgrade_out;
		}

		/**
		 * Now the new firmware has successfuly downloaded and checked
		 *
		 * Perform upgrade now
		 * */

		rc = boot_request_upgrade(1);
		if ( 0 != rc )
		{
			SYS_LOG_ERR("Request upgrade error.");
			rc = -4;
			goto upgrade_out;
		}

upgrade_out:
		json_object_set_number(json_object(root_out), "error_code", rc);

		message = json_serialize_to_string(root_out);
		if ( NULL == message )
		{
			rc = -ENOMEM;
			json_value_free(root_out);
			SYS_LOG_ERR("No memory at line: %d", __LINE__);
			return rc;
		}

		mqtt_msg_send(message);

		/* Release heap memory */
		json_free_serialized_string(message);
		json_value_free(root_out);

		/* Send out without save rc */
		if ( 0 == rc )
		{
			/* Use _Noreturn qualifier for this func */
			sys_reboot(SYS_REBOOT_COLD);
			return rc;
		}
		else
		{
			/* Some error happend, just return */
			SYS_LOG_ERR("Unknow error, rc = %d", rc);
			return rc;
		}
	}

	/* Other sub_cmd except "upgrade" */
	if ( 0 == strcmp(sub_cmd, "version") )
	{
		/* Add version field to JSON_Value */
		json_object_set_string(json_object(root_out), "sub_cmd", "version");
		json_object_set_string(json_object(root_out), "version",
				CONFIG_APP_DFU_VERSION_STRING);
	}
	else if ( 0 )
	{
		/* Reserved for future extension */
	}

out:
	json_value_free(root_out);
	return rc;
}

/**
 * @brief Send error log message via mqtt
 *
 * This function will be called at syslog_hook user defined function
 *
 * @param msg The log message that wait to be send
 *
 * @param 0 Send OK
 *		  <1 Some error happened
 * */
int service_send_error_log(const char *msg)
{
	int rc = 0;

	JSON_Value *root_out = NULL;

	root_out = json_value_init_object();
	if ( NULL == root_out )
	{
		rc = -ENOMEM;
		goto out;
	}

	json_object_set_string(json_object(root_out), "cmd", cmd_table[CMD_ERROR_LOG]);
	json_object_set_string(json_object(root_out), "msg", msg);

	char *json = json_serialize_to_string(root_out);
	if ( NULL == json )
	{
		rc = -ENOMEM;
		json_value_free(root_out);
		goto out;
	}

	rc = mqtt_msg_send(json);

	/* Release heap memory */
	json_free_serialized_string(json);
	json_value_free(root_out);

out:
	return rc;
}

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

	json_object_set_string(json_object(root_out), "version", CONFIG_APP_DFU_VERSION_STRING);
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

	k_sleep(1000);

	/* Open the door */
	rc = door_open(layer);
	if ( 0 != rc )
	{
		return rc;
	}

	/* Every thing is fine */
	return 0;
}

int8_t do_cmd_close(uint8_t layer, uint8_t position)
{
	int rc;

	ARG_UNUSED(position);
	rc = door_close(layer);
	if ( 0 != rc )
	{
		return rc;
	}

	/* TODO Error code design, check infrared detector status */
	/* Every thing is fine */
	return 0;
}

/**
 * @brief Check current open/close command is the borrow related command
 *
 * @param ext Address of ext string field
 *
 * @return true Yes
 *		   false No
 * */
static bool cmd_ext_is_borrow(const char *ext)
{
	JSON_Value * root_in_ext = NULL;

	/* Default not borrow */
	bool rc = false;

	/* Parse json */
	root_in_ext = json_parse_string(ext);
	if ( NULL == root_in_ext )
	{
		SYS_LOG_ERR("Can not parse ext field at line: %d!", __LINE__);
		goto out;
	}

	/* Get cmd field */
	const char *ext_cmd = json_object_get_string(json_object(root_in_ext), "cmd");
	if ( NULL == ext_cmd )
	{
		SYS_LOG_ERR("No cmd field in ext field at line: %d!", __LINE__);
		goto out;
	}

	/* Check command */
	if ( 0 == strcmp(ext_cmd, "borrow") )
	{
		rc = true;
	}

out:
	json_value_free(root_in_ext);
	return rc;
}

/**
 * @brief Check if current open/close command is the back related command
 *
 * @param ext Address of ext string field
 *
 * @return true Yes
 *		   false No
 * */
static bool cmd_ext_is_back(const char *ext)
{
	JSON_Value * root_in_ext = NULL;

	/* Default not back */
	bool rc = false;

	/* Parse json */
	root_in_ext = json_parse_string(ext);
	if ( NULL == root_in_ext )
	{
		SYS_LOG_ERR("Can not parse ext field at line: %d!", __LINE__);
		goto out;
	}

	/* Get cmd field */
	const char *ext_cmd = json_object_get_string(json_object(root_in_ext), "cmd");
	if ( NULL == ext_cmd )
	{
		SYS_LOG_ERR("No cmd field in ext field at line: %d!", __LINE__);
		goto out;
	}

	/* Check command */
	if ( 0 == strcmp(ext_cmd, "back") )
	{
		rc = true;
	}

out:
	json_value_free(root_in_ext);
	return rc;
}

static void out_json_add_coordinate(JSON_Value *root_out, uint8_t position, uint8_t layer)
{
	JSON_Value *coordinate = json_value_init_array();
	if ( NULL == coordinate )
	{
		SYS_LOG_ERR("No memory at line: %d", __LINE__);
		return ;
	}

	json_array_append_number(json_array(coordinate), layer);
	json_array_append_number(json_array(coordinate), position);

	json_object_set_value(json_object(root_out), "coordinate", coordinate);
}

static void run_cmd_open(JSON_Value *root_in, JSON_Value *root_out)
{
	uint8_t position, layer;
	int rc = 0;

	position = json_object_get_number(json_object(root_in), "position");
	if ( !(position >= 1 && position <= 7) )
	{
		/* Error handling */
		SYS_LOG_ERR("Position out of range, position = %d", position);
		return ;
	}

	layer = json_object_get_number(json_object(root_in), "layer");
	if ( !(layer >= 1 && layer <= 4) )
	{
		/* Error handling */
		SYS_LOG_ERR("Layer out of range, layer = %d", layer);
		return ;
	}

	/**
	 * Upper x86 is written in JS, async IO for all processing,
	 * so add more field to let the upper x86 know what's position
	 * it needed when send back operation result.
	 * */
	out_json_add_coordinate(root_out, position, layer);

	rc = do_cmd_open(layer, position);
	if ( 0 == rc )
	{
		json_object_set_string(json_object(root_out), "cmd", "open_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "open_error");
		json_object_set_number(json_object(root_out), "error_code", rc);
	}

	out_json_comm(root_in, root_out);
}

/**
 * @brief Run command close
 *
 * @param root_in Incoming parsed json value pointer
 *		  root_out Outgoing parsed json value pointer
 * */
static void run_cmd_close(JSON_Value *root_in, JSON_Value *root_out)
{
	uint8_t position, layer;
	int i, rc = 0;
	int error_code = 0;

	/* Get axle position */
	position = json_object_get_number(json_object(root_in), "position");
	if ( !(position >= 1 && position <= 7) )
	{
		/* Error handling */
		SYS_LOG_ERR("Position out of range, position = %d", position);
		return ;
	}

	/* Get door layer */
	layer = json_object_get_number(json_object(root_in), "layer");
	if ( !(layer >= 1 && layer <= 4) )
	{
		/* Error handling */
		SYS_LOG_ERR("Layer out of range, layer = %d", layer);
		return ;
	}

	/**
	 * Upper x86 is written in JS, async IO for all processing,
	 * so add more field to let the upper x86 know what's position
	 * it needed when send back the operation result.
	 * */
	out_json_add_coordinate(root_out, position, layer);

	const char *ext = json_object_get_string(json_object(root_in), "ext");
	if ( NULL == ext )
	{
		/* Error handling */
		SYS_LOG_ERR("No ext field find in command!");
		return ;
	}

	/**
	 * TODO Polling infrared detector status for at least 3 seconds,
	 * make sure the cargo is on board.
	 *
	 * For test purpose, we just sleep for 3 seconds.
	 * */
	if ( cmd_ext_is_back(ext) )
	{
		/**
		 * Upper x86 use 5 seconds for delay, we sleep 3 seconds
		 * first, make sure the cargo is on board.
		 * */
		k_sleep(3000);

		int i = 0;
		uint8_t *box_status = NULL;

		/* Check if the cargo is on board */
		box_status = infrared_get_status_array();
		if ( 0 == box_status[(layer - 1) * 7 + (position - 1)] )
		{
			/* Box empty */
			error_code = 2;
		}

		/* Polling infrared 2 seconds, make sure the cargo is on board */
		for ( i = 0; i < 4; ++i )
		{
			box_status = infrared_get_status_array();
			if ( 0 == box_status[(layer - 1) * 7 + (position - 1)] )
			{
				/* Box empty */
				break;
			}
			k_sleep(500);
		}

		/* On board cargo is not ready to launch */
		if ( i < 4 )
		{
			error_code = 3;
		}
	}
	else if ( cmd_ext_is_borrow(ext) )
	{
		/* Nothing */
		k_sleep(15000);
	}
	else
	{
		SYS_LOG_ERR("Line: %d Cmd field in ext not match \"back\" or \"borrow\"!",
				__LINE__);
		k_sleep(3000);
	}

	for ( i = 0; i < 3; ++i )
	{
		/**
		 * Try to close the door
		 * return positive means there is infrared detected
		 * return negetive means there is an error happened
		 * return zero means door close ok
		 * */
		rc = do_cmd_close(layer, position);

		if ( 0 == rc )
		{
			json_object_set_string(json_object(root_out), "cmd", "close_ok");
			break;
		}
		else if ( rc < 0 )
		{
			json_object_set_string(json_object(root_out), "cmd", "close_error");

			/* Add error_code */
			error_code = rc;
			break;
		}
		else
		{
			/* On door infrared detected */

			/* Start to open the door immidialtly */
			door_open(layer);

			/* When the door fully opened, wait 3 seconds then try to close agian */
			k_sleep(3000);
			continue;
		}
	}

	if ( i >= 3 )
	{
		/* Infrared detected, close door timeout */
		error_code = 1;
	}
	json_object_set_number(json_object(root_out), "error_code", error_code);

	out_json_comm(root_in, root_out);
}

static void run_cmd_admin_fetch(JSON_Value *root_in, JSON_Value *root_out)
{
	uint8_t position = json_object_get_number(json_object(root_in), "position");
	if ( !(position >= 1 && position <= 7) )
	{
		return ;
	}

	int rc = 0;

	rc = axle_rotate_to(position);
	if ( 0 != rc )
	{
		goto out;
	}

	printk("Move axle to position done!\n");

	rc = door_admin_open();
	if ( 0 != rc )
	{
		goto out;
	}

out:
	SYS_LOG_DBG("rc = %d", rc);
	if ( 0 == rc )
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_fetch_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_fetch_error");
		json_object_set_number(json_object(root_out), "error_code", rc);
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_admin_rotate(JSON_Value *root_in, JSON_Value *root_out)
{
	uint8_t position = json_object_get_number(json_object(root_in), "position");
	if ( !( position >= 1 && position <= 7 ) )
	{
		/* TODO Error handle */
		return ;
	}

	int rc = 0;

	rc = axle_rotate_to(position);
	if ( 0 == rc )
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_rotate_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_rotate_error");
		json_object_set_number(json_object(root_out), "error_code", rc);
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_admin_close(JSON_Value *root_in, JSON_Value *root_out)
{
	int rc = 0;

	rc = door_admin_close();
	if ( 0 == rc )
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_close_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "admin_close_error");
		json_object_set_number(json_object(root_out), "error_code", rc);
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_headset_buy(JSON_Value *root_in, JSON_Value *root_out)
{
	int rc = 0;

	rc = headset_buy();
	if ( 0 == rc )
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_buy_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_buy_error");
		json_object_set_number(json_object(root_out), "error_code", rc);
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_headset_add(JSON_Value *root_in, JSON_Value *root_out)
{
	int rc = 0;

	rc = headset_add();
	if ( 0 == rc )
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_add_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_add_error");
		json_object_set_number(json_object(root_out), "error_code", rc);
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_headset_recount(JSON_Value *root_in, JSON_Value *root_out)
{
	int rc = 0;

	rc = headset_stock_init();
	if ( 0 != rc )
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_recount_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_recount_error");
		json_object_set_number(json_object(root_out), "error_code", rc);
	}

	out_json_comm(root_in, root_out);
}

#ifdef CONFIG_APP_FACTORY_TEST

static void run_cmd_factory_test(JSON_Value *root_in, JSON_Value *root_out)
{
	int i, rc = 0;

	const char *component = NULL;
	const char *operation = NULL;

	int parameter = -1;

	const char *ext		= NULL;

	char *json		= NULL;

	component = json_object_get_string(json_object(root_in), "component");
	if ( NULL == component )
	{
		rc = -EINVAL;
		goto out;
	}

	operation = json_object_get_string(json_object(root_in), "operation");
	if ( NULL == operation )
	{
		rc = -EINVAL;
		goto out;
	}

	/* returns 0 on fail */
	parameter = json_object_get_number(json_object(root_in), "parameter");

	ext = json_object_get_string(json_object(root_in), "ext");

	enum conponent_enum {
		COMPONENT_START = -1,

#ifdef CONFIG_APP_AXLE_FACTORY_TEST
		COMPONENT_AXLE,
#endif /* CONFIG_APP_AXLE_FACTORY_TEST */

#ifdef CONFIG_APP_DOOR_FACTORY_TEST
		COMPONENT_DOOR,
#endif /* CONFIG_APP_DOOR_FACTORY_TEST */

#ifdef CONFIG_APP_INFRARED_FACTORY_TEST
		COMPONENT_INFRARED,
#endif /* CONFIG_APP_INFRARED_FACTORY_TEST */

#ifdef CONFIG_APP_HEADSET_FACTORY_TEST
		COMPONENT_HEADSET,
#endif /* CONFIG_APP_HEADSET_FACTORY_TEST */

		COMPONENT_NULL,
		COMPONENT_END = COMPONENT_NULL,
	};
	const static char *component_table[] = {

#ifdef CONFIG_APP_AXLE_FACTORY_TEST
		"axle",
#endif /* CONFIG_APP_AXLE_FACTORY_TEST */

#ifdef CONFIG_APP_DOOR_FACTORY_TEST
		"door",
#endif /* CONFIG_APP_DOOR_FACTORY_TEST */

#ifdef CONFIG_APP_INFRARED_FACTORY_TEST
		"infrared",
#endif /* CONFIG_APP_INFRARED_FACTORY_TEST */

#ifdef CONFIG_APP_HEADSET_FACTORY_TEST
		"headset",
#endif /* CONFIG_APP_HEADSET_FACTORY_TEST */

		NULL
	};

	for ( i = COMPONENT_START + 1; i < COMPONENT_END; ++i )
	{
		if ( 0 == strcmp(component, component_table[i]) )
		{
			break;
		}
	}

	switch ( i )
	{
#ifdef CONFIG_APP_AXLE_FACTORY_TEST
		case COMPONENT_AXLE:
			goto axle;
#endif /* CONFIG_APP_AXLE_FACROTY_TEST */

#ifdef CONFIG_APP_DOOR_FACTORY_TEST
		case COMPONENT_DOOR:
			goto door;
#endif /* CONFIG_APP_DOOR_FACTORY_TEST */

#ifdef CONFIG_APP_INFRARED_FACTORY_TEST
		case COMPONENT_INFRARED:
			goto infrared;
#endif /* CONFIG_APP_INFRARED_FACTORY_TEST */

#ifdef CONFIG_APP_HEADSET_FACTORY_TEST
		case COMPONENT_HEADSET:
			goto headset;
#endif /* CONFIG_APP_HEADSET_FACTORY_TEST */
		case COMPONENT_NULL:
		default:
			SYS_LOG_ERR("No supported component finded, conponent: %s", component);
			rc = -EINVAL;
			goto out;
	}

#ifdef CONFIG_APP_AXLE_FACTORY_TEST

axle:
{
	enum operation_axle_enum {
		OPERATION_AXLE_START = 0,
		OPERATION_AXLE_LOCK = OPERATION_AXLE_START,
		OPERATION_AXLE_UNLOCK,
		OPERATION_AXLE_ROTATE_DESC,
		OPERATION_AXLE_ROTATE_ASC,
		OPERATION_AXLE_ROTATE_STOP,
		OPERATION_AXLE_POSITION,
		OPERATION_AXLE_RELOCATION,
		OPERATION_AXLE_ROTATE_TO,
		OPERATION_AXLE_NULL,
		OPERATION_AXLE_END = OPERATION_AXLE_NULL,
	};
	const static char *operation_axle_table[] = {
		"lock",
		"unlock",
		"rotate_desc",
		"rotate_asc",
		"rotate_stop",
		"position",
		"relocation",
		"rotate_to",
		NULL
	};

	for ( i = OPERATION_AXLE_START; i < OPERATION_AXLE_END; ++i )
	{
		if ( 0 == strcmp(operation, operation_axle_table[i]) )
		{
			break;
		}
	}

	switch ( i )
	{
		case OPERATION_AXLE_LOCK:
			rc = axle_ft_lock();
			break;
		case OPERATION_AXLE_UNLOCK:
			rc = axle_ft_unlock();
			break;
		case OPERATION_AXLE_ROTATE_DESC:
			rc = axle_ft_rotate_desc();
			break;
		case OPERATION_AXLE_ROTATE_ASC:
			rc = axle_ft_rotate_asc();
			break;
		case OPERATION_AXLE_ROTATE_STOP:
			rc = axle_ft_rotate_stop();
			break;
		case OPERATION_AXLE_POSITION:
			rc = axle_ft_position();
			json_object_set_number(json_object(root_out), "position", rc);
			rc = 0;
			break;
		case OPERATION_AXLE_RELOCATION:
			rc = axle_ft_relocation();
			break;
		case OPERATION_AXLE_ROTATE_TO:
			if ( !(parameter >= 1 && parameter <= 7) )
			{
				SYS_LOG_ERR("Parameter out of range, parameter: %d", parameter);
				rc = -EINVAL;
				goto out;
			}
			rc = axle_ft_rotate_to(parameter);
			break;
		case OPERATION_AXLE_NULL:
		default:
			SYS_LOG_ERR("No supported operation finded, operation: %s", operation);
			rc = -EINVAL;
			break;
	}

	goto out;
}

#endif /* CONFIG_APP_AXLE_FACTORY_TEST */

#ifdef CONFIG_APP_DOOR_FACTORY_TEST

door:
{
	enum operation_door_enum {
		OPERATION_DOOR_START = 0,
		OPERATION_DOOR_OPEN = OPERATION_DOOR_START,
		OPERATION_DOOR_CLOSE,
		OPERATION_DOOR_STOP,
		OPERATION_DOOR_OPEN_ALL,
		OPERATION_DOOR_CLOSE_ALL,
		OPERATION_DOOR_STOP_ALL,
		OPERATION_DOOR_NULL,
		OPERATION_DOOR_END = OPERATION_DOOR_NULL,
	};
	const static char *operation_door_table[] = {
		"open",
		"close",
		"stop",
		"open_all",
		"close_all",
		"stop_all",
		NULL
	};

	for ( i = OPERATION_DOOR_START; i < OPERATION_DOOR_END; ++i )
	{
		if ( 0 == strcmp(operation, operation_door_table[i]) )
		{
			break;
		}
	}

	switch ( i )
	{
		case OPERATION_DOOR_OPEN:
			/* Boundray check */
			if ( !(parameter >= 1 && parameter <= 4) )
			{
				SYS_LOG_ERR("Parameter out of range, parameter: %d", parameter);
				rc = -EINVAL;
				break;
			}

			rc = door_ft_open(parameter);
			break;
		case OPERATION_DOOR_CLOSE:
			/* Boundray check */
			if ( !(parameter >= 1 && parameter <= 4) )
			{
				SYS_LOG_ERR("Parameter out of range, parameter: %d", parameter);
				rc = -EINVAL;
				break;
			}

			rc = door_ft_close(parameter);
			break;
		case OPERATION_DOOR_STOP:
			/* Boundray check */
			if ( !(parameter >= 1 && parameter <= 4) )
			{
				SYS_LOG_ERR("Parameter out of range, parameter: %d", parameter);
				rc = -EINVAL;
				break;
			}

			rc = door_ft_stop(parameter);
			break;
		case OPERATION_DOOR_OPEN_ALL:
			rc = door_ft_open_all();
			break;
		case OPERATION_DOOR_CLOSE_ALL:
			rc = door_ft_close_all();
			break;
		case OPERATION_DOOR_STOP_ALL:
			rc = door_ft_stop_all();
			break;
		case OPERATION_DOOR_NULL:
		default:
			SYS_LOG_ERR("No supported operation finded, operation: %s", operation);
			rc = -EINVAL;
			break;
	}

	goto out;
}

#endif /* CONFIG_APP_DOOR_FACTORY_TEST */

#ifdef CONFIG_APP_INFRARED_FACTORY_TEST

infrared:
{
	enum operation_infrared_enum {
		OPERATION_INFRARED_START = 0,
		OPERATION_INFRARED_REFRESH = OPERATION_INFRARED_START,
		OPERATION_INFRARED_NULL,
		OPERATION_INFRARED_END = OPERATION_INFRARED_NULL,
	};
	const static char *operation_infrared_table[] = {
		"refresh",
		NULL
	};

	for ( i = OPERATION_INFRARED_START; i < OPERATION_INFRARED_END; ++i )
	{
		if ( 0 == strcmp(operation, operation_infrared_table[i]) )
		{
			break;
		}
	}

	switch ( i )
	{
		case OPERATION_INFRARED_REFRESH:
		{
			int j, k;
			uint8_t *box_status_array = infrared_ft_refresh();

			JSON_Value *box_out		= json_value_init_array();
			JSON_Value *box_in[4]	= {NULL, NULL, NULL, NULL};

			for ( j = 0; j < 4; ++j )
			{
				box_in[i] = json_value_init_array();
				if ( NULL == box_in[i] )
				{
					rc = -EINVAL;
					goto out;
				}

				for ( k = 0; k < 7; ++k )
				{
					json_array_append_number(json_array(box_in[i]),
							box_status_array[j * 7 + k]);
				}
				json_array_append_value(json_array(box_out), box_in[i]);
			}
			json_object_set_value(json_object(root_out), "cabinets", box_out);

			rc = 0;

			break;
		}
		case OPERATION_INFRARED_NULL:
		default:
			SYS_LOG_ERR("No supported operation finded, operation: %s", operation);
			rc = -EINVAL;
			break;
	}

	goto out;
}

#endif /* CONFIG_APP_INFRARED_FACTORY_TEST */

#ifdef CONFIG_APP_HEADSET_FACTORY_TEST

headset:
{
	enum operation_headset_enum {
		OPERATION_HEADSET_START = 0,
		OPERATION_HEADSET_ROTATE = OPERATION_HEADSET_START,
		OPERATION_HEADSET_STOP,
		OPERATION_HEADSET_PUSH,
		OPERATION_HEADSET_INFRARED,
		OPERATION_HEADSET_ACCURACY,
		OPERATION_HEADSET_NULL,
		OPERATION_HEADSET_END = OPERATION_HEADSET_NULL
	};
	const static char *operation_headset_table[] = {
		"rotate",
		"stop",
		"push",
		"infrared",
		"accuracy",
		NULL
	};

	for ( i = OPERATION_HEADSET_START; i < OPERATION_HEADSET_END; ++i )
	{
		if ( 0 == strcmp(operation, operation_headset_table[i]) )
		{
			break;
		}
	}

	switch ( i )
	{
		case OPERATION_HEADSET_ROTATE:
			rc = headset_ft_rotate();
			break;
		case OPERATION_HEADSET_STOP:
			rc = headset_ft_stop();
			break;
		case OPERATION_HEADSET_PUSH:
			rc = headset_ft_push();
			break;
		case OPERATION_HEADSET_INFRARED:
			rc = headset_ft_infrared();
			json_object_set_number(json_object(root_out), "infrared", rc);
			rc = 0;
			break;
		case OPERATION_HEADSET_ACCURACY:
			rc = headset_ft_accuracy();
		case OPERATION_HEADSET_NULL:
		default:
			SYS_LOG_ERR("No supported operation finded, operation: %s", operation);
			rc = -EINVAL;
			break;
	}

	goto out;
}

#endif /* CONFIG_APP_HEADSET_FACTORY_TEST */

out:
	json_object_set_string(json_object(root_out), "cmd", "factory_test");
	json_object_set_string(json_object(root_out), "ext", ext);
	json_object_set_number(json_object(root_out), "error_code", rc);

	json = json_serialize_to_string(root_out);

	mqtt_msg_send(json);

	json_free_serialized_string(json);
}

#endif /* CONFIG_APP_FACTORY_TEST */

int service_cmd_parse(uint8_t *msg, size_t msg_len)
{
	int rc = 0;

	const char *cmd = NULL;
	enum cmd_table_index_e index;

	uint8_t * buff			= NULL;
	JSON_Value * root_in	= NULL;
	JSON_Value * root_out	= NULL;

	/* Local buff */
	buff = k_malloc(msg_len + 1);
	if ( NULL == buff )
	{
		SYS_LOG_ERR("Out of memory at line: %d", __LINE__);
		rc = -ENOMEM;
		goto out;
	}

	/* Copy net buff to local buff */
	memcpy(buff, msg, msg_len);
	buff[msg_len] = 0;

	SYS_LOG_DBG("%s", buff);

	/* Start to parse json, the memory will free at the end of this function */
	root_in = json_parse_string(buff);
	if ( NULL == root_in )
	{
		/* Json parse error */
		SYS_LOG_ERR("Out of memory at line: %d", __LINE__);
		rc = -ENOMEM;
		goto out;
	}

	/* Get "cmd" field string value */
	cmd = json_object_get_string(json_object(root_in), "cmd");
	if ( NULL == cmd )
	{
		SYS_LOG_ERR("Invalid json format, \"cmd\" no found!");
		rc = -EINVAL;
		goto out;
	}

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
		SYS_LOG_ERR("Unknown CMD!");
		rc = -EINVAL;
		goto out;
	}

	/* Out cmd json */
	root_out = json_value_init_object();
	if ( NULL == root_out )
	{
		SYS_LOG_ERR("No memory at line: %d", __LINE__);
		rc = -ENOMEM;
		goto out;
	}

	/* Start to run specific cmd */
	switch ( index )
	{
		case CMD_GET_STATUS:
			run_cmd_get_status(root_in, root_out);
			break;
		case CMD_OPEN:
			run_cmd_open(root_in, root_out);
			break;
		case CMD_CLOSE:
			run_cmd_close(root_in, root_out);
			break;
		case CMD_ADMIN_FETCH:
			run_cmd_admin_fetch(root_in, root_out);
			break;
		case CMD_ADMIN_ROTATE:
			run_cmd_admin_rotate(root_in, root_out);
			break;
		case CMD_ADMIN_CLOSE:
			run_cmd_admin_close(root_in, root_out);
			break;
		case CMD_HEADSET_BUY:
			run_cmd_headset_buy(root_in, root_out);
			break;
		case CMD_HEADSET_ADD:
			run_cmd_headset_add(root_in, root_out);
			break;
		case CMD_HEADSET_RECOUNT:
			run_cmd_headset_recount(root_in, root_out);
			break;
		case CMD_DFU:
			service_dfu(root_in);
			break;
#ifdef CONFIG_APP_FACTORY_TEST
		case CMD_FACTORY_TEST:
			run_cmd_factory_test(root_in, root_out);
			break;
#endif /* CONFIG_APP_FACTORY_TEST */
		default:
			SYS_LOG_ERR("Impossibile index!");
			rc = -EINVAL;
			goto out;
	}
	rc = 0;

out:
	json_value_free(root_out);
	json_value_free(root_in);
	k_free(buff);
	return rc;
}

#ifdef CONFIG_APP_SERVICE_DEBUG

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
			"[1,1,1,1]"
			"[1,1,1,1]"
			"[1,1,1,1]"
			"[1,1,1,1]"
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

#endif /* CONFIG_APP_SERVICE_DEBUG */

