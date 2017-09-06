#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdbool.h>

enum cmd_type_id
{
	/* in cmd */
	CMD_IN_START = 0,
	CMD_GET_STATUS = CMD_IN_START,
	CMD_BORROW,
	CMD_BACK,
	CMD_ADMIN_FETCH,
	CMD_ADMIN_ROTATE,
	CMD_IN_END = CMD_ADMIN_ROTATE,

	/* out cmd */
	CMD_STATUS,

	CMD_BORROW_OPENING,
	CMD_BORROW_OPENED,
	CMD_BORROW_CLOSING,
	CMD_BORROW_CLOSED,

	CMD_BACK_OPENING,
	CMD_BACK_OPENED,
	CMD_BACK_CLOSING,
	CMD_BACK_CLOSED,

	CMD_INVALID,

	// TODO CMD_ADMIN_*
};

/*******	cmd_out: status/borrow_close/return_close/admin_close		*******/

/**
 *
 * JSON format:
 *
 * {
 *		"cmd": "status",				// See supported command above
 *		"box":
 *		{
 *			round1: [bool, 7 max],		// Identify box empty or not
 *			round2: [bool, 7 max],		// True means empty
 *			round3: [bool, 7 max],		// 7 number per round maxmum
 *			round4: [bool, 7 max]
 *		}
 *		"door": [bool, 4 max],			// Is door ok or not
 *		"axle": bool,					// Is axle ok or not
 *		"vrid": [string, 4*7*2=56 max]	// All vrid in this box
 * }
 *
 * */

struct box
{
	bool round1[8];
	const size_t round1_len;

	bool round2[8];
	const size_t round2_len;

	bool round3[8];
	const size_t round3_len;

	bool round4[8];
	const size_t round4_len;
};

struct cmd_status
{
	char *cmd;
	struct box box;

	bool door[4];
	const size_t door_len;

	bool axle;

	char *vrid[4*8*2];
	const size_t vrid_len;
};

/*******	cmd_in: borrow/return		*******/

/**
 *
 * JSON format:
 *
 * {
 *		"cmd": "borrow",	// See supported command above
 *		"round": x,			// Which door should open, value 1-4
 *		"number": y			// Which position should disk rotate, value 1-7
 * }
 *
 * */

struct cmd_open
{
	char *cmd;
	uint8_t round;
	uint8_t number;
};

/*******	cmd_in_out: get_status/status/invalid	*******/

/**
 *
 * JSON format:
 *
 * {
 *		"cmd": "get_status"	// See supported command above
 * }
 *
 * */

struct cmd_single
{
	char *cmd;
};


#endif	/* CONTROLLER_H */

