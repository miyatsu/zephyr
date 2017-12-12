/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file log_hook.c
 *
 * @brief Implementation of system log function external hook function
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 16:45:41 December 12, 2017 GTM+8
 *
 * */
#include <stdio.h>
#include <string.h>

#include <kernel.h>

#ifdef CONFIG_FILE_SYSTEM
#include <fs.h>
#endif /* CONFIG_FILE_SYSTEM */

#include "config.h"

#include <logging/sys_log.h>

#ifndef CONFIG_SYS_LOG_EXT_HOOK

#warning	"Must define CONFIG_SYS_LOG_EXT_HOOK to enable "
			"external hook function for logging"

#else /* CONFIG_SYS_LOG_EXT_HOOK */

typedef struct data_item_s
{
	void *fifo_reserved;
	char *buff;
	size_t buff_size;
}data_item_t;

K_FIFO_DEFINE(app_log_hook_dispatch_fifo);

static void app_log_hook_dispatch_thread_entry_point(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	int rc = 0;
	data_item_t *item = NULL;

	while ( true )
	{
		item = k_fifo_get(&app_log_hook_dispatch_fifo, K_FOREVER);
		if ( NULL == item )
		{
			/* Error handling */
			continue;
		}

		/* Send out via network */
		rc = service_send_error_log(item->buff);
		if ( 0 != rc )
		{
#ifdef CONFIG_FILE_SYSTEM
			int app_log_hook_save_to_file(const char * buff, size_t buff_len);
			rc = app_log_hook_fifo_to_file(item->buff, item->buff_size);
#else
			/* Drop it when no file system support */
#endif /* CONFIG_FILE_SYSTEM */
		}

		k_free(item->buff);
		k_free(item);
	}
}

K_THREAD_DEFINE(app_log_hook_dispatch_tid,
				CONFIG_APP_LOG_HOOK_DISPATCH_THREAD_STACK_SIZE,
				app_log_hook_dispatch_thread_entry_point,
				NULL, NULL, NULL,
				0, 0, K_NO_WAIT);

void app_log_hook_func(const char *format, ...)
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

	item->buff = k_malloc(write_offset + 1);
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

	/* Terminator of string */
	item->buff[write_offset] = '\0';

	/* The length of the string should move one more byte to save NULL */
	item->buff_size = write_offset + 1;

	/* Daa to FIFO */
	k_fifo_put(&app_log_hook_dispatch_fifo, item);
}

#ifdef CONFIG_FILE_SYSTEM

static int app_log_hook_fifo_to_file(const char *buff, size_t buff_len)
{
	int rc = 0;
	fs_file_t file;

	rc = fs_open(&file, CONFIG_APP_LOG_HOOK_LOG_FILE_NAME);
	if ( 0 != rc )
	{
		goto out;
	}

	/* TODO Check the file system capacity */

	rc = fs_write(&file, buff, buff_len);
	if ( rc < 0 )
	{
		/* Error */
	}
	else if ( rc < buff_len )
	{
		/* Disk full */
		rc = -1;
	}
	else if ( rc == buff_len )
	{
		/* Write OK */
		rc = 0;
		goto out;
	}

out:
	return rc;
}

static int app_log_hook_file_to_fifo(void)
{
	int rc = 0;

	fs_file_t			file;
	struct fs_dirent	file_state;

	uint8_t				buff[256];

	data_item_t *		item = NULL;
	ssize_t				n;

	rc = fs_open(&file, CONFIG_APP_LOG_HOOK_LOG_FILE_NAME);
	if ( 0 != rc )
	{
		goto out;
	}

	rc = fs_stat(&file, &file_state);
	if ( 0 != rc )
	{
		goto out;
	}

	/* Check file size */
	if ( 0 == file_state.size )
	{
		/* No log message */
		goto out;
	}

	/**
	 * File not empty, there are some log message in this file.
	 *
	 * Retrive them all, and send them block by block rather than line by line.
	 * */

	while ( true )
	{
		n = fs_read(&file, buff, 256);
		if ( n == 0 )
		{
			break;
		}
		else if ( n < 0 )
		{
			rc = n;
			goto out;
		}

		item = k_malloc(sizeof(data_item_t));
		if ( NULL == item )
		{
			rc = -ENOMEM;
			goto out;
		}

		item->buff = k_malloc(n + 1);
		if ( NULL == item->buff )
		{
			rc = -ENOMEM;
			goto out;
		}

		/* Copy data */
		memcpy(item->buff, buff, n + 1);

		item->buff[n] = '\0';

		item->buff_size = n + 1;

		/* Put current log message into fifo */
		k_fifo_put(&app_log_hook_dispatch_fifo, item);

		/* Reset item to avoid wild pointer */
		item = NULL;
	}

out:
	return rc;
}

#else

#define app_log_hook_file_to_fifo(...)
#define app_log_hook_fifo_to_file(...)

#endif /* CONFIG_FILE_SYSTEM */

/**
 * @brief Initial ext log hook
 *
 * This function will read the remaining log in the file, if the kernel configured
 * to support file system.
 *
 * @return 0
 * */
int app_log_hook_init(void)
{
#ifdef CONFIG_FILE_SYSTEM
	return app_log_hook_file_to_fifo();
#else
	return 0;
#endif /* CONFIG_FILE_SYSTEM */
}

#ifdef CONFIG_APP_LOG_HOOK_DEBUG

_Noreturn void app_log_hook_debug(void)
{
	syslog_hook_install(app_log_hook_func);
	while ( 1 )
	{
		SYS_LOG_ERR("%d, %c, %s\n", 45, 'A', "check");
		k_sleep(1000);
	}
}

#endif /* CONFIG_APP_LOG_HOOK_DEBUG */

#endif /* CONFIG_SYS_LOG_EXT_HOOK */

