#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <kernel.h>

#include <net/mqtt.h>
#include <misc/util.h>
#include <misc/printk.h>
#include <json.h>

#include "controller.h"
#include "vrbox_mqtt.h"
#include "axle.h"
#include "door.h"
#include "infrared.h"
#include "config.h"

static const char *cmd_type[] =
{
	/* in cmd */
	"get_status",
	"borrow",
	"back",
	"admin_fetch",
	"admin_rotate",
	"admin_close",

	/* out cmd */
	"status",

	"borrow_opening",
	"borrow_opened",
	"borrow_closing"
	"borrow_closed",

	"back_opening",
	"back_opened",
	"back_closing",
	"back_closed",
  
	/* headset related cmd */
	"headset_ask",
	"headset_buy",

	/* Unexpected */
	"ivalid",

	NULL,
};

/*******	cmd_out: status/borrow_close/back_close/admin_close		*******/

/**
 * @brief Json format descriptor struct use for cmd_out group:
 *		  cmd_status, cmd_borrow_closed, cmd_back_closed, cmd_admin_closed
 *		  cmd_borrow_closing, cmd_back_closing, cmd_admin_closing.
 * */
static const struct json_obj_descr cmd_status_descr[] = 
{
	JSON_OBJ_DESCR_PRIM(struct cmd_status, cmd, JSON_TOK_STRING),
	JSON_OBJ_DESCR_ARRAY(struct cmd_status, box, 4 * 7, box_len, JSON_TOK_TRUE),
	JSON_OBJ_DESCR_ARRAY(struct cmd_status, door, 4, door_len, JSON_TOK_TRUE),
	JSON_OBJ_DESCR_PRIM(struct cmd_status, axle, JSON_TOK_TRUE),
};

/*******	cmd_in: borrow/back	*******/

static const struct json_obj_descr cmd_open_descr[] = 
{
	JSON_OBJ_DESCR_PRIM(struct cmd_open, cmd, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct cmd_open, round, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct cmd_open, number, JSON_TOK_NUMBER)
};

/*******	cmd_out: borrow_opening/back_opening	*******/

static const struct json_obj_descr cmd_opening_descr[] =
{
	JSON_OBJ_DESCR_PRIM(struct cmd_opening, cmd, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct cmd_opening, axle, JSON_TOK_TRUE),
	JSON_OBJ_DESCR_PRIM(struct cmd_opening, door, JSON_TOK_TRUE)
};

/*******	cmd_in_out: get_status/status/invalid	*******/

static const struct json_obj_descr cmd_single_descr[] = 
{
	JSON_OBJ_DESCR_PRIM(struct cmd_single, cmd, JSON_TOK_STRING)
};


static struct cmd_status		cmd_status_struct =
{
	.box_len = 4 * 7,
	.door_len = 4,
};
static struct cmd_open			cmd_open_struct;
static struct cmd_opening		cmd_opening_struct;
static struct cmd_single		cmd_single_struct;

#define GLOBAL_BUFF_SIZE 2048

static char global_buff[GLOBAL_BUFF_SIZE];


/**
 * @brief Send invalid response, "cmd_invalid". Possibile some error happen
 *		  or json encode/decode faild.
 * */
static void send_cmd_invalid(void)
{
	int rc;

	memset(global_buff, 0x00, sizeof(global_buff));
	cmd_single_struct.cmd = cmd_type[CMD_INVALID];

	rc = json_obj_encode_buf(cmd_single_descr, ARRAY_SIZE(cmd_single_descr),
							&cmd_single_struct,
							global_buff, GLOBAL_BUFF_SIZE);
	if ( rc != 0 )
	{
		/* TODO: Internner error handling... */
		return ;
	}

	mqtt_msg_send(global_buff);
}

/**
 * @brief Send current status to controller
 *
 * @parram cmd_index Possibile command lines index, those command includes
 * "status", "borrow_closing", "borrow_closed", "back_closing", "back_closed".
 * Indicate by @cmd_index with different response.
 *
 * */
static void send_cmd_status(enum cmd_type_id cmd_index)
{
	int rc;

	/* Clear json message space */
	memset(global_buff, 0x00, sizeof(global_buff));

	/* Indication of command string */
	cmd_status_struct.cmd = cmd_type[cmd_index];

	/**
	 * Only box status need update, other like door and axle are
	 * MAY modify by other functions.
	 * */
	infrared_box_update(cmd_status_struct.box);

	rc = json_obj_encode_buf(cmd_status_descr, ARRAY_SIZE(cmd_status_descr),
							&cmd_status_struct,
							global_buff, GLOBAL_BUFF_SIZE);

	if ( rc != 0 )
	{
		/* TODO : Internner erro handling... */
		return ;
	}

	mqtt_msg_send(global_buff);
}

/**
 * @brief Send open door command status, is door open normaly ?
 *
 * @param cmd_index CMD_BORROW_OPENING or CMD_BACK_OPENING
 * */
static void send_cmd_opening(enum cmd_type_id cmd_index)
{
	int rc;

	cmd_opening_struct.cmd = cmd_type[cmd_index];

	rc = json_obj_encode_buf(cmd_opening_descr, ARRAY_SIZE(cmd_opening_descr),
							&cmd_opening_struct,
							global_buff, GLOBAL_BUFF_SIZE);

	if ( 0 != rc )
	{
		// TODO
		return ;
	}

	mqtt_msg_send(global_buff);
}

/**
 * @brief Borrow command function wrapper, four of them.
 * */
static void send_cmd_borrow_opening(void)
{
	send_cmd_opening(CMD_BORROW_OPENING);
}

static void send_cmd_borrow_opened(void)
{
	/* May not nessesary */
}

static void send_cmd_borrow_closing(void)
{
	send_cmd_status(CMD_BORROW_CLOSING);
}

static void send_cmd_borrow_closed(void)
{
	send_cmd_status(CMD_BORROW_CLOSED);
}


/**
 * @brief Borrow command function wrapper, four of them.
 * */
static void send_cmd_back_opening(void)
{
	send_cmd_opening(CMD_BACK_OPENING);
}

static void send_cmd_back_opened(void)
{
	/* May not nessesary */
}

static void send_cmd_back_closing(void)
{
	send_cmd_status(CMD_BACK_CLOSING);
}

static void send_cmd_back_closed(void)
{
	send_cmd_status(CMD_BACK_CLOSED);
}


/*******	cmd_in: all		*******/

static void do_cmd_get_status(char *json_msg, uint16_t json_msg_len)
{
	int rc, expected_rc;

	memcpy(global_buff, json_msg, json_msg_len);

	rc = json_obj_parse(global_buff, json_msg_len,
						cmd_single_descr, ARRAY_SIZE(cmd_single_descr),
						&cmd_open_struct);

	expected_rc = ( 1 << ARRAY_SIZE(cmd_single_descr) ) - 1;
	if ( rc != expected_rc
		 || NULL == cmd_single_struct.cmd
		 || 0 != strcmp(cmd_single_struct.cmd, cmd_type[CMD_GET_STATUS]))
	{
		send_cmd_invalid();
		return ;
	}

	send_cmd_status(CMD_STATUS);
}

/**
 * @brief Parse the borrow and back command, and open the door with
 *		  status auto send function
 *
 * @param json_msg, json message string pointer
 *		  json_msg_len, json message length
 *		  cmd_index, expected command type id
 *
 * @return  0, open success
 *		   -1, open error
 * */
static int8_t do_cmd_borrow_back_opening_common(char *json_msg, uint16_t json_msg_len,
										enum cmd_type_id cmd_index)
{
	int rc, expected_rc;
	enum cmd_type_id cmd_in;
	enum cmd_type_id cmd_out;

	if ( cmd_index == CMD_BORROW )
	{
		cmd_in = CMD_BORROW;
		cmd_out = CMD_BORROW_OPENING;
	}
	else
	{
		cmd_in = CMD_BACK;
		cmd_out = CMD_BACK_OPENING;
	}

goto jump_json_parse;
	memset(&cmd_open_struct, 0x00, sizeof(cmd_open_struct));
	memcpy(global_buff, json_msg, json_msg_len);

	rc = json_obj_parse(global_buff, json_msg_len,
						cmd_open_descr, ARRAY_SIZE(cmd_open_descr),
						&cmd_open_struct);
	expected_rc = ( 1 << ARRAY_SIZE(cmd_open_descr) ) - 1;
	if ( rc != expected_rc
		 || 0 != strcmp(cmd_open_struct.cmd, cmd_type[cmd_in])
		 || !( cmd_open_struct.round >= 1 && cmd_open_struct.round <= 4)
		 || !( cmd_open_struct.number >= 1 && cmd_open_struct.round <= 7 )
		 )
	{
		/* Json format parse error */
		send_cmd_invalid();
		return -1;
	}
jump_json_parse:
	cmd_open_struct.round = 1;
	cmd_open_struct.number = 1;
json_parse:
	rc = axle_rotate_to(cmd_open_struct.number);
	if ( 0 != rc )
	{
		/**
		 * Mark opening error, and notify the controller.
		 *
		 * Due to short-circuit operation on if/else, if the axle is false,
		 * no need to calculate the rest of the condition like door.
		 * */

		/* Update axle status */
		cmd_status_struct.axle = cmd_opening_struct.axle = false;

		/* Send current opening status */
		send_cmd_opening(cmd_out);
		return -1;
	}
	else
	{
		cmd_status_struct.axle = cmd_opening_struct.axle = true;
	}

	rc = door_open(cmd_open_struct.round);
	if ( 0 != rc )
	{
		/* Mark door open error, and notify the controller */

		/* Update door status */
		cmd_status_struct.door[cmd_open_struct.round - 1] = cmd_opening_struct.door = false;

		/* Send current opening status */
		send_cmd_opening(cmd_out);
		return -1;
	}
	else
	{
		cmd_status_struct.door[cmd_open_struct.round - 1] = cmd_opening_struct.door = true;
	}

	/**
	 * Until now, the axle is in position, and the door has opened.
	 * The user can take out his cargo in the box.
	 * Let the controller know about it.
	 * */
	send_cmd_opening(cmd_out);
	return 0;
}

static void do_cmd_borrow(uint8_t position, uint8_t layer)
{
	if ( 0 != do_cmd_borrow_back_opening_common(json_msg, json_msg_len, CMD_BORROW) )
	{
		/**
		 * Open door error, close the door without send any message to controller.
		 *
		 * Note: We don't care about wether the door is fully closed, since the
		 * door can not open. We assume the door is already broken even if it
		 * can fully closed.
		 * */
		k_sleep(3000);
		door_close(cmd_open_struct.round);
		return ;
	}

	/**
	 * At this point, the door is opened correct, try to closing the door after
	 * 3 seconds.
	 *
	 * At the mean time, user may alredy take out the vr glass,
	 * */
	k_sleep(3000);
	if ( 0 != door_close(cmd_open_struct.round) )
	{
		/* Try closing door failed */
		cmd_status_struct.door[cmd_open_struct.round - 1] = false;
		send_cmd_borrow_closing();
		return ;
	}

	/* Door fully closed, send status */
	send_cmd_borrow_closed();
}


static void do_cmd_back(char *json_msg, uint16_t json_msg_len)
{
	/* Open the door */
	if ( 0 != do_cmd_borrow_back_opening_common(json_msg, json_msg_len, CMD_BACK) )
	{
		k_sleep(3000);
		door_close(cmd_open_struct.round);
		return ;
	}

	/* Open success, try to close after 3 seconds */
	k_sleep(3000);
	if ( 0 != door_close(cmd_open_struct.round) )
	{
		cmd_status_struct.door[cmd_open_struct.round - 1] = false;
		send_cmd_back_closing();
		return ;
	}

	/* Close success, send status to controller */
	send_cmd_back_closed();
}




/**
 * @brief Borrow and back command opening stage
 *
 * @param position Axle position that need to rotate
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return	0, OK
 *			-1, axle error
 *			-2, door open error, never opened
 *			-3, door open error, timeout and leave it half open
 * */
int8_t do_cmd_borrow_back_comm_open(uint8_t position, uint8_t layer)
{
	int8_t rc;
	rc = axle_rotate_to(position);
	if ( 0 != rc )
	{
		return -2;
	}

	rc = door_open(layer);
	if ( 0 != rc )
	{
		return rc - 1;
	}
}

int8_t do_cmd_borrow_back_comm_close(uint8_t layer)
{
	return door_close(layer);
}



/*************	admin related function	*************/

void do_cmd_admin_fetch(void)
{
	if ( 0 != axle_rotate_to(1) )
	{
		/* TODO Error handling */
		return ;
	}
	if ( 0 != door_admin_open() )
	{
		/* TODO Error handling */
		return ;
	}
	return ;
}

void do_cmd_admin_rotate(void)
{
	if ( 0 != axle_rotate_to_next() )
	{
		/ TODO Error handling /
		return 
	}
	return ;
}

void do_cmd_admin_close(void)
{
	if ( 0 != door_admin_close() )
	{
		/* TODO Error handling */
		return ;
	}
	return ;
}

/*************	headset related function	*************/

void do_cmd_headset_ask(char *json_msg, uint16_t json_msg_len)
{
	return ;
}

void do_cmd_headset_buy(char *json_msg, uint16_t json_msg_len)
{
	return ;
}

/**
 * @brief Parse json message recive from the x86
 *
 * @param json_msg, json message string pointer
 *		  json_msg_len, the length of the json message
 * */
void controller_cmd_parse(char *json_msg, uint16_t json_msg_len)
{
	int rc, expected_rc;
	enum cmd_type_id index;

	memset(&cmd_single_struct, 0x00, sizeof(cmd_single_struct));
	memcpy(global_buff, json_msg, json_msg_len);

	cmd_single_struct.cmd = NULL;
	rc = json_obj_parse(global_buff, json_msg_len,
						cmd_single_descr, ARRAY_SIZE(cmd_single_descr),
						&cmd_single_struct);

	expected_rc = ( 1 << ARRAY_SIZE(cmd_single_descr) ) -1 ;
	if ( rc != expected_rc ||				/* How many obj parsed */
			cmd_single_struct.cmd == NULL )	/* Cmd field parse result */
	{
		/* Json parse error or no specific command find */
		goto invalid;
	}

	/* Ergodic every possible command */
	for ( index = CMD_IN_START; index <= CMD_IN_END; ++index )
	{
		if ( 0 == strcmp(cmd_type[index], cmd_single_struct.cmd) )
		{
			/* Command match */
			break;
		}
	}

	/* Check for command match status */
	if ( index > CMD_IN_END )
	{
		/* No match any possibile command */
		goto invalid;
	}

	/* Start to run specific command */
	switch ( index )
	{
		case CMD_GET_STATUS:
			do_cmd_get_status(json_msg, json_msg_len);
			break;
		case CMD_BORROW:
			do_cmd_borrow(json_msg, json_msg_len);
			break;
		case CMD_BACK:
			do_cmd_back(json_msg, json_msg_len);
			break;
		case CMD_ADMIN_FETCH:
			do_cmd_admin_fetch(json_msg, json_msg_len);
			break;
		case CMD_ADMIN_ROTATE:
			do_cmd_admin_rotate(json_msg, json_msg_len);
			break;
		case CMD_ADMIN_CLOSE:
			do_cmd_admin_close(json_msg, json_msg_len);
			break;
		case CMD_HEADSET_ASK:
			do_cmd_headset_ask(json_msg, json_msg_len);
			break;
		case CMD_HEADSET_BUY:
			do_cmd_headset_buy(json_msg, json_msg_len);
			break;
		default:
			/* Never happend */
			goto invalid;
	}

	/* Run one cmd done */
	return ;

	/* Json parse error or other json format error */
invalid:
	send_cmd_invalid();
}

int8_t controller_init(void)
{
	return 0;
}

#ifdef CONFIG_APP_CONTROLLER_FACTORY_TEST
int8_t contoller_factory_test(void)
{
	return 0;
}
#endif /* CONFIG_APP_CONTROLLER_FACTORY_TEST */

#ifdef CONFIG_APP_CONTROLLER_DEBUG

void controller_debug_1(void)
{
	char json_borrow[] =
	"{\"cmd\":\"borrow\","
		"\"round\":1,"
		"\"number\":1}";
	char json_back[] = "{\"cmd\":\"back\",\"round\":1,\"number\":1}";
	char json_status[] = "{\"cmd\":\"get_status\"}";

	axle_init();
	door_init();
	while (1)
	{
		k_sleep(3000);

		printk("send borrow cmd...\n");
		do_cmd_borrow(json_borrow, sizeof(json_borrow));
		printk("borrow done!\n");

		k_sleep(3000);

		printk("send back cmd...\n");
		do_cmd_back(json_back, sizeof(json_back));
		printk("back done!\n");
	}
	return ;
}

void controller_debug(void)
{
	int rc, expected_rc;
	char json_msg[] = "{\"cmd\":\"get_status\"}";

	memset(&cmd_single_struct, 0x00, sizeof(cmd_single_struct));
	memcpy(global_buff, json_msg, sizeof(json_msg) - 1);
	global_buff[sizeof(json_msg)] = NULL;

	printk("%s\n", global_buff);

	rc = json_obj_parse(global_buff, sizeof(json_msg) - 1,
						cmd_single_descr, ARRAY_SIZE(cmd_single_descr),
						&cmd_single_struct);

	expected_rc = ( 1 << ARRAY_SIZE(cmd_single_descr) ) - 1;
	printk("rc = %d\n", rc);

	cmd_status_struct.cmd = "status";
	rc = json_obj_encode_buf(cmd_status_descr, ARRAY_SIZE(cmd_status_descr),
							&cmd_status_struct,
							global_buff, GLOBAL_BUFF_SIZE);
	printk("%s\nrc = %d\n", global_buff, rc);
	return ;
}

#endif /* CONFIG_APP_CONTROLLER_DEBUG */

