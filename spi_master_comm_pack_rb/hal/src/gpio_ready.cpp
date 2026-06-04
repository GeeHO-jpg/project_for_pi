#include "gpio_ready.hpp"

#include <stdexcept>
#include <string>

namespace hal {

GPIOReady::GPIOReady(const char* chip, unsigned int line_num)
    : chip_(nullptr), line_(nullptr), pin_was_high_(false)
{
    chip_ = gpiod_chip_open_by_name(chip);
    if (!chip_)
        throw std::runtime_error("GPIOReady: cannot open " + std::string(chip));

    line_ = gpiod_chip_get_line(chip_, line_num);
    if (!line_) {
        gpiod_chip_close(chip_);
        throw std::runtime_error("GPIOReady: cannot get line " + std::to_string(line_num));
    }

    // อ่าน level ปัจจุบันก่อน เพื่อ handle กรณี ESP32 HIGH อยู่แล้วตอน Pi เพิ่ง start
    if (gpiod_line_request_input(line_, "spi_ready") == 0) {
        pin_was_high_ = (gpiod_line_get_value(line_) == 1);
        gpiod_line_release(line_);
    }

    // ขอ rising edge events
    if (gpiod_line_request_rising_edge_events(line_, "spi_ready") < 0) {
        gpiod_chip_close(chip_);
        throw std::runtime_error("GPIOReady: cannot request events on line " + std::to_string(line_num));
    }
}

GPIOReady::~GPIOReady() {
    if (line_) gpiod_line_release(line_);
    if (chip_) gpiod_chip_close(chip_);
}

bool GPIOReady::waitReady(int timeout_ms) {
    // ถ้า pin เป็น HIGH ตอน init → ผ่านเลยไม่รอ (จะเกิดแค่ครั้งแรก)
    if (pin_was_high_) {
        pin_was_high_ = false;
        return true;
    }

    struct timespec  ts;
    struct timespec* pts = nullptr;

    if (timeout_ms >= 0) {
        ts.tv_sec  = timeout_ms / 1000;
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        pts = &ts;
    }

    int ret = gpiod_line_event_wait(line_, pts);
    if (ret <= 0) return false;

    struct gpiod_line_event ev;
    gpiod_line_event_read(line_, &ev);
    return ev.event_type == GPIOD_LINE_EVENT_RISING_EDGE;
}

} // namespace hal
