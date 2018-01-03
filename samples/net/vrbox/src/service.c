#include <stdint.h>
#include <string.h>

#include <kernel.h>

#include <misc/reboot.h>
#include <misc/printk.h>

#include "config.h"
#include "parson.h"

#include "json.h"

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

	"error_log",

	/* Unexpected */
	NULL,
};

static int service_dfu(JSON_Value *root_in)
{
	int rc = 0;

	JSON_Value *root_out	= NULL;
	char *sub_cmd			= NULL;

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
		char *url = NULL;
		char *md5 = NULL;
		int size  = 0;

		char *message = NULL;

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
	printk("[RC = %d]%s\n", rc, json);

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
	/* Warning: DEBUG Modify, MUST check twice before release!!! */
	//json_object_set_number(json_object(root_out), "axle", axle_status ? 1 : 0);
	json_object_set_number(json_object(root_out), "axle", axle_status ? 1 : 1);

	for ( i = 0; i < 4; ++i )
	{
		/* Warning: DEBUG Modify, MUST check twice before release!!! */
		//json_array_append_number(json_array(door), door_status_array[i] ? 1 : 0);
		json_array_append_number(json_array(door), door_status_array[i] ? 1 : 1);
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

out:
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

	if ( 0 == axle_rotate_to(position) )
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

static void run_cmd_headset_add(JSON_Value *root_in, JSON_Value *root_out)
{
	if ( 0 == headset_add() )
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_add_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_add_error");
	}

	out_json_comm(root_in, root_out);
}

static void run_cmd_headset_recount(JSON_Value *root_in, JSON_Value *root_out)
{
	if ( 0 == headset_init() )
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_recount_ok");
	}
	else
	{
		json_object_set_string(json_object(root_out), "cmd", "headset_recount_error");
	}

	out_json_comm(root_in, root_out);
}


int service_cmd_parse(uint8_t *msg, size_t msg_len)
{
	int rc = 0;

	const char *cmd = NULL;
	enum cmd_table_index_e index;

	uint8_t * buff			= NULL;
	JSON_Value * root_in	= NULL;
	JSON_Value * root_out	= NULL;

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
		default:
			SYS_LOG_ERR("Impossibile index!");
			rc = -EINVAL;
	}
	rc = 0;

out:
	json_value_free(root_out);
	json_value_free(root_in);
	k_free(buff);
	return rc;
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

#endif /* CONFIG_APP_JSON_DEBUG */

