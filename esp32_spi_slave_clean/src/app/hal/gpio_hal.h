#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void gpio_hal_init(void);
void gpio_hal_set_ready(bool high);

#ifdef __cplusplus
}
#endif
