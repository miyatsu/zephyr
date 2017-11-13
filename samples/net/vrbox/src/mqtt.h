/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file mqtt.h
 *
 * @brief MQTT network API
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 20:37:55 November 4, 2017 GTM+8
 *
 * */
#ifndef MQTT_H
#define MQTT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int net_mqtt_init();

int mqtt_msg_send(const char *buff);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MQTT_H */

