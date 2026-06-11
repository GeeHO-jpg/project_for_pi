#pragma once
#include <cstdint>
#include <cstddef>

namespace hal {

class SPIBus {
public:
    SPIBus(const char* device, uint32_t speed_hz, uint8_t mode = 0);
    ~SPIBus();

    SPIBus(const SPIBus&)            = delete;
    SPIBus& operator=(const SPIBus&) = delete;

    uint8_t transferByte(uint8_t tx);
    void transfer(const uint8_t* tx, uint8_t* rx, size_t len);
    void write(const uint8_t* data, size_t len);
    void read(uint8_t* buf, size_t len);
    bool isOpen() const;

private:
    int      fd_;
    uint32_t speed_hz_;
    uint8_t  mode_;
    uint8_t  bits_per_word_;

    void configure();
};

} // namespace hal
