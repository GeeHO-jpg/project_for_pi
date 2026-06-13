#include "app/app_spi.h"
#include "app/rtsp_streamer.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {

constexpr int IMAGE_WIDTH = 320;
constexpr int IMAGE_HEIGHT = 240;
constexpr int TEST_FPS = 15;

void run_rtsp_test_pattern() {
    if (!rtsp_streamer_start(IMAGE_WIDTH, IMAGE_HEIGHT, TEST_FPS, "/spi")) {
        std::fprintf(stderr, "[RTSP] failed to start test pattern stream\n");
        return;
    }

    std::printf("[RTSP] test pattern running on %s\n", rtsp_streamer_url_path());
    std::vector<uint8_t> frame(IMAGE_WIDTH * IMAGE_HEIGHT);
    uint32_t t = 0;

    while (true) {
        for (int y = 0; y < IMAGE_HEIGHT; ++y) {
            for (int x = 0; x < IMAGE_WIDTH; ++x) {
                frame[y * IMAGE_WIDTH + x] = static_cast<uint8_t>((x + y + t) & 0xFF);
            }
        }

        rtsp_streamer_publish_gray8(frame.data(), static_cast<uint32_t>(frame.size()));
        ++t;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / TEST_FPS));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--test-pattern") == 0) {
        run_rtsp_test_pattern();
        return 0;
    }

    app_init();
    while (true)
        app_tick();
    return 0;
}
