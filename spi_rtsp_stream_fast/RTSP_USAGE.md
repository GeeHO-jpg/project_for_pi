# SPI RTSP Stream

This program reads complete 320x240 grayscale frames from the SPI chunk protocol
and serves them as an H.264 RTSP stream.

## Endpoint

```text
rtsp://<pi-ip>:8555/spi
```

- Bind address: `0.0.0.0`
- Default RTSP port: `8555`
- Override with environment variable: `SPI_RTSP_PORT`
- RTSP path: `/spi`
- Video codec: H.264
- Source frame format: `GRAY8`
- Output size: `320x240`
- Nominal frame rate: `15 fps`

When the program starts, it prints detected non-loopback IPv4 URLs:

```text
[RTSP] URL: rtsp://192.168.1.42:8555/spi (wlan0)
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

If another service already uses port `8555`, run on another RTSP port:

```bash
SPI_RTSP_PORT=8554 ./spi_rtsp --test-pattern
sudo env SPI_RTSP_PORT=8554 ./spi_rtsp
```

Then verify from another terminal or client machine:

```bash
ffprobe -rtsp_transport tcp -v error -select_streams v:0 \
  -show_entries stream=codec_name,width,height,avg_frame_rate \
  -of default=nw=1 rtsp://<pi-ip>:8555/spi
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

## Verify Optimized SPI Protocol

After flashing the matching ESP32 firmware and running `sudo ./spi_rtsp`, check
the Pi `[STATS]` and ESP32 `[SPI_SLV]` logs together.

Expected steady-state Pi log properties:

```text
wrong_index=0 incomplete=0 resync=0 rtsp_fail=0
```

- `tx_info` should stop increasing after startup unless a resync happens.
- `tx_frame_start` should keep increasing and should track frame starts.
- `incomplete` should stay `0`; if it increases, the Pi skipped a frame because
  one or more chunks were missing.
- `frames` should be higher than the old CMD_INFO-per-frame protocol.

Expected steady-state ESP32 log properties:

```text
no_pkt=0 timeout=0 active_snapshot=1
```

- `info` should be low after startup unless the Pi resyncs.
- `frame_start` should increase with each `CMD_DATA index=0`.
- `snapshot_ready` should stay `1` once the camera task has produced frames.

If `resync` increases, the Pi intentionally returns to `CMD_INFO` to refresh
the cached chunk config before resuming `CMD_DATA` streaming.

You can also save combined Pi and ESP32 logs to a text file and run:

```bash
python3 verify_protocol_logs.py run.log
```

The verifier expects the same steady-state properties above and reports
`RESULT: PASS` or the counters that need attention.

One practical way to create `run.log` is:

```bash
# Pi terminal
sudo ./spi_rtsp 2>&1 | tee pi.log

# ESP32 serial terminal output should be saved as esp32.log.
# Then combine both logs before running the verifier:
cat pi.log esp32.log > run.log
python3 verify_protocol_logs.py run.log
```

## Run On Boot

```bash
sudo cp spi_rtsp.service /etc/systemd/system/spi_rtsp.service
sudo systemctl daemon-reload
sudo systemctl enable spi_rtsp.service
sudo systemctl start spi_rtsp.service
sudo journalctl -u spi_rtsp.service -f
```

The service waits for `network-online.target` before starting so the printed
RTSP URL can include the Pi network interface address. The service file sets
`SPI_RTSP_PORT=8555` to avoid conflicts with `mediamtx` on port `8554`.
