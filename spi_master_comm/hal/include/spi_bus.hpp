#pragma once
#include <cstdint>
#include <cstddef>

namespace hal {

/**
 * @brief SPI Bus HAL
 * 
 * ห่อหุ้ม /dev/spidev + ioctl
 * Layer นี้รู้จักแค่ hardware ไม่รู้จัก protocol ใดๆ
 */
class SPIBus {
public:
    /**
     * @brief เปิด SPI device และตั้งค่า
     * @param device   เช่น "/dev/spidev0.0"
     * @param speed_hz ความเร็ว เช่น 500000 (500kHz)
     * @param mode     SPI mode 0-3 (default: 0)
     * @throws std::runtime_error ถ้าเปิดหรือตั้งค่าไม่ได้
     */
    SPIBus(const char* device, uint32_t speed_hz, uint8_t mode = 0);

    ~SPIBus();

    // ไม่อนุญาต copy
    SPIBus(const SPIBus&)            = delete;
    SPIBus& operator=(const SPIBus&) = delete;

    /**
     * @brief ส่งและรับ 1 byte (full duplex)
     * @param tx byte ที่จะส่ง
     * @return   byte ที่รับกลับมา
     */
    uint8_t transferByte(uint8_t tx);

    /**
     * @brief ส่งและรับหลาย byte พร้อมกัน (full duplex)
     * @param tx  buffer ที่จะส่ง
     * @param rx  buffer สำหรับเก็บข้อมูลที่รับ
     * @param len จำนวน byte
     */
    void transfer(const uint8_t* tx, uint8_t* rx, size_t len);

    /**
     * @brief ส่งอย่างเดียว ไม่สนใจ rx
     */
    void write(const uint8_t* data, size_t len);

    /**
     * @brief รับอย่างเดียว (ส่ง 0x00 ไปฝั่ง tx)
     */
    void read(uint8_t* buf, size_t len);

    /**
     * @brief ตรวจสอบว่า device เปิดอยู่หรือเปล่า
     */
    bool isOpen() const;

private:
    int      fd_;
    uint32_t speed_hz_;
    uint8_t  mode_;
    uint8_t  bits_per_word_;

    void configure();
};

} // namespace hal