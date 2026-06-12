# SPI RTSP Stream

This program reads complete 320x240 grayscale frames from the SPI chunk protocol
and serves them as an H.264 RTSP stream.

## Endpoint

```text
rtsp://<pi-ip>:8554/spi
```

- Bind address: `0.0.0.0`
- RTSP port: `8554`
- RTSP path: `/spi`
- Video codec: H.264
- Source frame format: `GRAY8`
- Output size: `320x240`
- Nominal frame rate: `15 fps`

When the program starts, it prints detected non-loopback IPv4 URLs:

```text
[RTSP] URL: rtsp://192.168.1.42:8554/spi (wlan0)
```

Use that printed URL from VLC, ffplay, ffprobe, or another RTSP client.

## Build On Raspberry Pi

```bash
sudo apt update
sudo apt install -y build-essential pkg-config ffmpeg libgpiod-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  libgstrtspserver-1.0-dev gstreamer1.0-tools \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly

./build.sh
```

## Test Without SPI Hardware

```bash
./spi_rtsp --test-pattern
```

Then verify from another terminal or client machine:

```bash
ffprobe -rtsp_transport tcp -v error -select_streams v:0 \
  -show_entries stream=codec_name,width,height,avg_frame_rate \
  -of default=nw=1 rtsp://<pi-ip>:8554/spi
```

Expected stream fields include:

```text
codec_name=h264
width=320
height=240
```

## Run With SPI Hardware

```bash
sudo ./spi_rtsp
```

The RTSP server starts before the first SPI frame arrives. Clients can connect
after startup; video begins once complete SPI frames are received.

## Run On Boot

```bash
sudo cp spi_rtsp.service /etc/systemd/system/spi_rtsp.service
sudo systemctl daemon-reload
sudo systemctl enable spi_rtsp.service
sudo systemctl start spi_rtsp.service
```

The service waits for `network-online.target` before starting so the printed
RTSP URL can include the Pi network interface address.
