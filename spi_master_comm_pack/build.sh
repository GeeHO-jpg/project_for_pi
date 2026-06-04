#!/bin/bash

g++ -std=c++17 -Wall \
  hal/src/spi_bus.cpp \
  app/src/app_hal_test.cpp \
  app/src/main.cpp \
  app/src/Packet_RCSA/UDPPacketHeader.c \
  app/src/Packet_RCSA/UDPPacket.c \
  app/src/Packet_RCSA/Commands.c \
  app/src/Packet_RCSA/IDNumber.c \
  app/src/Packet_RCSA/crc_32.c \
  app/src/Packet_RCSA/CircularBuffer.c \
  app/src/Packet_RCSA/SerialComm.c \
  app/src/warp_rb/ring_buffer.c \
  -I . -I hal/include -I app/include -I app/src \
  -o spi_test

echo "Build done!"
