# SPI Master Communication — Raspberry Pi 5

โปรเจคนี้พัฒนา SPI Master บน **Raspberry Pi 5** เพื่อส่ง/รับข้อมูลกับ Slave device ผ่าน SPI bus

> **สถานะปัจจุบัน:** พัฒนาและทดสอบโครงสร้างบน PC ก่อน แล้วจึง deploy จริงบน Raspberry Pi 5

---

## เป้าหมาย

- ใช้ Raspberry Pi 5 เป็น SPI Master
- ติดต่อสื่อสารกับ Slave device ผ่าน `/dev/spidev`
- ออกแบบโครงสร้างให้ portable และนำไปใช้บน Pi 5 ได้โดยตรง

---

## โครงสร้างโปรเจค

```
spi_master_comm/
└── hal/
    ├── include/
    │   └── spi_bus.h       # Interface ของ SPIBus class
    └── src/
        └── spi_bus.cpp     # Implementation ผ่าน ioctl + /dev/spidev
```

### Layer Design

| Layer | ไฟล์ | หน้าที่ |
|-------|------|---------|
| HAL   | `hal/spi_bus` | ห่อหุ้ม `/dev/spidev` และ `ioctl` — รู้จักแค่ hardware ไม่รู้จัก protocol |

---

## HAL: SPIBus

### การใช้งานเบื้องต้น

```cpp
#include "hal/include/spi_bus.h"

hal::SPIBus spi("/dev/spidev0.0", 500000); // 500 kHz, mode 0

// ส่ง-รับ 1 byte
uint8_t rx = spi.transferByte(0xAB);

// ส่งหลาย byte
uint8_t tx[] = {0x01, 0x02, 0x03};
uint8_t buf[3]{};
spi.transfer(tx, buf, sizeof(tx));

// ส่งอย่างเดียว
spi.write(tx, sizeof(tx));

// รับอย่างเดียว
spi.read(buf, sizeof(buf));
```

### SPI Mode

| Mode | CPOL | CPHA |
|------|------|------|
| 0    | 0    | 0    |
| 1    | 0    | 1    |
| 2    | 1    | 0    |
| 3    | 1    | 1    |

---

## Hardware Target

| รายการ | รายละเอียด |
|--------|-----------|
| Board  | Raspberry Pi 5 |
| OS     | Raspberry Pi OS (Linux) |
| SPI device | `/dev/spidev0.0` (หรือ `0.1`, `1.0` ตาม config) |
| Speed  | ปรับได้ (เช่น 500 kHz – 10 MHz) |

### เปิดใช้งาน SPI บน Pi 5

```bash
sudo raspi-config
# Interface Options → SPI → Enable
```

หรือเพิ่มในไฟล์ `/boot/firmware/config.txt`:

```
dtparam=spi=on
```

---

## Build (บน Pi 5 หรือ Linux)

```bash
g++ -std=c++17 -o spi_test hal/src/spi_bus.cpp -I hal/include
```

---

## หมายเหตุ

- โครงสร้างทั้งหมดออกแบบสำหรับ Linux (`ioctl`, `spidev`) จึงใช้บน Pi 5 ได้โดยตรง
- การ build/test บน Windows PC ใช้สำหรับตรวจสอบโครงสร้างโค้ดเท่านั้น (ไม่สามารถ run ได้จริงเพราะไม่มี `/dev/spidev`)
