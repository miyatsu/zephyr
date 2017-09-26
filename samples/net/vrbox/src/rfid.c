/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All right reserved.
 *
 * @file rfid.c
 *
 * @brief RFID Reader driver for sharing-vr-box project
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @data 15:21:04 September 22, 2017 GTM+8
 *
 * */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <kernel.h>
#include <device.h>

#include <uart.h>

#ifdef CONFIG_APP_RFID_DEBUG
#include <misc/printk.h>
#endif /* CONFIG_APP_RFID_DEBUG */

/**
 * Received RFIDs will be stored at here.
 *
 * ISO18000-6C(EPC G1G2) standard defined two length of the EPC numbers,
 * 64 bit and 96 bit. Although the length of EPC number is variable,
 * we use 96 bit tag for this project.
 *
 * EPC number is NOT the same as the value in EPC sector.
 *
 * We use EPC number to identify one specific RFID.
 *
 * 96 bit EPC length is 12 byte, but the scan result of EPC-ID will contain
 * a length value, so use 13 byte storage will be perfect.
 *
 * Figure illustrated as follow:
 * --------------------------
 * | Length |	EPC number	|
 * --------------------------
 * | 1 byte |	8 bytes		|
 * --------------------------
 *
 * Note: The length value will always be 0x0C, we store it anyway for either
 * debug usage and error correcting usage.
 * */
static uint8_t rfid_list[4*7*2][13];

/**
 * RFIDs string will be stored at here.
 *
 * We have hex result stored at "rfid_list" above, but the upstream will not
 * accept value like this, we convert the hex into string locally and store
 * at here for JSON lib usage.
 *
 * Note: 13 byte hex convert to string will use at least (13 * 2 + 1) byte,
 * one byte as '\0' at the end of string to end a string.
 * */
char rfid_list_string[4*7*2][27];

/**
 * The format of response shows the response length will not bigger the 256 byte,
 * it use one byte to indicate the rest of the result.
 * */
static uint8_t rfid_cmd_buff[256];

/**
 * @brief Get rfid_list length
 *
 * @return The number of tags that the rfid_list contained
 *
 * */
uint8_t rfid_list_get_length(void)
{
	uint8_t i;
	for ( i = 0; i < 4*7*2; ++i )
	{
		if ( 0 == rfid_list[i][0] )
		{
			break;
		}
	}
	return i;
}

/**
 * @brief Set the rfid_list to Empty
 *
 * @return None
 * */
void rfid_list_set_empty(void)
{
	for ( uint8_t i = 0; i < 4*7*2; ++i )
	{
		/*
		if ( 0 == rfid_list[i][0] )
		{
			//List tail reached
			break;
		}
		*/

		/**
		 * The code above is absolute wrong, when the last item added at the tail
		 * of rfid_list, the length of rfid_list will not be sure if the next position
		 * is not set to zero.
		 *
		 * We comment out that piece of code in case of over optimization in the future.
		 * */
		rfid_list[i][0] = 0;
	}
}

void rfid_list_to_string(void)
{
	uint8_t i, j, len;
	len = rfid_list_get_length();
	for ( i = 0; i < len; ++i )
	{
		for ( j = 0; j < 13; ++j )
		{
			/* Fill ID string every two litter/digital at a time */
			sprintf(&rfid_list_string[i][j*2], "%02X", rfid_list[i][j]);
		}
		/* String terminated */
		rfid_list_string[i][j*2] = '\0';
	}

	/* Set rest of space to empty */
	for (; i < 4*7*2; ++i)
	{
		rfid_list_string[i][0] = '\0';
	}
}

/**
 * @brief Insert tag scan result into rfid_list
 *
 * @param insert_position Index of first empty rfid_list
 *
 * @param buff Scan result Data field
 *
 * @param n Number of scan tags saved at @buff
 *
 * @return None
 * */
void rfid_list_insert(uint8_t insert_position, uint8_t *buff, uint8_t n)
{
	/**
	 *
	 * The @buff point to "Data" field, has following format:
	 *
	 * -----------------------------------------------------------------
	 * |							EPC-IDs							   |
	 * -----------------------------------------------------------------
	 * | EPC-1 Length | EPC-1 Value | EPC-2 Length | EPC-2 Value | ... |
	 * -----------------------------------------------------------------
	 * |    1 byte    |    N byte   |    1 byte    |    N byte   | ... |
	 * -----------------------------------------------------------------
	 *
	 * Note: The "EPC Length" always 0x0C and ONLY indicate the "EPC Value" length,
	 *		 NOT include "EPC Length" it self.
	 *
	 * For Example: Assume the "EPC-1 Length" is 12, then the "EPC-1 Value"
	 *				will be 12 byte long. According to the note above,
	 *				the next "EPC-2 Length" byte position will be "buff + 12 + 1".
	 *
	 * */
	uint8_t i, j, pos;
	uint8_t rfid_list_len = rfid_list_get_length();

	/* N item(s) wait to be added */
	for ( i = 0, pos = 0; i < n; ++i )
	{
		if ( 0x0C != buff[pos] )
		{
			/* The length of EPC number will always 12(0x0C) byte. */
			return ;
		}
		/* Search for duplicate tags */
		for ( j = 0; j < rfid_list_len; ++j )
		{
			/* Use memcmp instead of strcmp, due to non NULL termination letter */
			if ( 0 == memcmp(rfid_list[j], &buff[pos], buff[pos] + 1) )
			{
				/* Duplicate tags */
				break;
			}
		}

		if ( j < rfid_list_len )
		{
			/* Tag already in list, continue insert next tag */
			continue;
		}
		else
		{
			/* Save new tag to list */

			/**
			 * Note: the third param need plus 1 (0x0C + 1 == 13 byte) to save
			 *		 EPC Length field into rfid_list.
			 **/
			memcpy(rfid_list[rfid_list_len++], &buff[pos], buff[pos] + 1);
		}

		/**
		 * Jump to next EPC Length position.
		 *
		 * pos = pos + EPC Length byte + EPC Value byte
		 *
		 * */
		pos = pos + 1 + buff[pos];
	}
}

/**
 *
 * CMD = 0x01 return message format:
 *
 * ----------------------------------------------------------
 * |     |     |       |        |	 Data[]		|	CRC-16	|
 * | Len | Adr | reCmd | Status |---------------|-----------|
 * |     |     |       |        | Num | EPC-IDs | LSB | MSB |
 * ----------------------------------------------------------
 *
 *	  Len: The rest cmd body length, not include Len it self
 *	  Adr: Address of RFID reader
 *	reCmd: Command that the reader response
 * Status: Current command execute status
 *	 Data: For some command may response with data
 * CRC-16: CRC16 check value from Len to Data, append at whole response
 *
 * WARNING: The "Num" field indicate the serial number of the first EPC-ID
 * that this response contained. That means, the number of EPC-ID of this
 * response is NOT identify by this field but Len field. See status 0x03
 * and 0x04 at response parsing function for more detail.
 *
 * EPC-ID format:
 *
 * --------------------------
 * |     PC    |  EPC Value	|
 * |----------------------- |
 * |   1 byte  |   N byte   |
 * --------------------------
 *
 * Note: PC is the length of EPC value, NOT include PC it self.
 * If EPC Value length is 12 byte, the PC value will be 12.
 * More information can be found at function rfid_list_insert.
 *
 * */


/**
 * @brief command CRC16 calculator
 *
 * @param buff The string address wait to calculate crc16
 *
 * @param len The length of buff
 *
 * @return CRC16 result
 * */
uint16_t rfid_cmd_crc16_calculator(uint8_t *buff, uint8_t len)
{
	uint8_t i, j;
	uint16_t crc = 0xFFFF;
	for ( i = 0; i < len; ++i )
	{
		crc = crc ^ buff[i];
		for ( j = 0; j < 8; ++j )
		{
			if ( crc & 0x0001 )
			{
				crc = (crc >> 1) ^ 0x8408;
			}
			else
			{
				crc = (crc >> 1);
			}
		}
	}
	return crc;
}

/**
 * @brief Response cmd crc16 check. Due to little-endian core of STM32F4,
 *		  We can not compare uint16_t directly. Make this helper to do crc16 check.
 * 
 * @param buff Command received from (UART)/(RFID Reader)
 *
 * @param len Command length with length of CRC16 added
 *
 * @return bool true,  check pass
 *				false, check failed
 * */
bool rfid_cmd_crc16_check(uint8_t *buff, uint8_t len)
{
	uint16_t crc = rfid_cmd_crc16_calculator(buff, len - 2);
	return crc == (buff[len - 1] << 8) + (buff[len - 2]);
}

void rfid_send_request(uint8_t *cmd, uint8_t cmd_len)
{
	/**
	 * XXX We can add CRC16 cal at here and send it to rfid reader
	 * so the upper caller will no need to cal CRC16 them self.
	 * */
	struct device *dev = device_get_binding(CONFIG_UART_STM32_PORT_3_NAME);
	for ( uint8_t i = 0; i < cmd_len; ++i )
	{
		uart_poll_out(dev, cmd[i]);
	}
}

int8_t rfid_get_response(uint8_t *buff)
{
	struct device *dev = device_get_binding(CONFIG_UART_STM32_PORT_3_NAME);
	/* Get response length first */
	if ( NULL == buff || 0 != uart_poll_in(dev, &buff[0]) || buff[0] < 3 )
	{
		/* Wrong arg ** No input on uart ** read length less than three */
		return -1;
	}

	/* Get the rest of the response */
	for ( uint8_t i = 1; i <= buff[0]; ++i )
	{
		if ( 0 != uart_poll_in(dev, &buff[i]) )
		{
			return -1;
		}
	}

	/* Check response CRC16 */
	if ( !rfid_cmd_crc16_check(buff, buff[0] + 1) )
	{
		return -1;
	}

	return 0;
}

int rfid_query(void)
{
	uint8_t tags_already_insert = 0;
	uint8_t tags_insert_position = rfid_list_get_length();
	uint8_t cmd[5] = {0x04, 0x00, 0x01, 0xDB, 0x4B};

	memset(rfid_cmd_buff, 0, 256);

	/* Send cmd */
	rfid_send_request(cmd, 5);
read_again:
	/* Get response */
	if ( 0 != rfid_get_response(rfid_cmd_buff) )
	{
		/* Read error, or no scan result */
		return -1;
	}

	if ( rfid_cmd_buff[0] < 12 )
	{
		/* No scan result */
		return -1;
	}

	/**
	 * Status of command response stored in rfid_cmd_buff[3].
	 *
	 * There is four possible value it can be:
	 *
	 * 0x01: Command execute finished, and return execute result.
	 * 0x02: Command execute timeout, abort execution and return execute result.
	 * 0x03: Command execute result can NOT response with one message, there
	 *		 will be one or more message to send.
	 * 0x04: Command execute error with no enough space for reader to store
	 *		 tags it read, and return execute result.
	 *
	 * For example: Let's assume the first response with status code 0x03,
	 *				and its Num is 0x10. That means current response contains
	 *				16 EPC-IDs.
	 *				For the next response, no matter the status code with 0x01,
	 *				0x03 or 0x04. The Num field will be at least 17(0x11).
	 *				Because the Num field is NOT the length of current response
	 *				EPC-IDs but the numbers of EPC-IDs transmited until current
	 *				response arrives.
	 * */

	/**
	 * We use switch/case rather than if/else
	 * for error check extension consideration.
	 * */
	switch ( rfid_cmd_buff[3] )
	{
		case 0x01:	/* OK */
			/* Update readed tag list */
			rfid_list_insert(tags_insert_position,
							 &rfid_cmd_buff[5],
							 rfid_cmd_buff[4] - tags_already_insert);
			/**
			 * Note the third parameter of this function call, we don't know if
			 * current response is the first response we received.
			 *
			 * Use the whole rfid_cmd_buff[4] take away tags_already_insert, we
			 * can get the number of EPC-IDs that this response contains.
			 * */
			return 0;

		case 0x02:	/* Timeout */
			return -1;

		case 0x03:	/* There is more data */
			/* Nothing, same as status code 0x04 */

		case 0x04:	/* Too many tags, not read all tags yet */
			/* Update readed tag list */
			rfid_list_insert(tags_insert_position,
							 &rfid_cmd_buff[5],
							 rfid_cmd_buff[4] - tags_already_insert);

			/* Adjust insert position, to avoid new data overwritten */
			tags_insert_position = rfid_list_get_length();

			/* Update insert numbers */
			tags_already_insert += rfid_cmd_buff[4];

			/* Re-read tags, and drop duplicate tags */
			goto read_again;

		default:
			/* Never happened */
			return -1;
	}
}

void rfid_scan(uint8_t times)
{
	rfid_list_set_empty();
	while ( times-- )
	{
		rfid_query();
		void rfid_cmd_print_head(void);
		void rfid_list_print(void);
		rfid_cmd_print_head();
		//rfid_list_print();
		k_sleep(200);		/* 3ms will be fine, 2ms we will not get packages*/
	}
	rfid_list_to_string();
}

#ifdef CONFIG_APP_RFID_DEBUG

void rfid_list_print(void)
{
	for ( uint8_t i = 0; rfid_list[i][0] != 0; ++i )
	{
		for ( uint8_t j = 0; j < 13; ++j )
		{
			printk("%02X ", rfid_list[i][j]);
		}
		printk("\n");
	}
}

void rfid_cmd_print(void)
{
	printk("rfid_cmd_buff:\n");
	for ( uint8_t i = 0; i < rfid_cmd_buff[0] + 1; ++i )
	{
		printk("%02X ", rfid_cmd_buff[i]);
	}
	printk("\n\n");
}

void rfid_cmd_print_head(void)
{
	printk("response head:");
	for ( uint8_t i = 0; i < 4; ++i )
	{
		printk("%02X ", rfid_cmd_buff[i]);
	}
	printk("%d\n", rfid_cmd_buff[4]);
}

void rfid_list_string_print(void)
{
	for ( uint8_t i = 0; rfid_list_string[i][0] != '\0'; ++i )
	{
		printk("%s\n", rfid_list_string[i]);
	}
}

int rfid_test(void)
{
	while (1)
	{
		rfid_scan(10);
	}
	return 0;
}

#endif /* CONFIG_APP_RFID_DEBUG */

void rfid_init(void)
{
	/**
	 * TODO
	 * Initial RFID reader TX power, bee, timeout duration etc.
	 * */
	return ;
}


