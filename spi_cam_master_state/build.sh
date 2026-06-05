#!/bin/bash

set -e

DRIVER="app/driver"
PACKET="${DRIVER}/Packet_RCSA"
RB="${DRIVER}/ring_buffer"
HAL="app/hal"
PROTO="app/protocol"

g++ -std=c++17 -Wall -Wextra \
  main.cpp \
  app/app_spi.cpp \
  ${DRIVER}/spi_comm.cpp \
  ${PACKET}/CircularBuffer.c \
  ${PACKET}/Commands.c \
  ${PACKET}/crc_32.c \
  ${PACKET}/IDNumber.c \
  ${PACKET}/SerialComm.c \
  ${PACKET}/UDPPacket.c \
  ${PACKET}/UDPPacketHeader.c \
  ${RB}/ring_buffer.c \
  ${PROTO}/spi_cmd.c \
  ${PROTO}/frame_assembler.c \
  ${HAL}/spi_bus.cpp \
  ${HAL}/gpio_ready.cpp \
  -I . \
  -I app \
  -I app/driver \
  -I app/hal \
  -I app/protocol \
  -lgpiod \
  -o spi_cam_master

echo "Build done -> ./spi_cam_master"
