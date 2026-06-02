# Project Notes

## สรุปโปรเจค

**เป้าหมาย:** ใช้ Raspberry Pi 5 เป็น SPI Master รับข้อมูลภาพ Grayscale 320×240 จาก ESP32 (Slave)

- พัฒนาและทดสอบโครงสร้างบน PC ก่อน แล้วค่อย deploy จริงบน Pi5
- มี Data Ready GPIO pin สำหรับให้ ESP32 signal ว่าข้อมูลพร้อม
- ฝั่ง ESP32 slave firmware ยังไม่ได้เริ่มทำ

---

## โครงสร้างปัจจุบัน

```
spi_master_comm/
├── hal/
│   ├── include/spi_bus.h      ← สร้างแล้ว
│   └── src/spi_bus.cpp        ← สร้างแล้ว
└── README.md                  ← สร้างแล้ว
```

## Layer ที่ยังต้องทำ

| Layer | ไฟล์ | หมายเหตุ |
|-------|------|---------|
| Protocol | `protocol/` | มี package ของตัวเองอยู่แล้ว (ซับซ้อน ยังไม่ได้ใส่) |
| Driver | `driver/image_receiver` | จัดการ DATA_READY + รับภาพทั้งเฟรม |
| App | `app/main.cpp` | ทำต่อไป |

---

## แผน Session ถัดไป

สร้าง `app/main.cpp` ส่ง **dummy data** ผ่าน `hal::SPIBus` โดยตรง **ยังไม่ใช้ package จริง** เพื่อทดสอบว่า HAL layer ทำงานได้ถูกต้องก่อน

---

## ข้อมูลอ้างอิง

- ภาพ: Grayscale 320×240 = 76,800 bytes/frame
- CHUNK_SIZE = 512 bytes → 150 packets/frame
- Packet format: `[SOF][TYPE][SEQ(2B)][LEN(2B)][PAYLOAD][CRC16(2B)]`
- [Linux Kernel SPI userspace API](https://docs.kernel.org/spi/spidev.html)
