/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file net_app_buff.c
 *
 * @brief net_app buff define
 *
 * Zephyr use net_app as base layer for all kind of application network stack,
 * the net_app support user defined network buff. As for this project, we use
 * more than one application network stack. Both mqtt and http are use the same
 * net_app buff, for code read ability consideration, we define the net_app buff
 * in this file, all other application network can set an alloc callback here.
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 18:17:07 December 6, 2017 GTM+8
 *
 * */
#include <kernel.h>
#include <net/net_pkt.h>

#ifdef CONFIG_NET_CONTEXT_NET_PKT_POOL

NET_PKT_TX_SLAB_DEFINE(tx_slab, 30);
NET_PKT_DATA_POOL_DEFINE(data_pool, 15);

struct k_mem_slab * app_tx_slab(void)
{
	return &tx_slab;
}

struct net_buf_pool * app_data_pool(void)
{
	return &data_pool;
}

#endif /* CONFIG_NET_CONTEXT_NET_PKT_POOL */

