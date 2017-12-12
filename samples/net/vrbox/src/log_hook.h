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

void app_log_hook_func(const char *format, ...);

#ifdef CONFIG_APP_LOG_HOOK_DEBUG
void app_log_hook_debug(void);
#endif /* CONFIG_APP_LOG_HOOK_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LOG_HOOK_H */

