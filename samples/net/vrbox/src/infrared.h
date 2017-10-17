#ifndef INFRARED_H
#define INFRARED_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>

/**
 * @brief infrared initial function
 *
 * Some of the gpio need to drain its default input.
 * More detial please refer to the comment within this function.
 * */
void infrared_init(void);

/**
 * @brief Get infrared status array
 *
 * @return pointer of poiner point to infrared_status[4][7]
 * */
bool** infrared_get_status_array(void);

#ifdef CONFIG_APP_INFRARED_DEBUG
void infrared_debug(void);
#endif /* CONFIG_APP_INFRARED_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* INFRARED */

