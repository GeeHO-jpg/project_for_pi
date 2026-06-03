#!/bin/bash

g++ -std=c++17 -Wall \
  hal/src/spi_bus.cpp \
  app/src/app_hal_test.cpp \
  app/src/main.cpp \
  -I . -I hal/include -I app/include \
  -o spi_test

echo "Build done!"
