/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file dfu_http.c
 *
 * @brief Download the firmware via HTTP by a single url
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 15:02:41 December 6, 2017 GTM+8
 *
 * */
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include <net/http_parser.h>
#include <net/http.h>

#include <dfu/flash_img.h>

/*#include "mbedtls/md5.h"*/

#include "net_app_buff.h"
#include "config.h"

/**
 * The net_core.h alredy defined SYS_LOG_DOMAIN, this net_core.h will be
 * include by all net_app related network stack head files include http.h.
 * We want use the orignal SYS_LOG_*, so undefine it
 * */
#undef SYS_LOG_DOMAIN
#undef SYS_LOG_LEVEL

#define SYS_LOG_DOMAIN "dfu_http"
#ifdef CONFIG_APP_DFU_HTTP_DEBUG
	#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_DEBUG
#else
	#define SYS_LOG_LEVEL	SYS_LOG_LEVEL_ERROR
#endif /* CONFIG_APP_DFU_HTTP_DEBUG */
#include <logging/sys_log.h>

struct http_user_data
{
	struct flash_img_context *	flash_img_ctx;
	int							rc_flash_img;
	int							rc_http_parse;
	size_t						http_recv_bytes;
	struct k_sem				sem;
};

static void http_response_cb(struct http_ctx *http_ctx, 
		uint8_t *data, size_t buf_len, size_t data_len,
		enum http_final_call data_end, void *user_data_arg)
{
	int rc = 0;

	/* Use to mark the body position */
	uint8_t *	body_start	= NULL;
	size_t		body_len	= 0;

	/* User data */
	struct http_user_data *http_user_data;

	http_user_data = (struct http_user_data *)user_data_arg;

	if ( NULL != http_ctx->http.rsp.body_start )
	{
		/**
		 * Current fragment contains head field and body field,
		 * the body_start point to the body start position.
		 * */
		body_start	= http_ctx->http.rsp.body_start;
		body_len	= data_len - ( body_start - data );
	}
	else
	{
		/**
		 * If the body_start equal to NULL,
		 * that means the whole fragment will all be body field with no other
		 * field like head.
		 * */
		body_start	= data;
		body_len	= data_len;
	}

	/* Body size counter */
	http_user_data->http_recv_bytes += data_len;

	if ( HTTP_DATA_MORE == data_end )
	{
		rc = flash_img_buffered_write(http_user_data->flash_img_ctx,
					body_start, body_len, 0);
		if ( 0 != rc && 0 == http_user_data->rc_flash_img )
		{
			/* Flash write error, do NOT abort until http transfer complete */
			http_user_data->rc_flash_img = rc;
			SYS_LOG_ERR("Write flash error, return: rc = %d", rc);
		}
		return ;
	}

	/* Last fragment is comming */

	/**
	 * Save the last body to flash
	 *
	 * Note: The last fragment may NOT contian a body, but we still call
	 * this flash_img_buffered_write. If this fragment did not carry a body,
	 * then the body_len will be 0, pass 0 into this function will cause no
	 * effects on write flash.
	 * */
	rc = flash_img_buffered_write(http_user_data->flash_img_ctx,
				body_start, body_len, 1);
	if ( 0 != rc && 0 == http_user_data->rc_flash_img )
	{
		http_user_data->rc_flash_img = rc;
		SYS_LOG_ERR("Write flash error, return: rc = %d", rc);
	}

	/* Check http parse result */
	if ( HPE_OK != http_ctx->http.parser.http_errno )
	{
		http_user_data->rc_http_parse = http_ctx->http.parser.http_errno;
		SYS_LOG_ERR("HTTP parser status: %s",
				http_errno_description(http_ctx->http.parser.http_errno));
	}

	/**
	 * Ignore all other checks, because we don't care about it.
	 * We have other ways to check if the firmware valid or not.
	 * */
	k_sem_give(&http_user_data->sem);
}

static int dfu_get_firmware_via_http(struct http_ctx *http_ctx,
			const char *host_name, const char *path)
{
	int rc = 0;

	struct http_request			http_req = { };
	uint8_t *					http_rx_buff	= NULL;
	struct flash_img_context *	flash_img_ctx	= NULL;
	struct http_user_data		http_user_data;

	http_rx_buff = k_malloc(CONFIG_APP_DFU_HTTP_RX_BUFF_SIZE);
	if ( NULL == http_rx_buff )
	{
		rc = -ENOMEM;
		SYS_LOG_ERR("No memory at line: %d", __LINE__);
		goto out;
	}

	flash_img_ctx = k_malloc(sizeof(struct flash_img_context));
	if ( NULL == flash_img_ctx )
	{
		rc = -ENOMEM;
		SYS_LOG_ERR("No memory at line: %d", __LINE__);
		goto out;
	}

	/* HTTP Request structure */
	http_req.method		= HTTP_GET;
	http_req.url		= path;
	http_req.protocol	= " " HTTP_PROTOCOL;
	http_req.host		= host_name;

	/* Flash context write firmware into Slot1 */
	flash_img_init(flash_img_ctx, device_get_binding(CONFIG_SOC_FLASH_STM32_DEV_NAME));

	/* HTTP user data of response callback */
	http_user_data.flash_img_ctx	= flash_img_ctx;
	http_user_data.rc_flash_img		= 0;
	http_user_data.rc_http_parse	= 0;
	http_user_data.http_recv_bytes	= 0;
	k_sem_init(&http_user_data.sem, 0, 1);

	/* Get connect to the server */
	rc = http_client_send_req(http_ctx, &http_req, http_response_cb,
			http_rx_buff, CONFIG_APP_DFU_HTTP_RX_BUFF_SIZE,
			&http_user_data, K_SECONDS(CONFIG_APP_DFU_HTTP_TX_TIMEOUT_IN_SEC));
	if ( rc < 0 && rc != -EINPROGRESS )
	{
		SYS_LOG_ERR("Can not send request, rc = %d, host_name = %s, path = %s",
				rc, host_name, path);
		goto out;
	}

	/**
	 * Wait for transmission complete.
	 * In case of irq called by accident,
	 * use two times rx timeout as real timeout param.
	 * */
	rc = k_sem_take(&http_user_data.sem,
			K_SECONDS(CONFIG_APP_DFU_HTTP_RX_TIMEOUT_IN_SEC));
	if ( 0 != rc )
	{
		SYS_LOG_ERR("Wait http transmition complete error, rc = %d", rc);
		goto out;
	}

	if ( 0 != http_user_data.rc_http_parse )
	{
		SYS_LOG_ERR("HTTP error, rc_http_parse = %d", http_user_data.rc_http_parse);
		rc = -1;
		goto out;
	}

	if ( 0 != http_user_data.rc_flash_img )
	{
		SYS_LOG_ERR("Flash error, rc_flash_img = %d", http_user_data.rc_flash_img);
		rc = -1;
		goto out;
	}

	if ( http_user_data.http_recv_bytes !=
			flash_img_bytes_written(http_user_data.flash_img_ctx) )
	{
		SYS_LOG_ERR("Error size! http received: %d bytes, flash write: %d bytes",
				http_user_data.http_recv_bytes,
				flash_img_bytes_written(http_user_data.flash_img_ctx));
		rc = -1;
		goto out;
	}

out:
	/* HTTP transmission complete, or some error happened */
	http_release(http_ctx);

	k_free(flash_img_ctx);
	k_free(http_rx_buff);

	return rc;
}

/**
 * @brief Download firmware via HTTP
 *
 * This function will download the firmware using the @uri that caller give.
 * Firmware will automaticly write into image-slot1 and wait to be checked
 * and upgrade from it.
 *
 * @param uri The dowmload url of firmware
 *
 * @param uri_len The length of the download url
 *
 * @return	  0 Download success
 *			< 0 Some error happened, may write flash error
 * */
int dfu_http_download(const char *uri, size_t uri_len)
{
	int rc = 0;

	char * host_name		= NULL;
	size_t host_name_len	= 0;

	char * path				= NULL;
	size_t path_len			= 0;

	size_t port				= 80;

	struct http_parser_url	http_url;
	struct http_ctx *		http_ctx = NULL;

	const uint16_t expected_field_set =
		(1 << UF_SCHEMA) | (1 << UF_HOST) | (1 << UF_PATH);

	http_ctx = k_malloc(sizeof(struct http_ctx));
	if ( NULL == http_ctx )
	{
		rc = -ENOMEM;
		SYS_LOG_ERR("No memory at line: %d", __LINE__);
		goto out;
	}

	/* Initial http url parser */
	http_parser_url_init(&http_url);

	/* Parse the uri */
	rc = http_parser_parse_url(uri, uri_len, 0, &http_url);
	if ( 0 != rc )
	{
		rc = -EINVAL;
		SYS_LOG_ERR("Invalid url, http_parser_parse_url return %d", rc);
		goto out;
	}

	/**
	 * The uri must contains three componets:
	 * Schema, host and path.
	 * http://hostname/path
	 * */
	if ( expected_field_set != ( http_url.field_set & expected_field_set ) )
	{
		rc = -EINVAL;
		SYS_LOG_ERR("http_url->field_set = %d", http_url.field_set);
		goto out;
	}

	/**
	 * TODO Check if we need support https or IPv6 address.
	 * */

	/* Host name */
	host_name_len = http_url.field_data[UF_HOST].len;
	host_name = k_malloc(host_name_len + 1);
	if ( NULL == host_name )
	{
		rc = -ENOMEM;
		SYS_LOG_ERR("No memory at line: %d", __LINE__);
		goto out;
	}

	/* FIXME IPv6 have a "[" at the start of address and "]" at the end of address */
	memcpy(host_name, &uri[ http_url.field_data[UF_HOST].off ], host_name_len);
	host_name[host_name_len] = '\0';

	/* Port */
	if ( 0 != http_url.port )
	{
		port = http_url.port;
	}
	else
	{
		/* Default use 80, no https support yet */
		port = 80;
	}

	/* Start to handle http related ops */

	/* Start to connect */
	rc = http_client_init(http_ctx, host_name, port, NULL,
			K_SECONDS(CONFIG_APP_DFU_HTTP_CONNECTION_TIMEOUT_IN_SEC));
	if ( 0 != rc )
	{
		SYS_LOG_ERR("http_client_init error, rc = %d", rc);
		goto out;
	}

#if defined(CONFIG_NET_CONTEXT_NET_PKT_POOL)
	net_app_set_net_pkt_pool(&http_ctx->app_ctx, app_tx_slab, app_data_pool);
#endif /* CONFIG_NET_CONTEXT_NET_PKT_POOL */

	/* Path name */
	path_len = http_url.field_data[UF_PATH].len;
	path = k_malloc(path_len + 1);
	if ( NULL == path )
	{
		rc = -ENOMEM;
		SYS_LOG_ERR("No memory at line: %d", __LINE__);
		goto out;
	}

	memcpy(path, &uri[ http_url.field_data[UF_PATH].off ], path_len);
	path[path_len] = '\0';

	/* Send request and wait it finish */
	rc = dfu_get_firmware_via_http(http_ctx, host_name, path);

out:
	k_free(path);
	k_free(host_name);
	k_free(http_ctx);

	return rc;
}

/**
 * @brief Check if the downloaded firmware match the given md5 value
 *
 * @param firmware_size The real firmware need be downloaded, use to
 * calculate the local md5 value
 *
 * @param md5_str The real firmware md5 value
 *
 * @return	0 Local md5 value match the remote md5 value
 *			-1 Local md5 value NOT match the remote md5 value
 * */
int dfu_md5_check(size_t firmware_size, const char *md5_str)
{
	int i;

	uint8_t local_md5[16];
	uint8_t local_md5_str[33];
	uint8_t remote_md5_str[33];

	/* @md5_str declared as const, copy its data to local stack */
	for ( i = 0; i < 32; ++i )
	{
		remote_md5_str[i] = md5_str[i];
	}
	remote_md5_str[32] = '\0';

	/**
	 * Some how the <mbedtls/md5.h> no find in include dir,
	 * so we just declar it here.
	 * */
	void mbedtls_md5(const unsigned char *imput, size_t ilen, unsigned char output[16]);

	/* Calculate local md5 in flash */
	mbedtls_md5((const unsigned char *)FLASH_AREA_IMAGE_1_OFFSET,
			firmware_size, local_md5);

	/* Convert remote md5 string all upper case char to lowwer case */
	for ( i = 0; i < 32; ++i )
	{
		if ( remote_md5_str[i] >= 'A' && remote_md5_str[i] <= 'Z' )
		{
			remote_md5_str[i] += 32;
		}
	}

	/* Convert local md5 value to string */
	for ( i = 0; i < 16; ++i )
	{
		sprintf(&local_md5_str[ 2 * i ], "%02x", local_md5[i]);
	}
	local_md5_str[32] = '\0';

	/* Check md5 string */
	if ( 0 != memcmp(local_md5_str, remote_md5_str, 32) )
	{
		/* MD5 not match */
		SYS_LOG_ERR("MD5 chack failed, local md5 = %s, remote md5 = %s\n",
				local_md5_str, remote_md5_str);
		return 0;
	}

	/* MD5 match */
	return -1;
}

#ifdef CONFIG_APP_DFU_HTTP_DEBUG

void dfu_debug(void)
{
	char uri[] = "http://172.16.0.1/screen/index.html";

	dfu_http_download(uri, sizeof(uri) - 1);
	while ( 1 )
	{
		k_sleep(1000);
	}
	return ;
}

#endif /* CONFIG_APP_DFU_HTTP_DEBUG */

