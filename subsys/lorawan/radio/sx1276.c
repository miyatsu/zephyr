/*
 * Copyright (c) 2018 Ding Tao
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <gpio.h>
#include <spi.h>

#include <radio.h>
#include <sx1276.h>
#include <sx1276-board.h>

#define SYS_LOG_DOMAIN	"LoRaWAN"
#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>

extern DioIrqHandler *DioIrq[];

static struct spi_cs_control spi_cs = {
	.gpio_pin = 4,
	.delay = 0,
};

static struct spi_config spi_config = {
	.frequency = 16000000,
	.operation = SPI_OP_MODE_MASTER | SPI_MODE_CPOL |
		SPI_MODE_CPHA | SPI_WORD_SET(8) | SPI_LINES_SINGLE,
	.slave = 1,
	.cs = &spi_cs,
};

static char *dio_gpio_dev_name_table[6] = {
	CONFIG_SX1276_DIO0_DEV_NAME,
	CONFIG_SX1276_DIO1_DEV_NAME,
	CONFIG_SX1276_DIO2_DEV_NAME,
	CONFIG_SX1276_DIO3_DEV_NAME,
	CONFIG_SX1276_DIO4_DEV_NAME,
	CONFIG_SX1276_DIO5_DEV_NAME,
};

static uint8_t dio_gpio_pin_table[6] = {
	CONFIG_SX1276_DIO0_PIN_NUM,
	CONFIG_SX1276_DIO1_PIN_NUM,
	CONFIG_SX1276_DIO2_PIN_NUM,
	CONFIG_SX1276_DIO3_PIN_NUM,
	CONFIG_SX1276_DIO4_PIN_NUM,
	CONFIG_SX1276_DIO5_PIN_NUM,
};

int bus_spi_init(void)
{
	spi_cs.gpio_dev = device_get_binding("GPIOA");
	if ( NULL == spi_cs.gpio_dev )
	{
		SYS_LOG_ERR("Can not find device GPIOA");
		return -1;
	}

	spi_config.dev = device_get_binding(CONFIG_SX1276_SPI_DEV_NAME);
	if ( NULL == spi_config.dev )
	{
		SYS_LOG_ERR("Can not find device SPI_DEV");
		return -1;
	}

	return 0;
}

void sx1276_io_irq_handler(struct device *dev, struct gpio_callback *gpio_cb, unsigned int pins)
{
	int i;

	uint32_t pin = 0;

	struct device *dev_temp = NULL;

	while ( 1 != pins )
	{
		pins >>= 1;
		pin++;
	}

	for ( i = 0; i < 6; ++i )
	{
		if ( dio_gpio_pin_table[i] == pin )
		{
			dev_temp = device_get_binding(dio_gpio_dev_name_table[i]);
			if ( dev == dev_temp )
			{
				(*DioIrq[i])();
				break;
			}
		}
	}
}

const struct Radio_s Radio =
{
	SX1276Init,
	SX1276GetStatus,
	SX1276SetModem,
	SX1276SetChannel,
	SX1276IsChannelFree,
	SX1276Random,
	SX1276SetRxConfig,
	SX1276SetTxConfig,
	SX1276CheckRfFrequency,
	SX1276GetTimeOnAir,
	SX1276Send,
	SX1276SetSleep,
	SX1276SetStby,
	SX1276SetRx,
	SX1276StartCad,
	SX1276SetTxContinuousWave,
	SX1276ReadRssi,
	SX1276Write,
	SX1276Read,
	SX1276WriteBuffer,
	SX1276ReadBuffer,
	SX1276SetMaxPayloadLength,
	SX1276SetPublicNetwork
};

void SX1276IoIrqInit( DioIrqHandler **irqHandlers )
{
	static struct gpio_callback gpio_cb[6];
	static int gpio_cb_number = 0;

	bool gpio_initialized_table[6] = {
		false, false, false, false, false, false,
	};

	struct device *dev = NULL;

	int i, j, pin_mask;

	for ( i = 0, gpio_cb_number = 0; i < 6; ++i )
	{
		if ( gpio_initialized_table[i] )
		{
			/* Already initialized, skip to next pin */
			continue;
		}

		dev = device_get_binding(dio_gpio_dev_name_table[i]);
		pin_mask = 0;

		/* Get the same gpio device pins into pin_mask */
		for ( j = i; j < 6; ++j )
		{
			if ( 0 == strcmp(dio_gpio_dev_name_table[i], dio_gpio_dev_name_table[j]) )
			{
				gpio_pin_configure(dev, dio_gpio_pin_table[j],
						GPIO_DIR_IN | GPIO_INT | GPIO_INT_DEBOUNCE |	\
						GPIO_PUD_PULL_DOWN | GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH);
				pin_mask |= BIT(dio_gpio_pin_table[j]);

				/* Skip current pin on initialization */
				gpio_initialized_table[j] = true;
			}
		}

		/* Initial callback */
		gpio_init_callback(&gpio_cb[gpio_cb_number], sx1276_io_irq_handler, pin_mask);

		/* Add one particular callback structure */
		gpio_add_callback(dev, &gpio_cb[gpio_cb_number]);

		gpio_cb_number++;
	}
}

bool SX1276CheckRfFrequency( uint32_t frequency )
{
	return true;
}

void SX1276SetAntSwLowPower(bool status)
{
	return ;
}

void SX1276SetAntSw( uint8_t opMode )
{
	return ;
}

void SX1276SetRfTxPower( int8_t power )
{
    uint8_t paConfig = 0;
    uint8_t paDac = 0;

    paConfig = SX1276Read( REG_PACONFIG );
    paDac = SX1276Read( REG_PADAC );

    paConfig = ( paConfig & RF_PACONFIG_PASELECT_MASK ) | SX1276GetPaSelect( SX1276.Settings.Channel );
    paConfig = ( paConfig & RF_PACONFIG_MAX_POWER_MASK ) | 0x70;

    if( ( paConfig & RF_PACONFIG_PASELECT_PABOOST ) == RF_PACONFIG_PASELECT_PABOOST )
    {
        if( power > 17 )
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_ON;
        }
        else
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_OFF;
        }
        if( ( paDac & RF_PADAC_20DBM_ON ) == RF_PADAC_20DBM_ON )
        {
            if( power < 5 )
            {
                power = 5;
            }
            if( power > 20 )
            {
                power = 20;
            }
            paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 5 ) & 0x0F );
        }
        else
        {
            if( power < 2 )
            {
                power = 2;
            }
            if( power > 17 )
            {
                power = 17;
            }
            paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 2 ) & 0x0F );
        }
    }
    else
    {
        if( power < -1 )
        {
            power = -1;
        }
        if( power > 14 )
        {
            power = 14;
        }
        paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power + 1 ) & 0x0F );
    }
    SX1276Write( REG_PACONFIG, paConfig );
    SX1276Write( REG_PADAC, paDac );
}

uint8_t SX1276GetPaSelect( uint32_t channel )
{
    if( channel < RF_MID_BAND_THRESH )
    {
        return RF_PACONFIG_PASELECT_PABOOST;
    }
    else
    {
        return RF_PACONFIG_PASELECT_RFO;
    }
}


void SX1276Reset( void )
{
	struct device *dev = device_get_binding(CONFIG_SX1276_RESET_DEV_NAME);
	if ( NULL == dev )
	{
		return ;
	}

	gpio_pin_configure(dev, CONFIG_SX1276_RESET_PIN_NUM, GPIO_DIR_OUT | GPIO_PUD_NORMAL);
	gpio_pin_write(dev, CONFIG_SX1276_RESET_PIN_NUM, 0);

	k_sleep( 1 );

	gpio_pin_configure(dev, CONFIG_SX1276_RESET_PIN_NUM, GPIO_DIR_IN | GPIO_PUD_NORMAL);
	k_sleep( 6 );
}

void SX1276WriteBuffer( uint8_t addr, uint8_t *buffer, uint8_t size )
{
	addr |= 0x80;

	struct spi_buf tx_bufs[] = {
		{
			.buf = &addr,
			.len = 1,
		},
		{
			.buf = buffer,
			.len = size,
		},
	};

	spi_write(&spi_config, tx_bufs, ARRAY_SIZE(tx_bufs));
}

void SX1276ReadBuffer( uint8_t addr, uint8_t *buffer, uint8_t size )
{
	addr &= 0x7F;

	struct spi_buf tx_bufs[] = {
		{
			.buf = &addr,
			.len = 1,
		},
	};

	spi_write(&spi_config, tx_bufs, ARRAY_SIZE(tx_bufs));

	struct spi_buf rx_bufs[] = {
		{
			.buf = buffer,
			.len = size,
		},
	};

	spi_read(&spi_config, rx_bufs, ARRAY_SIZE(rx_bufs));
}
