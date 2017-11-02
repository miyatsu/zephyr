#ifndef HEADSET_H
#define HEADSET_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>

int8_t headset_buy(void);

#ifdef CONFIG_APP_HEADSET_DEBUG
int8_t headset_debug(void);
#endif /* CONFIG_APP_HEADSET_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HEADSET_H */

