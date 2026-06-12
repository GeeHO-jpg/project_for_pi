#!/bin/bash

set -e

DRIVER="app/driver"
PACKET="${DRIVER}/Packet_RCSA"
RB="${DRIVER}/ring_buffer"
HAL="app/hal"

pkg-config --exists gstreamer-1.0 gstreamer-app-1.0 gstreamer-rtsp-server-1.0 || {
  echo "Missing GStreamer development packages."
  echo "Install them with the apt command in commands.txt, then run ./build.sh again."
  exit 1
}

for plugin in appsrc videoconvert x264enc rtph264pay; do
  gst-inspect-1.0 "$plugin" >/dev/null 2>&1 || {
    echo "Missing GStreamer runtime plugin: $plugin"
    echo "Install the GStreamer packages in commands.txt, then run ./build.sh again."
    exit 1
  }
done

g++ -std=c++17 -Wall -Wextra \
  main.cpp \
  app/app_spi.cpp \
  app/app_state.cpp \
  app/rtsp_streamer.cpp \
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
  -pthread \
  -lgpiod \
  $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 gstreamer-rtsp-server-1.0) \
  -o spi_rtsp

echo "Build done -> sudo ./spi_rtsp"
echo "RTSP URL -> rtsp://<pi-ip>:8554/spi"
echo "Use another port -> SPI_RTSP_PORT=8555 ./spi_rtsp"
