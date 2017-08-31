/*
 * Copyright (c) 2017 Ding Tao <miyatsu@qq.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <pinmux.h>
#include <sys_io.h>

#include <pinmux/stm32/pinmux_stm32.h>

/* pin assignments for STM32F4EXPLORER board */
static const struct pin_config pinconf[] = {
#ifdef CONFIG_UART_STM32_PORT_1
	{STM32_PIN_PA9, STM32F4_PINMUX_FUNC_PA9_USART1_TX},
	{STM32_PIN_PA10, STM32F4_PINMUX_FUNC_PA10_USART1_RX},
#endif	/* CONFIG_UART_STM32_PORT_1 */

#ifdef CONFIG_ETH_STM32_HAL
	{STM32_PIN_PC1, STM32F4_PINMUX_FUNC_PC1_ETH},	/* ETH_MDC */
	{STM32_PIN_PC4, STM32F4_PINMUX_FUNC_PC4_ETH},	/* ETH_RMII_RXD0 */
	{STM32_PIN_PC5, STM32F4_PINMUX_FUNC_PC5_ETH},	/* ETH_RMII_RXD1 */

	{STM32_PIN_PA1, STM32F4_PINMUX_FUNC_PA1_ETH},	/* ETH_RMII_REF_CLK */
	{STM32_PIN_PA2, STM32F4_PINMUX_FUNC_PA2_ETH},	/* ETH_MDIO */
	{STM32_PIN_PA7, STM32F4_PINMUX_FUNC_PA7_ETH},	/* ETH_RMII_CRS_DV */

	{STM32_PIN_PG11, STM32F4_PINMUX_FUNC_PG11_ETH},	/* ETH_RMII_TX_EN */
	{STM32_PIN_PG13, STM32F4_PINMUX_FUNC_PG13_ETH},	/* ETH_RMII_TXD0 */
	{STM32_PIN_PG14, STM32F4_PINMUX_FUNC_PG14_ETH},	/* ETH_RMII_TXD1 */
#endif /* CONFIG_ETH_STM32_HAL */
};

static int pinmux_stm32_init(struct device *port)
{
	ARG_UNUSED(port);

	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	return 0;
}

SYS_INIT(pinmux_stm32_init, PRE_KERNEL_1,
		CONFIG_PINMUX_STM32_DEVICE_INITIALIZATION_PRIORITY);

