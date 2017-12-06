/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file net_app_buff.h
 *
 * @brief API to set net_app user define net_pkt and net_buf
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 18:27:07 December 6, 2017 GTM+8
 *
 * */
#ifndef NET_APP_BUFF_H
#define NET_APP_BUFF_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct k_mem_slab * app_tx_slab(void);

struct net_buf_pool * app_data_pool(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NET_APP_BUFF_H */

