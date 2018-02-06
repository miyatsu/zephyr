/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file libc_ext.h
 *
 * @brief Head file of missing C standard function strtod() and fabs
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 17:48:33 October 20, 2017 GTM+8
 *
 * */
#ifndef LIBC_EXT_H
#define LIBC_EXT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

double strtod(const char * string, char **endPtr);

#define fabs(n) (((n) > 0) ? ((n)) : (-(n)))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LIBC_EXT_H */

