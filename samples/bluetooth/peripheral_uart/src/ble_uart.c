/* ble_uart.c - UART communication over Bluetooth Low Energy */

/*
 * Copyright (c) 2017 Shenzhen Trylong Intelligent Technology Co.,Ltd.
 * 
 * 2017-07-08	Ding Tao	miyatsu@qq.com	Initial Version
 *
 * */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/util.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include "ble_uart.h"

static struct bt_uuid_128 ble_uart_uuid = BT_UUID_INIT_128(
		0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
		0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static struct bt_uuid_128 ble_uart_rx_uuid = BT_UUID_INIT_128(
		0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
		0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static struct bt_uuid_128 ble_uart_tx_uuid = BT_UUID_INIT_128(
		0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
		0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static ssize_t _ble_uart_write_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, u16_t len, u16_t offset, u8_t flags)
{
	ble_uart_write_cb(buf, len);
	return 0;
}

static void ble_uart_ccc_cfg_changed(const struct bt_gatt_attr *attr, u16_t value)
{
	/* Must register an empty callback function for "CCC", other wise will cause a HARD FAULT due to NULL address function called. */
	return ;
}

static struct bt_gatt_ccc_cfg ble_uart_ccc_cfg[/*BT_GATT_CCC_MAX*/2] = {};

static struct bt_gatt_attr ble_uart_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(&ble_uart_uuid),

	BT_GATT_CHARACTERISTIC(&ble_uart_tx_uuid.uuid, BT_GATT_CHRC_NOTIFY),
	BT_GATT_DESCRIPTOR(&ble_uart_tx_uuid.uuid, BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(ble_uart_ccc_cfg, ble_uart_ccc_cfg_changed),

	BT_GATT_CHARACTERISTIC(&ble_uart_rx_uuid.uuid, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP),
	BT_GATT_DESCRIPTOR(&ble_uart_rx_uuid.uuid, BT_GATT_PERM_WRITE, NULL, _ble_uart_write_cb, NULL),
};

static struct bt_gatt_service ble_uart_service = BT_GATT_SERVICE(ble_uart_attrs);

int ble_uart_init()
{
	return bt_gatt_service_register(&ble_uart_service);
}

void ble_uart_send_string(const void *buf, u16_t len)
{
	bt_gatt_notify(NULL, &ble_uart_attrs[2], buf, len);
	return ;
}

void ble_uart_write_cb(const void *buf, u16_t len)
{
	printk("%s called.\n", __func__);
	char str[30] = {0};
	memcpy(str, buf, len);
	printk("%s\n", str);
	ble_uart_send_string(str, strlen(str));
	return ;
}

