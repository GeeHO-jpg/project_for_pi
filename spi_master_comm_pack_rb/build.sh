#!/bin/bash

set -e

PACKET="app/src/Packet_RCSA"
RB="app/src/ring_buffer"

g++ -std=c++17 -Wall -Wextra \
  hal/src/spi_bus.cpp \
  hal/src/gpio_ready.cpp \
  app/src/main.cpp \
  ${PACKET}/CircularBuffer.c \
  ${PACKET}/Commands.c \
  ${PACKET}/crc_32.c \
  ${PACKET}/IDNumber.c \
  ${PACKET}/SerialComm.c \
  ${PACKET}/UDPPacket.c \
  ${PACKET}/UDPPacketHeader.c \
  ${RB}/ring_buffer.c \
  -I . \
  -I app/src \
  -I hal/include \
  -lgpiod \
  -o spi_pack_rb

echo "Build done -> ./spi_pack_rb"
