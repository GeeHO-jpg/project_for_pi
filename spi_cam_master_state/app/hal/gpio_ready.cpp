#include "gpio_ready.hpp"

#include <stdexcept>
#include <string>
#include <unistd.h>

namespace hal {

GPIOReady::GPIOReady(const char* chip_path, unsigned int line_num)
    : chip_(nullptr), req_(nullptr), offset_(line_num), pin_was_high_(false)
{
    chip_ = gpiod_chip_open(chip_path);
    if (!chip_)
        throw std::runtime_error("GPIOReady: cannot open " + std::string(chip_path));

    gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_RISING);

    gpiod_line_config* line_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg, &offset_, 1, settings);
    gpiod_line_settings_free(settings);

    gpiod_request_config* req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "spi_ready");

    req_ = gpiod_chip_request_lines(chip_, req_cfg, line_cfg);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);

    if (!req_) {
        gpiod_chip_close(chip_);
        throw std::runtime_error("GPIOReady: cannot request line " + std::to_string(line_num));
    }

    // อ่าน level ปัจจุบัน เพื่อ handle กรณี ESP32 HIGH อยู่แล้วตอน Pi เพิ่ง start
    pin_was_high_ = (gpiod_line_request_get_value(req_, offset_) == GPIOD_LINE_VALUE_ACTIVE);
}

GPIOReady::~GPIOReady()
{
    if (req_)  gpiod_line_request_release(req_);
    if (chip_) gpiod_chip_close(chip_);
}

bool GPIOReady::waitReady(int timeout_ms)
{
    // ถ้า pin เป็น HIGH ตอน init → ผ่านเลยไม่รอ (จะเกิดแค่ครั้งแรก)
    if (pin_was_high_) {
        pin_was_high_ = false;
        return true;
    }

    long long timeout_ns = (timeout_ms < 0) ? -1LL
                         : (long long)timeout_ms * 1000000LL;

    int ret = gpiod_line_request_wait_edge_events(req_, timeout_ns);
    if (ret <= 0) return false;

    gpiod_edge_event_buffer* buf = gpiod_edge_event_buffer_new(1);
    gpiod_line_request_read_edge_events(req_, buf, 1);
    gpiod_edge_event* ev = gpiod_edge_event_buffer_get_event(buf, 0);
    bool rising = (gpiod_edge_event_get_event_type(ev) == GPIOD_EDGE_EVENT_RISING_EDGE);
    gpiod_edge_event_buffer_free(buf);
    if (rising) usleep(200);  /* 200 μs: let slave DMA stabilise before clocking */
    return rising;
}

} // namespace hal
