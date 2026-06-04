#pragma once
#include <gpiod.h>

namespace hal {

class GPIOReady {
public:
    // chip: "gpiochip4" บน Pi5
    // line: GPIO number เช่น 22
    GPIOReady(const char* chip, unsigned int line);
    ~GPIOReady();

    GPIOReady(const GPIOReady&)            = delete;
    GPIOReady& operator=(const GPIOReady&) = delete;

    // block จนกว่าจะเห็น rising edge (HIGH)
    // timeout_ms < 0 = รอไม่มีกำหนด
    // return true = ได้ edge, false = timeout
    bool waitReady(int timeout_ms = -1);

private:
    struct gpiod_chip* chip_;
    struct gpiod_line* line_;
};

} // namespace hal
