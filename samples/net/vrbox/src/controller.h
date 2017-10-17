#ifndef CONTROLLER_H
#define CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>

enum cmd_type_id
{
	CMD_START = 0,
	/* in cmd */
	CMD_IN_START = CMD_START,
	CMD_GET_STATUS = CMD_IN_START,
	CMD_BORROW,
	CMD_BACK,
	CMD_ADMIN_FETCH,
	CMD_ADMIN_ROTATE,
	CMD_ADMIN_CLOSE,
	CMD_IN_END = CMD_ADMIN_ROTATE,

	/* out cmd */
	CMD_OUT_START,
	CMD_STATUS = CMD_OUT_START,

	CMD_BORROW_OPENING,
	CMD_BORROW_OPENED,
	CMD_BORROW_CLOSING,
	CMD_BORROW_CLOSED,

	CMD_BACK_OPENING,
	CMD_BACK_OPENED,
	CMD_BACK_CLOSING,
	CMD_BACK_CLOSED,

	// TODO: CMD_ADMIN_*

	CMD_HEADSET_ASK,
	CMD_HEADSET_BUY,

	CMD_INVALID,
	
	CMD_OUT_END = CMD_INVALID,
	CMD_END = CMD_OUT_END,
	CMD_NULL,
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
 * }
 *
 * */
struct cmd_status
{
	char *cmd;

	bool box[4*7];
	const size_t box_len;

	bool door[4];
	const size_t door_len;

	bool axle;
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

/*******	cmd_out: borrow_opening/back_opening	*******/

/**
 *
 * JSON format:
 * {
 *		"cmd": "borrow_opening",
 *		"axle": true,
 *		"door": true
 * }
 *
 * */
struct cmd_opening
{
	char *cmd;
	bool axle;
	bool door;
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


void controller_cmd_parse(char *, uint16_t);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* CONTROLLER_H */

