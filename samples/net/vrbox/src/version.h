/**
 * Copyright (c) 2017 Shenzhen Trylong Intelligence Technology Co., Ltd. All rights reserved.
 *
 * @file version.h
 *
 * @brief Application version macro definition.
 *
 * @author Ding Tao <miyatsu@qq.com>
 *
 * @date 15:00:23 December 19, 2017 GTM+8
 *
 * */
#ifndef VERSION_H
#define VERSION_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CONFIG_APP_DFU_VERSION_MAJOR	0
#define CONFIG_APP_DFU_VERSION_MINOR	0
#define CONFIG_APP_DFU_VERSION_PATCH	2

#define CONFIG_APP_DFU_VERSION_BUILD	6
/**
 * Application version build level,
 * zero means this version is release for production usage,
 * other wise means this firmware current at debug, alpha or beta stage.
 * */

#define CONFIG_APP_DFU_VERSION_NUMBER	\
	( CONFIG_APP_DFU_VERSION_MAJOR << 24 ) ||	\
	( CONFIG_APP_DFU_VERSION_MINOR << 16 ) ||	\
	( CONFIG_APP_DFU_VERSION_PATCH << 8  ) ||	\
	( CONFIG_APP_DFU_VERSION_BUILD )

#if CONFIG_APP_DFU_VERSION_BUILD

#define _CONFIG_APP_DFU_VERSION_STRING(MAJOR, MINOR, PATCH, BUILD)	\
		STRINGIFY(MAJOR) "."	\
		STRINGIFY(MINOR) "."	\
		STRINGIFY(PATCH) "-"	\
		STRINGIFY(BUILD)

#define CONFIG_APP_DFU_VERSION_STRING	_CONFIG_APP_DFU_VERSION_STRING(	\
		CONFIG_APP_DFU_VERSION_MAJOR,	\
		CONFIG_APP_DFU_VERSION_MINOR,	\
		CONFIG_APP_DFU_VERSION_PATCH,	\
		CONFIG_APP_DFU_VERSION_BUILD)

#else	/* CONFIG_APP_DFU_VERSION_BUILD */

#define _CONFIG_APP_DFU_VERSION_STRING(MAJOR, MINOR, PATCH)	\
		STRINGIFY(MAJOR) "."	\
		STRINGIFY(MINOR) "."	\
		STRINGIFY(PATCH)

#define CONFIG_APP_DFU_VERSION_STRING	_CONFIG_APP_DFU_VERSION_STRING(	\
		CONFIG_APP_DFU_VERSION_MAJOR,	\
		CONFIG_APP_DFU_VERSION_MINOR,	\
		CONFIG_APP_DFU_VERSION_PATCH)

#endif	/* CONFIG_APP_DFU_VERSION_BUILD */

#ifdef __cplusplus
}
#endif

#endif /* VERSION_H */

