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
#include <stdnoreturn.h>

#include <kernel.h>

#ifdef CONFIG_FILE_SYSTEM
#include <fs.h>
#endif /* CONFIG_FILE_SYSTEM */

#include "service.h"
#include "config.h"

#include <logging/sys_log.h>

#ifndef CONFIG_SYS_LOG_EXT_HOOK

#warning	"Must define CONFIG_SYS_LOG_EXT_HOOK to enable " \
			"external hook function for logging"

#else /* CONFIG_SYS_LOG_EXT_HOOK */

typedef struct data_item_s
{
	void *fifo_reserved;
	char *buff;
	size_t buff_size;
}data_item_t __attribute__ ((aligned (4)));

K_FIFO_DEFINE(app_log_hook_dispatch_fifo);

static noreturn void app_log_hook_dispatch_thread_entry_point(void *arg1, void *arg2, void *arg3)
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
			printk("Detected a NULL item entering in FIFO!\n");
			continue;
		}

		/* Send out via network */
		rc = service_send_error_log(item->buff);

		if ( 0 != rc )
		{

#ifdef CONFIG_FILE_SYSTEM
			int app_log_hook_fifo_to_file(const char * buff, size_t buff_len);
			rc = app_log_hook_fifo_to_file(item->buff, item->buff_size);
#endif /* CONFIG_FILE_SYSTEM */

			/* Drop it when no file system support */
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
	char local_buff[256] __attribute__ ((aligned (4)));
	int write_offset = 0;

	data_item_t *item = NULL;

	/**
	 * Alloc this buff on heap, because we don't know how long
	 * the stack of caller was.
	 *
	 * If the caller's stack size is too small to hold such a
	 * "huge" array, may write memory out of range without warning
	 * but crash.
	 * */

	/*
	local_buff = k_malloc(256);
	if ( NULL == local_buff )
	{
		printk("[%s] No memory at line: %d\n", __func__, __LINE__);
		return ;
	}
	*/

	/* Start to parse variadic variables */
	va_list args;

	va_start(args, format);

	write_offset += vsnprintf(&local_buff[write_offset],
			sizeof(local_buff), format, args);

	va_end(args);

	/* Set it as a string, with NULL terminated */
	local_buff[write_offset] = '\0';

	/**
	 * Now the data have all retrieved to local_buff
	 * Add it to FIFO
	 * */

	item = k_malloc(sizeof(data_item_t));
	if ( (uint32_t)item & 0x3 )
	{
		printk("Address not aligned!\n");
	}
	if ( NULL == item )
	{
		/**
		 * DO NOT CALL : "SYS_LOG_ERR"
		 *
		 * This will cause endless recursive and crash the system
		 * */
		printk("[%s] No memory at line: %d\n", __func__, __LINE__);
		goto out;
	}

	item->buff = k_malloc(write_offset + 1);
	if ( NULL == item->buff )
	{
		/**
		 * DO NOT CALL : "SYS_LOG_ERR"
		 *
		 * This will cause endless recursive and crash the system
		 * */
		printk("[%s] No memory at line: %d\n", __func__, __LINE__);
		k_free(item);
		goto out ;
	}

	item->buff_size = write_offset + 1;

	/* Copy data */
	strcpy(item->buff, local_buff);

	/* Add to FIFO */
	k_fifo_put(&app_log_hook_dispatch_fifo, item);

out:
	//k_free(local_buff);
	return ;
}


#ifdef CONFIG_FILE_SYSTEM

/**
 * @brief Save one log message to file
 *
 * @param buff Log message string address
 *
 * @param buff_len Log message string length
 *
 * @return 0 Success
 *		   < 0 Failed
 * */
int app_log_hook_fifo_to_file(const char *buff, size_t buff_len)
{
	int rc = 0;

	fs_file_t file;

	ssize_t n = 0;

	/* Open file */
	rc = fs_open(&file, CONFIG_APP_LOG_HOOK_LOG_FILE_NAME);
	if ( 0 != rc )
	{
		printk("Open file error at line %d, rc = %d\n", __LINE__, rc);
		goto out;
	}

	/* TODO Check the file system capacity */

	/* Move to the last position of this file */
	rc = fs_seek(&file, 0, FS_SEEK_END);
	if ( 0 != rc )
	{
		printk("Seek file position error at line %d, rc = %d\n", __LINE__, rc);
		goto out;
	}

	/* Do write, appened message in the end of file */
	n = fs_write(&file, buff, buff_len);

	/* Check write status */
	if ( n < 0 )
	{
		/* Some error happened */
		rc = -1;
		goto out;
	}
	else if ( n < buff_len )
	{
		/* Disk full */
		rc = -2;
		goto out;
	}
	else if ( n == buff_len )
	{
		/* Write OK */
		rc = 0;
		goto out;
	}

out:
	return rc;
}

/**
 * @brief Retrive log message from file and put into dispatch fifo
 *
 * WARNING: This function can be called ONLY if the network is avaliable.
 *
 * SUGGEST: Use this function right after network is connected or reconnected.
 *
 * @return 0 Success
 *		   <0 Some error happened
 * */
int app_log_hook_file_to_fifo(void)
{
	int rc = 0;

	fs_file_t			file;
	struct fs_dirent	file_state;

	uint8_t				buff[256];

	data_item_t *		item = NULL;
	ssize_t				n;

	/* Get file state */
	rc = fs_stat(CONFIG_APP_LOG_HOOK_LOG_FILE_NAME, &file_state);
	if ( 0 != rc )
	{
		/* File system crashed */
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

	/* Open file */
	rc = fs_open(&file, CONFIG_APP_LOG_HOOK_LOG_FILE_NAME);
	if ( 0 != rc )
	{
		/* File open error */
		goto out;
	}

	/* Polling all message in log file */
	while ( true )
	{
		n = fs_read(&file, buff, 256);
		if ( n == 0 )
		{
			/* No log message in file */
			break;
		}
		else if ( n < 0 )
		{
			/* File read error */
			SYS_LOG_ERR("File read error at line: %d", __LINE__);
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
		memcpy(item->buff, buff, n);

		/* Terminator of string */
		item->buff[n] = '\0';

		/* The length of the string should move one more byte to save NULL */
		item->buff_size = n + 1;

		/* Put current log message into fifo */
		k_fifo_put(&app_log_hook_dispatch_fifo, item);

		/* Reset item to avoid wild pointer */
		item = NULL;
	}

out:
	/**
	 * WARNING!!
	 *
	 * FIXME XXX: Delete file may cause loss of message
	 *
	 * When the consumer get those message without send via networking, the message
	 * will rewrite into the log file. Once this is happened, the message that
	 * rewrite into this file will all be loss.
	 * */

	/* Delete file */
	fs_unlink(CONFIG_APP_LOG_HOOK_LOG_FILE_NAME);

	return rc;
}

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
	int rc = 0;

	syslog_hook_install(app_log_hook_func);

#ifdef CONFIG_FILE_SYSTEM
	rc = app_log_hook_file_to_fifo();
#endif /* CONFIG_FILE_SYSTEM */

	return rc;
}

#ifdef CONFIG_APP_LOG_HOOK_DEBUG

noreturn void app_log_hook_debug(void)
{
	syslog_hook_install(app_log_hook_func);
	while ( 1 )
	{
		SYS_LOG_ERR("01234567890123456789012345678901234567890123456789"
					"01234567890123456789012345678901234567890123456789"
					"01234567890123456789012345678901234567890123456789");
		k_sleep(1000);
	}
}

#endif /* CONFIG_APP_LOG_HOOK_DEBUG */

#endif /* CONFIG_SYS_LOG_EXT_HOOK */

