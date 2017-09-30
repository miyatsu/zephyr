#ifndef INFRARED_H
#define INFRARED_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>

void infrared_init(void);
void infrared_box_update(bool *box);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* INFRARED */
