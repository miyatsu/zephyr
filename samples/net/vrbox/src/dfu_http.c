#include <stdint.h>
#include <errno.h>

#include <http_parser.h>
#include <http.h>

#include <dfu/flash_img.h>

#include "config.h"

struct http_response_user_data
{
	int		rc_flash_img;
	int		rc_http_parse;
	k_sem	sem;
};

static void http_response_cb(struct http_client_ctx *http_ctx, 
		uint8_t *data, size_t buf_len, size_t data_len,
		enum http_final_call data_end, void *user_data_arg)
{
	struct http_response_user_data *http_user_data = user_data_arg;

	uint8_t *	body_start;
	size_t		body_len;

	/* Mark the body position */
	if ( NULL != ctx->rsp.body_start )
	{
		/* This fragment contains http headers */
		body_start	= ctx->rsp.body_start;
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
		ctx->rsp.body_start = data;
	}
	else
	{
		body_start	= data;
		body_len	= data_len;
	}

	rc = flash_img_buffered_write(http_user_data->flash_img_ctx,
					body_start, body_len, data_end);
	if ( 0 != rc )
	{
		/**
		 * Flash write error handle
		 *
		 * Do not exit, wait http transfer complete
		 * */
		http_user_data->rc_flash_img = 0x87654321;
	}

	/* Transfer not complete */
	if ( HTTO_DATA_MORE == data_end )
	{
		/* Just return, wait for next package */
		return ;
	}

	/* Transmision complete */

	/* Check http parse result */
	if ( HPE_OK != ctx->parser.http_errno )
	{
		/**
		 * HTTP format parse error
		 *
		 * We can get error message using:
		 *
		 * (a). const char *http_error_name();
		 * (b). const char *http_error_description();
		 * */
		http_user_data->rc_http_parse = 0x82345678;
	}

	/**
	 * Here we just ignore all other checks.
	 *
	 * Because we don't give a shit about it, we have other
	 * ways to check if the firmware valid or not.
	 * */

	k_sem_give(&http_user_data->sem, 1);
}

static int dfu_get_firmware_via_http(struct http_client_ctx, const char *url)
{
	static const int http_rx_buff_size	= CONFIG_APP_DFU_HTTP_RX_BUFF_SIZE;
	static const int http_tx_timeout	= CONFIG_APP_DFU_HTTP_TX_TIMEOUT;
	static const int http_rx_timeout	= CONFIG_APP_DFU_HTTP_RX_TIMEOUT;

	struct http_client_request *		http_req		= NULL;
	uint8_t *							http_rx_buff	= NULL;
	struct http_response_user_data *	http_user_data	= NULL;
	struct flash_img_context *			flash_img_ctx	= NULL;

	/* HTTP Request structure */
	http_req = k_malloc(sizeof(struct http_client_request));
	if ( NULL == http_req )
	{
		rc = -ENOMEM;
		goto out;
	}

	/* HTTP recive buffer */
	http_rx_buff = k_malloc(http_rx_buff_size);
	if ( NULL == http_rx_buff )
	{
		rc = -ENOMEM;
		goto out;
	}

	/* HTTP user data of response callback */
	http_user_data = k_malloc(sizeof(struct http_response_user_data));
	if ( NULL == http_user_data )
	{
		rc = -ENOMEM;
		goto out;
	}

	/* Flash context write firmware into Slot1 */
	flash_img_ctx = k_malloc(struct flash_img_context);
	if ( NULL == flash_img_ctx )
	{
		rc = -ENOMEM;
		goto out;
	}
	flash_img_init(flash_img_ctx, device_get_binding(CONFIG_FLASH_STM32_DEV_NAME));

	/* Initialize the user data */
	http_user_data->rc_http_parse	= 0;
	http_user_data->rc_flash_img	= 0;
	k_sem_init(http_user_data.sem, 0, 1);

	/* Initialize the HTTP request structure */
	memset(&http_req, 0, sizeof(struct http_client_request));
	http_req->method	= HTTP_GET;
	http_req->url		= url;
	http_req->protocol	= " " HTTP_PROTOCOL HTTP_CRLF;

	/* Get connect to the server */
	rc = http_client_send_req(http_ctx, http_req, http_response_cb,
			http_rx_buff, http_rx_buff_size,
			http_user_date, K_SECONDS(http_tx_timeout));
	if ( rc < 0 && rc != -EINPROGRESS )
	{
		rc = 0x12345678;
		goto out;
	}

	/**
	 * Wait for transmission complete.
	 * In case of irq called by accident,
	 * use two times rx timeout as real timeout param.
	 * */
	if ( 0 == k_sem_take(&user_data->sem, K_SECONDS(http_rx_timeout)) )
	{
		/* Wait http response timeout, or never wait */
		rc = 0x12345678;
	}

	/* HTTP transmission complete, or some error happened */
	http_client_release(http_ctx);

out:
	k_free(user_data);
	k_free(http_rx_buff);
	k_free(http_req);
	k_free(flash_img_ctx);

	return rc;
}

int dfu_parse_url(const char *url_str, size_t url_len)
{
	int rc;
	http_parser_url_init(http_url);
	rc = http_parser_parse_url(url_str, url_len, 0, http_url);
	if ( 0 != rc )
	{
		return rc;
	}
	if ( !( http_url->field_set & (1 << UF_HOST) ) ||
		 !( http_url->field_set & (1 << UF_PATH) ))
	{
		return -1;
	}
	/**
	 * TODO
	 * Parse the schema, use http or https. (Here we default use http)
	 * Parse the host, use ipv4, ipv6 or domain ? (Here we default use ipv4 address)
	 * Recheck port value, is the port value useful ? use port 80, 443 ?
	 * */
	return 0;
}

int dfu_download_firmware(const char *uri, size_t uri_len)
{
	static const uint16_t expected_field_set =
		(1<<UF_SCHEMA) | (1<<UF_HOST) | (1<<UF_PATH);

	int rc;

	struct http_parser_url *http_url = NULL;

	char * filed		= NULL;
	char * field_off	= NULL;
	size_t field_len	= 0;

	/* Alloc uri parser result in heap memory */
	http_url = k_malloc(sizeof(struct http_parser_url));
	if ( NULL == http_url )
	{
		rc = -ENOMEM;
		goto out;
	}

	http_parser_url_init(http_url);

	/* Parse the uri */
	rc = http_parser_parse_url(uri_str, uri_len, 0, http_url);
	if ( 0 != rc )
	{
		rc = 0x0001111;
		goto out;
	}

	/**
	 * The uri must contains three componets:
	 * Schema, host and path.
	 * http://hostname/path
	 * */
	if ( expected_field_set != ( http_url->field_set & expected_field_set ) )
	{
		rc = 0x34234;
		goto out;
	}

	if (  )

	field_off = http_url->field_data[UF_HOST].off;
	field_len = http_url->field_data[UF_HOST].len;

	http_ctx = k_malloc(sizeof(struct http_client_ctx));

	const char *host_str = http_url->field_date[UF_HOST].off;
	size_t host_len = http_url->field_date[UF_HOST].len;
	host_str[host_len] = '\0';

	/**
	 * FIXME We use port 80 for default, waht if the url spec a port ?
	 * */
	rc = http_client_init(http_ctx, host_str, 80);
	if ( 0 != rc )
	{
		k_free(http_url);
		k_free(http_ctx);
		return -1;
	}
	ota_download_firmware(http_url);
	return 0;
out:
	free(http_url);
}

