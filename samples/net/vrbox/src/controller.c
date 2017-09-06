#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <kernel.h>

#include <misc/util.h>
#include <misc/printk.h>
#include <json.h>

#include "controller.h"

char *cmd_type[] =
{
	/* in cmd */
	"get_status",
	"borrow",
	"back",
	"admin_fetch",
	"admin_rotate",

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

	"ivalid",

	// TODO "cmd_admin_*"
};

/*******	cmd_out: status/borrow_close/return_close/admin_close		*******/

static const struct json_obj_descr box_descr[] =
{
	JSON_OBJ_DESCR_ARRAY(struct box, round1, 7, round1_len, JSON_TOK_TRUE),
	JSON_OBJ_DESCR_ARRAY(struct box, round2, 7, round2_len, JSON_TOK_TRUE),
	JSON_OBJ_DESCR_ARRAY(struct box, round3, 7, round3_len, JSON_TOK_TRUE),
	JSON_OBJ_DESCR_ARRAY(struct box, round4, 7, round4_len, JSON_TOK_TRUE),
};

static const struct json_obj_descr cmd_status_descr[] = 
{
	JSON_OBJ_DESCR_PRIM(struct cmd_status, cmd, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT(struct cmd_status, box, box_descr),
	JSON_OBJ_DESCR_ARRAY(struct cmd_status, door, 4, door_len, JSON_TOK_TRUE),
	JSON_OBJ_DESCR_PRIM(struct cmd_status, axle, JSON_TOK_TRUE),
	JSON_OBJ_DESCR_ARRAY(struct cmd_status, vrid, 28, vrid_len, JSON_TOK_STRING),
};

/*******	cmd_in: borrow/return	*******/

static const struct json_obj_descr cmd_open_descr[] = 
{
	JSON_OBJ_DESCR_PRIM(struct cmd_open, cmd, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct cmd_open, round, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct cmd_open, number, JSON_TOK_NUMBER)
};

/*******	cmd_in_out: get_status/status/invalid	*******/

static const struct json_obj_descr cmd_single_descr[] = 
{
	JSON_OBJ_DESCR_PRIM(struct cmd_single, cmd, JSON_TOK_STRING)
};


struct k_mutex		cmd_status_struct_mutex;
struct cmd_status	cmd_status_struct;
struct cmd_open		cmd_open_struct;
struct cmd_single	cmd_single_struct;

char global_buff[1024];

void controller_init(void)
{
	k_mutex_init(&cmd_status_struct_mutex);
}

void mqtt_json_msg_send(char *buff, uint16_t buff_len)
{
	return ;
}

void send_cmd_invalid(void)
{
	int rc;
	cmd_single_struct.cmd = cmd_type[CMD_INVALID];
	memset(global_buff, sizeof(global_buff), 0);

	rc = json_obj_encode_buf(cmd_single_descr,
							ARRAY_SIZE(cmd_single_descr),
							&cmd_single_struct,
							global_buff,
							1024);
	if ( rc != 0 )
	{
		// TODO: Internner error handling...
		return ;
	}

	mqtt_json_msg_send(global_buff, strlen(global_buff));

	return ;
}

void do_cmd_get_status(char *json_msg, uint16_t json_msg_len)
{
	return ;
}

void do_cmd_borrow(char *json_msg, uint16_t json_msg_len)
{
	return ;
}

void do_cmd_back(char *json_msg, uint16_t json_msg_len)
{
	return ;
}

void do_cmd_admin_fetch(char *json_msg, uint16_t json_msg_len)
{
	return ;
}

void do_cmd_admin_rotate(char *json_msg, uint16_t json_msg_len)
{
	return ;
}



void mqtt_json_msg_parse(char *json_msg, uint16_t json_msg_len)
{
	int rc, expected_rc;
	enum cmd_type_id index;

	memset(&cmd_single_struct, sizeof(cmd_single_struct), 0);

	rc = json_obj_parse(json_msg,
						json_msg_len,
						cmd_single_descr,
						ARRAY_SIZE(cmd_single_descr),
						&cmd_single_struct);
	expected_rc = ( 1 << ARRAY_SIZE(cmd_single_descr) ) -1 ;
	if ( rc != expected_rc ||				/* How many obj parsed */
			cmd_single_struct.cmd == NULL )	/* Cmd field parse result */
	{
		goto invalid;
	}

	for ( index = CMD_IN_START; index <= CMD_IN_END; ++index )
	{
		if ( 0 == strcmp(cmd_type[index], cmd_single_struct.cmd) )
		{
			break;
		}
	}
	if ( index > CMD_IN_END )
	{
		goto invalid;
	}
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
		default:
			/* Never happend */
			goto invalid;
	}

	return ;

invalid:
	send_cmd_invalid();
	return ;
}

void test(void)
{
	struct cmd_status cmd_status_struct = 
	{
		.cmd = "status",
		.box = 
		{
			.round1_len = 7,
			.round2_len = 7,
			.round3_len = 7,
			.round4_len = 7
		},
		.door_len = 4,
		.vrid_len = 28
	};

	char buff[1024] = {'\0'};
	int rc = json_obj_encode_buf(cmd_status_descr, ARRAY_SIZE(cmd_status_descr), &cmd_status_struct, buff, 1024);
	if ( rc != 0 )
	{
		printk("Json_encode failed!\n");
		return ;
	}
	else
	{
		printk("%s\n", buff);
	}


	struct cmd_open cmd_open_struct;
	struct cmd_single cmd_single_struct;
	char buff2[] =
	{
		"{"
		"\"cmd\": \"borrow\","
		"\"round\": 3,"
		"\"number\": 6"
		"}"
	};
	rc = json_obj_parse(buff2, sizeof(buff2) - 1, cmd_single_descr, ARRAY_SIZE(cmd_single_descr), &cmd_single_struct);
	if ( rc != ( 1 << ARRAY_SIZE(cmd_single_descr) ) - 1 )
	{
		printk("Json_decode failed!\n");
	}
	else
	{
		printk("json parse result: cmd = %s\n", cmd_single_struct.cmd);
	}
}

