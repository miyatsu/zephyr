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
#include <stdint.h>
#include <errno.h>

#include <net/http_parser.h>
#include <net/http.h>

#include <dfu/flash_img.h>

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
	struct k_sem				sem;
};

static void http_response_cb(struct http_ctx *http_ctx, 
		uint8_t *data, size_t buf_len, size_t data_len,
		enum http_final_call data_end, void *user_data_arg)
{
	int			rc;
	printk("Message comming: body_start = %d\n", http_ctx->http.rsp.body_start?1:0);
	for ( int i = 0; i < data_len; ++i )
	{
		printk("%c", data[i]);
	}
	printk("\n");
	return ;
	/* Use to mark the body position */
	uint8_t *	body_start;
	size_t		body_len;

	/* User data */
	struct http_user_data *http_user_data;

	http_user_data = (struct http_user_data *)user_data_arg;

	/* Mark the body position */
	if ( NULL != http_ctx->http.rsp.body_start )
	{
		/* This fragment contains http headers */
		body_start	= http_ctx->http.rsp.body_start;
		body_len	= data_len - ( body_start - data );

		/**
		 * In http_client.c, there is no reset ops on body_start
		 * field. We need reset it to the start of buffer. Pull
		 * request is under going at:
		 * https://github.com/zephyrproject-rtos/zephyr/pull/4550
		 *
		 * If this block is running once, the next running will
		 * not contains the http headers. So we just move the
		 * body_start to the very beginning of the recv buffer.
		 * */
		//http_ctx->rsp.body_start = data;
	}
	else
	{
		body_start	= data;
		body_len	= data_len;
	}

	rc = flash_img_buffered_write(http_user_data->flash_img_ctx,
					body_start, body_len, data_end);
	if ( 0 != rc && 0 == http_user_data->rc_flash_img )
	{
		/**
		 * Flash write error handle
		 *
		 * Do not exit, wait http transfer complete
		 * */
		http_user_data->rc_flash_img = rc;
	}

	/* Transfer not complete */
	if ( HTTP_DATA_MORE == data_end )
	{
		/* Just return, wait for next package */
		return ;
	}

	/* Transmision complete */

	/* Check http parse result */
	if ( HPE_OK != http_ctx->http.parser.http_errno )
	{
		/**
		 * HTTP format parse error
		 *
		 * We can get error message using:
		 *
		 * (a). const char *http_error_name();
		 * (b). const char *http_error_description();
		 * */
		http_user_data->rc_http_parse = http_ctx->http.parser.http_errno;
	}

	/**
	 * Here we just ignore all other checks.
	 *
	 * Because we don't give a shit about it, we have other
	 * ways to check if the firmware valid or not.
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

#ifdef CONFIG_APP_DFU_HTTP_DEBUG

void dfu_debug(void)
{
	char uri[] = "http://192.168.0.181/index.html";

	dfu_http_download(uri, sizeof(uri) - 1);
	while ( 1 )
	{
		k_sleep(1000);
	}
	return ;
}

#endif /* CONFIG_APP_DFU_HTTP_DEBUG */

