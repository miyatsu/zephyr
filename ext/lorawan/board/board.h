#include <stdint.h>
#include <stdbool.h>

#include <kernel.h>

#include "utilities.h"

#include "timer.h"
#include "delay.h"
#include "radio.h"

#if defined(CONFIG_SX1272)
#include "sx1272.h"
#include "sx1272-board.h"
#endif

#if defined(CONFIG_SX1276)
#include "sx1276.h"
#include "sx1276-board.h"
#endif
