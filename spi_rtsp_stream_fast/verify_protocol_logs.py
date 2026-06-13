#!/usr/bin/env python3
"""
Check Pi [STATS] and ESP32 [SPI_SLV] logs for the optimized SPI protocol.

Usage:
  python verify_protocol_logs.py run.log
  type run.log | python verify_protocol_logs.py
"""

import re
import sys


KV_RE = re.compile(r"([A-Za-z_]+)=([0-9.]+)")


def parse_kv(line):
    out = {}
    for key, value in KV_RE.findall(line):
        out[key] = float(value) if "." in value else int(value)
    return out


def load_lines():
    if len(sys.argv) > 1:
        with open(sys.argv[1], "r", encoding="utf-8", errors="replace") as f:
            return f.readlines()
    return sys.stdin.readlines()


def nondecreasing(values):
    return all(b >= a for a, b in zip(values, values[1:]))


def main():
    lines = load_lines()
    pi = [parse_kv(line) for line in lines if "[STATS]" in line]
    esp = [parse_kv(line) for line in lines if "[SPI_SLV]" in line]

    failures = []
    warnings = []

    if not pi:
        failures.append("missing Pi [STATS] lines")
    else:
        latest = pi[-1]
        if latest.get("wrong_index", 0) != 0:
            failures.append(f"Pi wrong_index is {latest.get('wrong_index')}, expected 0")
        if latest.get("incomplete", 0) != 0:
            failures.append(f"Pi incomplete is {latest.get('incomplete')}, expected 0")
        if latest.get("rtsp_fail", 0) != 0:
            failures.append(f"Pi rtsp_fail is {latest.get('rtsp_fail')}, expected 0")
        if latest.get("resync", 0) != 0:
            warnings.append(f"Pi resync is {latest.get('resync')} (acceptable only if recovery was intentional)")
        if latest.get("frames", 0) <= 0:
            failures.append("Pi frames/s is not positive")
        if "tx_info" in latest and len(pi) >= 2:
            tx_info_values = [row.get("tx_info", 0) for row in pi if "tx_info" in row]
            if not nondecreasing(tx_info_values):
                failures.append("Pi tx_info counter decreased")
            if tx_info_values[-1] > tx_info_values[-2] and latest.get("resync", 0) == 0:
                warnings.append("Pi tx_info increased in steady state without resync")
        if latest.get("tx_frame_start", 0) <= 0:
            failures.append("Pi tx_frame_start did not advance")

    if not esp:
        warnings.append("missing ESP32 [SPI_SLV] lines")
    else:
        latest = esp[-1]
        if latest.get("timeout", 0) != 0:
            failures.append(f"ESP32 timeout is {latest.get('timeout')}, expected 0")
        if latest.get("no_pkt", 0) != 0:
            failures.append(f"ESP32 no_pkt is {latest.get('no_pkt')}, expected 0")
        if latest.get("active_snapshot", 0) != 1:
            failures.append(f"ESP32 active_snapshot is {latest.get('active_snapshot')}, expected 1")
        if latest.get("snapshot_ready", 0) != 1:
            failures.append(f"ESP32 snapshot_ready is {latest.get('snapshot_ready')}, expected 1")
        if latest.get("frame_start", 0) <= 0:
            failures.append("ESP32 frame_start did not advance")

    print(f"Pi [STATS] lines: {len(pi)}")
    print(f"ESP32 [SPI_SLV] lines: {len(esp)}")

    if warnings:
        print("WARNINGS:")
        for item in warnings:
            print(f"- {item}")

    if failures:
        print("RESULT: FAIL")
        for item in failures:
            print(f"- {item}")
        return 1

    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
