#include <stdio.h>
#include <logging/sys_log.h>

#define CONFIG_APP_LOG_BUFF_SIZE

struct emitter
{
	char *buff;
	int buff_size;
	int len;
};

static int print_to_buff(int c, struct emitter *p)
{
	if ( 0 == p->buff_size || NULL == p->buff )
	{
		/* No buff */
		return EOF;
	}

	if ( p->len )
}

void syslog_hook_mqtt(const char *fmt, ...)
{
	return ;
}

