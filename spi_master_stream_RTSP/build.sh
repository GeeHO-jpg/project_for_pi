#!/bin/bash

set -e

DRIVER="app/driver"
PACKET="${DRIVER}/Packet_RCSA"
RB="${DRIVER}/ring_buffer"
HAL="app/hal"

# OUTPUT_MODE=OPENCV (default) -> แสดงผลผ่านหน้าต่าง OpenCV (imshow)
# OUTPUT_MODE=RTSP             -> เปิด ffmpeg เป็น subprocess push H.264/RTSP ไปยัง mediamtx
#   RTSP_URL กำหนดปลายทาง (default rtsp://127.0.0.1:8554/spi)
#
# ตัวอย่าง: OUTPUT_MODE=RTSP RTSP_URL=rtsp://127.0.0.1:8554/spi ./build.sh
OUTPUT_MODE="${OUTPUT_MODE:-OPENCV}"
RTSP_URL="${RTSP_URL:-rtsp://127.0.0.1:8554/spi}"

EXTRA_FLAGS=()

if [ "$OUTPUT_MODE" = "RTSP" ]; then
  EXTRA_FLAGS+=(-DOUTPUT_MODE_RTSP "-DRTSP_URL=\"${RTSP_URL}\"")
else
  EXTRA_FLAGS+=($(pkg-config --cflags --libs opencv4))
fi

g++ -std=c++17 -Wall -Wextra \
  main.cpp \
  app/app_spi.cpp \
  app/app_state.cpp \
  app/chunk_manage.c \
  ${DRIVER}/spi_comm.cpp \
  ${PACKET}/CircularBuffer.c \
  ${PACKET}/Commands.c \
  ${PACKET}/crc_32.c \
  ${PACKET}/IDNumber.c \
  ${PACKET}/SerialComm.c \
  ${PACKET}/UDPPacket.c \
  ${PACKET}/UDPPacketHeader.c \
  ${RB}/ring_buffer.c \
  ${HAL}/spi_bus.cpp \
  ${HAL}/gpio_ready.cpp \
  -I . \
  -I app \
  -I app/driver \
  -I app/hal \
  -lgpiod \
  "${EXTRA_FLAGS[@]}" \
  -o spi_stream

echo "Build done -> ./spi_stream (OUTPUT_MODE=${OUTPUT_MODE})"
