#ifndef BLE_UART_H
#define BLE_UART_H

#ifdef __cplusplus
extern "C" {
#endif

int ble_uart_init(void);
void ble_uart_write_cb(const void *buf, u16_t len);
void ble_uart_send_string(const void *buf, u16_t len);

#ifdef __cplusplus
}
#endif	/* __cplusplus */

#endif	/* BLE_UART_H */
