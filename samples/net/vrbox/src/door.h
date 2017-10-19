#ifndef DOOR_H
#define DOOR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Door hardware initial function
 *
 * @return  0 Initial OK
 *		   <0 Initial Failed
 * */
int8_t door_init(void);

/**
 * @brief Get door functionality status
 *
 * @return pointer point to door status array
 * */
const bool* door_get_status(void);

/**
 * @brief Public wrapper function of door open
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return -1 Door has never opened
 *			0 Door has fully opened
 *		   -2 Door open timeout, and leave it half open
 * */
int8_t door_open(uint8_t layer);

/**
 * @brief Public wrapper function of door close
 *
 * @param layer Door number, [1-4] is expected
 *
 * @return -3 Door has never close after timeout
 *		   -2 Door has been blocked by something which detected by on
 *			  door infrared detector
 *		   -1 Door close timeout, and leave it half close
 *			0 Door has fully closed
 * */
int8_t door_close(uint8_t layer);

/**
 * @brief Open all four doors at a time
 *
 * @return  0 Door open success
 *		   -1 Door open error
 * */
int8_t door_admin_open(void);

/**
 * @brief Close all four doors at a time
 *
 * @return  0 Door close success
 *		   -1 Door close error
 * */
int8_t door_admin_close(void);

#ifdef CONFIG_APP_DOOR_FACTORY_TEST
int8_t door_factory_test(void);
#endif /* CONFIG_APP_DOOR_FACTORY_TEST */

#ifdef CONFIG_APP_DOOR_DEBUG
void door_debug(void);
#endif /* CONFIG_APP_DOOR_FACTORY_TEST */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DOOR_H */

