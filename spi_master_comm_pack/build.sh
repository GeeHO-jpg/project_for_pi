#!/bin/bash

set -e

PACKET="app/src/Packet_RCSA"

g++ -std=c++17 -Wall -Wextra \
  hal/src/spi_bus.cpp \
  app/src/main.cpp \
  ${PACKET}/CircularBuffer.c \
  ${PACKET}/Commands.c \
  ${PACKET}/crc_32.c \
  ${PACKET}/IDNumber.c \
  ${PACKET}/SerialComm.c \
  ${PACKET}/UDPPacket.c \
  ${PACKET}/UDPPacketHeader.c \
  -I . \
  -I app/src \
  -I hal/include \
  -o spi_pack_test

echo "Build done -> ./spi_pack_test"
