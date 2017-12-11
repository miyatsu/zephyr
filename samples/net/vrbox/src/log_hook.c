#include <stdio.h>

#include <kernel.h>

#include <logging/sys_log.h>

typedef struct data_item_s
{
	void *fifo_reserved;
	char *buff;
	size_t buff_size;
}data_item_t;

K_FIFO_DEFINE(dispatch_fifo);

static void dispatch_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
	data_item_t *item = NULL;

	while ( true )
	{
		item = k_fifo_get(&dispatch_fifo, K_FOREVER);
		if ( NULL == item )
		{
			/* Error handling */
			continue;
		}

		/* Handle message */

		k_free(item->buff);
		k_free(item);
	}
}

K_THREAD_DEFINE(dispatch_tid, 2048,
				dispatch_thread_entry_point, NULL, NULL, NULL,
				0, 0, K_NO_WAIT);

void sys_log_hook_func(const char *format, ...)
{
	char local_buff[512];
	int write_offset = 0;

	/* Start to parse variadic variables */
	va_list args;

	va_start(args, format);

	write_offset += vsnprintf(&local_buff[write_offset],
			sizeof(local_buff), format, args);

	va_end(args);

	/**
	 * Now the data have all retrieved to local_buff
	 * Add it to FIFO
	 * */
	data_item_t *item = k_malloc(sizeof(data_item_t));
	if ( NULL == item )
	{
		/**
		 * DO NOT CALL : "SYS_LOG_ERR"
		 *
		 * This will cause endless recursive and crash the system
		 * */
		return ;
	}

	item->buff_size = write_offset;
	item->buff = k_malloc(item->buff_size);
	if ( NULL == item->buff )
	{
		/**
		 * DO NOT CALL : "SYS_LOG_ERR"
		 *
		 * This will cause endless recursive and crash the system
		 * */
		return ;
	}

	/* Copy data */
	memcpy(item->buff, local_buff, item->buff_size);

	/* Daa to FIFO */
	k_fifo_put(&dispatch_fifo, item);
}

