#ifndef AXLE_H
#define AXLE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

/**
 * @brief Axle hardware initializer
 *
 * @return  0 Succeess
 *		   <0 Some error happened
 * */
int8_t axle_init(void);

/**
 * @brief Rotate the axle to particular position
 *
 * @param destination_position
 *		  The real position we want to go, value [1-7] is expected.
 * @return  0 Success
 *		   <0 Some error happened
 * */
int8_t axle_rotate_to(uint8_t destination_position);

#ifdef CONFIG_APP_AXLE_FACTORY_TEST
int8_t axle_factory_test(void);
#endif /* CONFIG_APP_AXLE_FACTORY_TEST */

#ifdef CONFIG_APP_AXLE_DEBUG
void axle_debug(void);
#endif /* CONFIG_APP_AXLE_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AXLE_H */

