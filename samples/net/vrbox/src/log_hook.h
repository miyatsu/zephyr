/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file log_hook.h
 *
 * @brief User define hook function API
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 17:04:58 December 12, 2017 GTM+8
 *
 * */
#ifndef LOG_HOOK_H
#define LOG_HOOK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef CONFIG_SYS_LOG_EXT_HOOK

#include <stdnoreturn.h>

void app_log_hook_func(const char *format, ...);
int app_log_hook_init(void);

#ifdef CONFIG_FILE_SYSTEM
int app_log_hook_file_to_fifo(void);
#endif /* CONFIG_FILE_SYSTEM */

#ifdef CONFIG_APP_LOG_HOOK_DEBUG
noreturn void app_log_hook_debug(void);
#endif /* CONFIG_APP_LOG_HOOK_DEBUG */

#endif /* CONFIG_SYS_LOG_EXT_HOOK */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LOG_HOOK_H */

