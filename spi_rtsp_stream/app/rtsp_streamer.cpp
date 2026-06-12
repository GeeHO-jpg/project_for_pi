#include "rtsp_streamer.hpp"

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace {

std::mutex g_mutex;
GstElement* g_appsrc = nullptr;
GMainLoop* g_loop = nullptr;
std::thread g_loop_thread;
std::string g_mount_path = "/spi";
int g_width = 0;
int g_height = 0;
int g_fps = 15;
uint32_t g_frame_size = 0;
bool g_started = false;
uint64_t g_frame_index = 0;

void on_media_configure(GstRTSPMediaFactory*, GstRTSPMedia* media, gpointer) {
    GstElement* element = gst_rtsp_media_get_element(media);
    GstElement* appsrc = gst_bin_get_by_name_recurse_up(GST_BIN(element), "src");
    gst_object_unref(element);

    if (!appsrc) {
        std::fprintf(stderr, "[RTSP] failed to find appsrc in media pipeline\n");
        return;
    }

    g_object_set(G_OBJECT(appsrc),
                 "format", GST_FORMAT_TIME,
                 "is-live", TRUE,
                 "block", FALSE,
                 "do-timestamp", FALSE,
                 nullptr);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_appsrc) {
            gst_object_unref(g_appsrc);
        }
        g_appsrc = GST_ELEMENT(gst_object_ref(appsrc));
        g_frame_index = 0;
    }

    gst_object_unref(appsrc);
    std::printf("[RTSP] media configured\n");
}

std::string make_launch_string() {
    char caps[160];
    std::snprintf(caps, sizeof(caps),
                  "video/x-raw,format=GRAY8,width=%d,height=%d,framerate=%d/1",
                  g_width, g_height, g_fps);

    return "( appsrc name=src is-live=true block=false format=time caps=\"" +
           std::string(caps) +
           "\" ! queue leaky=downstream max-size-buffers=2 "
           "! videoconvert ! video/x-raw,format=I420 "
           "! x264enc tune=zerolatency speed-preset=ultrafast bitrate=800 key-int-max=15 "
           "! rtph264pay name=pay0 pt=96 config-interval=1 )";
}

void print_rtsp_urls() {
    std::printf("[RTSP] stream ready on all interfaces, mount=%s\n", g_mount_path.c_str());
    std::printf("[RTSP] URL template: rtsp://<pi-ip>:8554%s\n", g_mount_path.c_str());

    ifaddrs* addrs = nullptr;
    if (getifaddrs(&addrs) != 0) {
        return;
    }

    for (ifaddrs* it = addrs; it; it = it->ifa_next) {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if ((it->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }

        char ip[INET_ADDRSTRLEN] = {0};
        auto* addr = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
        if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip))) {
            std::printf("[RTSP] URL: rtsp://%s:8554%s (%s)\n",
                        ip, g_mount_path.c_str(), it->ifa_name);
        }
    }

    freeifaddrs(addrs);
}

void clear_appsrc_if_current(GstElement* appsrc) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_appsrc == appsrc) {
        gst_object_unref(g_appsrc);
        g_appsrc = nullptr;
        g_frame_index = 0;
    }
}

} // namespace

bool rtsp_streamer_start(int width, int height, int fps, const char* mount_path) {
    if (g_started) {
        return true;
    }

    g_width = width;
    g_height = height;
    g_fps = fps > 0 ? fps : 15;
    g_frame_size = static_cast<uint32_t>(width * height);
    if (mount_path && mount_path[0] != '\0') {
        g_mount_path = mount_path;
    }

    int argc = 0;
    char** argv = nullptr;
    gst_init(&argc, &argv);

    GstRTSPServer* server = gst_rtsp_server_new();
    gst_rtsp_server_set_address(server, "0.0.0.0");
    gst_rtsp_server_set_service(server, "8554");

    GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(server);
    GstRTSPMediaFactory* factory = gst_rtsp_media_factory_new();
    std::string launch = make_launch_string();

    gst_rtsp_media_factory_set_launch(factory, launch.c_str());
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    g_signal_connect(factory, "media-configure", G_CALLBACK(on_media_configure), nullptr);

    gst_rtsp_mount_points_add_factory(mounts, g_mount_path.c_str(), factory);
    g_object_unref(mounts);

    if (gst_rtsp_server_attach(server, nullptr) == 0) {
        std::fprintf(stderr, "[RTSP] failed to attach server on port 8554\n");
        g_object_unref(server);
        return false;
    }

    g_loop = g_main_loop_new(nullptr, FALSE);
    g_loop_thread = std::thread([] {
        g_main_loop_run(g_loop);
    });
    g_loop_thread.detach();

    g_started = true;
    print_rtsp_urls();
    return true;
}

void rtsp_streamer_publish_gray8(const uint8_t* data, uint32_t size) {
    if (!g_started || !data || size < g_frame_size) {
        return;
    }

    GstElement* appsrc = nullptr;
    uint64_t frame_index = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_appsrc) {
            return;
        }
        appsrc = GST_ELEMENT(gst_object_ref(g_appsrc));
        frame_index = g_frame_index++;
    }

    const GstClockTime duration = gst_util_uint64_scale_int(1, GST_SECOND, g_fps);

    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, g_frame_size, nullptr);
    if (!buffer) {
        gst_object_unref(appsrc);
        return;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        gst_buffer_unref(buffer);
        gst_object_unref(appsrc);
        return;
    }
    std::memcpy(map.data, data, g_frame_size);
    gst_buffer_unmap(buffer, &map);

    GstClockTime pts = frame_index * duration;
    GST_BUFFER_PTS(buffer) = pts;
    GST_BUFFER_DTS(buffer) = pts;
    GST_BUFFER_DURATION(buffer) = duration;

    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
    if (ret != GST_FLOW_OK) {
        std::fprintf(stderr, "[RTSP] push buffer failed: %d\n", ret);
        clear_appsrc_if_current(appsrc);
    }

    gst_object_unref(appsrc);
}

const char* rtsp_streamer_url_path() {
    return g_mount_path.c_str();
}
