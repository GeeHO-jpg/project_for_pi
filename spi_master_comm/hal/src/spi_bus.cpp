#include "spi_bus.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include <vector>
#include <stdexcept>
#include <cstring>

namespace hal {

SPIBus::SPIBus(const char* device, uint32_t speed_hz, uint8_t mode)
    : fd_(-1)
    , speed_hz_(speed_hz)
    , mode_(mode)
    , bits_per_word_(8)
{
    fd_ = open(device, O_RDWR);
    if (fd_ < 0)
        throw std::runtime_error("SPIBus: cannot open " + std::string(device));

    configure();
}

SPIBus::~SPIBus() {
    if (fd_ >= 0)
        close(fd_);
}

void SPIBus::configure() {
    if (ioctl(fd_, SPI_IOC_WR_MODE,          &mode_)          < 0 ||
        ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word_) < 0 ||
        ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ,  &speed_hz_)      < 0)
    {
        close(fd_);
        fd_ = -1;
        throw std::runtime_error("SPIBus: failed to configure SPI");
    }
}

bool SPIBus::isOpen() const {
    return fd_ >= 0;
}

uint8_t SPIBus::transferByte(uint8_t tx) {
    uint8_t rx = 0;
    transfer(&tx, &rx, 1);
    return rx;
}

void SPIBus::transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
    struct spi_ioc_transfer tr{};
    tr.tx_buf        = reinterpret_cast<unsigned long>(tx);
    tr.rx_buf        = reinterpret_cast<unsigned long>(rx);
    tr.len           = static_cast<uint32_t>(len);
    tr.speed_hz      = speed_hz_;
    tr.bits_per_word = bits_per_word_;

    if (ioctl(fd_, SPI_IOC_MESSAGE(1), &tr) < 0)
        throw std::runtime_error("SPIBus: transfer failed");
}

void SPIBus::write(const uint8_t* data, size_t len) {
    std::vector<uint8_t> dummy(len, 0x00);
    transfer(data, dummy.data(), len);
}

void SPIBus::read(uint8_t* buf, size_t len) {
    std::vector<uint8_t> dummy(len, 0x00);
    transfer(dummy.data(), buf, len);
}

} // namespace hal