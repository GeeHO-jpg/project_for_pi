#pragma once

#include <cstdint>

bool rtsp_streamer_start(int width, int height, int fps, const char* mount_path);
void rtsp_streamer_publish_gray8(const uint8_t* data, uint32_t size);
const char* rtsp_streamer_url_path();
uint64_t rtsp_streamer_get_push_fail_count();
