#include "gpio_hal.h"
#include <Arduino.h>
#include "config.hpp"

void gpio_hal_init(void) {
    pinMode(PIN_DATA_READY, OUTPUT);
    digitalWrite(PIN_DATA_READY, LOW);
}

void gpio_hal_set_ready(bool high) {
    digitalWrite(PIN_DATA_READY, high ? HIGH : LOW);
}
