"""
Software simulation of the SPI big-chunk protocol between
spi_master_clean_big_chunk (app_state.cpp) and
optical_flow_SF30c_spi_big_chunk (rtos.cpp send_img_task + chunk_manage.c),
faithfully ported line-by-line from the C/C++ sources, run over a simulated
full-duplex byte channel (RCSA packet framing + CRC32, matching
Packet_RCSA/SerialComm.c + UDPPacket.c + crc_32.c).

Goal: verify the protocol design actually reassembles a 76800-byte image
end-to-end without needing real SPI hardware.
"""

import struct
import sys
import zlib

# ── constants (spi_comm.h, both sides) ──────────────────────────────────────
DATA_PAYLOAD_SIZE = 76800
DATA_CHUNK_SIZE   = 3274
DATA_TOTAL_CHUNKS = (DATA_PAYLOAD_SIZE + DATA_CHUNK_SIZE - 1) // DATA_CHUNK_SIZE  # 24
DATA_CAPACITY     = DATA_TOTAL_CHUNKS * DATA_CHUNK_SIZE                          # 78576
FRAME_PAYLOAD_SIZE = 2 + DATA_CHUNK_SIZE                                         # 3276
BUF_SIZE          = 9 + FRAME_PAYLOAD_SIZE + 4                                   # 3289
DATA_LAST_CHUNK_SIZE = DATA_PAYLOAD_SIZE - (DATA_TOTAL_CHUNKS - 1) * DATA_CHUNK_SIZE  # 1498
STALL_RESYNC_THRESHOLD = 30

CMD_INFO = 0x01
CMD_DATA = 0x02

UDPPAYLOAD_MAX_SIZE = 4096


def u16le(b, off):
    return b[off] | (b[off + 1] << 8)


def put_u16le(buf, off, v):
    buf[off] = v & 0xFF
    buf[off + 1] = (v >> 8) & 0xFF


# ── RCSA packet framing (UDPPacketHeader.c / UDPPacket.c / crc_32.c) ────────
def build_packet(cmd, payload: bytes) -> bytes:
    header = b"RCSA" + struct.pack("<H", 1) + bytes([cmd]) + struct.pack("<H", len(payload))
    body = header + payload
    crc = zlib.crc32(body) & 0xFFFFFFFF
    return body + struct.pack("<I", crc)


def parse_packet(buf: bytes):
    """Mirrors ProcessBufferSerialComm + RunReceiveSerialComm + FinalizeIncompletePacketSerialComm
    for the case of one clean, byte-aligned packet per transfer."""
    if buf[:4] != b"RCSA":
        return None
    payload_size = struct.unpack("<H", buf[7:9])[0]
    if payload_size > UDPPAYLOAD_MAX_SIZE:
        return None
    if 9 + payload_size + 4 > len(buf):
        return None
    payload = buf[9:9 + payload_size]
    crc_recv = struct.unpack("<I", buf[9 + payload_size:9 + payload_size + 4])[0]
    crc_calc = zlib.crc32(buf[:9] + payload) & 0xFFFFFFFF
    if crc_calc != crc_recv:
        return None
    return {"cmd": buf[6], "payload_size": payload_size, "payload": payload}


# ── chunk_manage.c get_chunk ─────────────────────────────────────────────────
def get_chunk(payload: bytes, payload_size, chunk_size, chunk_index):
    offset = chunk_index * chunk_size
    if offset >= payload_size:
        return b""
    remaining = payload_size - offset
    n = min(remaining, chunk_size)
    return payload[offset:offset + n]


# ── slave: rtos.cpp send_img_task ────────────────────────────────────────────
class Slave:
    def __init__(self, images):
        assert images
        for image in images:
            assert len(image) == DATA_PAYLOAD_SIZE
        self.images = images
        self.next_image_index = 0
        self.active_snapshot = bytes(DATA_PAYLOAD_SIZE)
        self.active_snapshot_valid = False
        self.frame_start_snapshots = []

    def handle(self, pkt):
        resp = bytearray(FRAME_PAYLOAD_SIZE)
        if pkt is None:
            return build_packet(0, bytes(resp))  # tx stays as last build (won't matter, rx side checks cmd)

        if pkt["cmd"] == CMD_INFO:
            put_u16le(resp, 0, DATA_CHUNK_SIZE)
            put_u16le(resp, 2, DATA_TOTAL_CHUNKS)
            put_u16le(resp, 4, DATA_LAST_CHUNK_SIZE)
            return build_packet(CMD_INFO, bytes(resp))

        if pkt["cmd"] == CMD_DATA:
            chunk_index = u16le(pkt["payload"], 0) if pkt["payload_size"] >= 2 else 0
            if chunk_index == 0:
                image = self.images[self.next_image_index % len(self.images)]
                self.next_image_index += 1
                self.active_snapshot = image
                self.active_snapshot_valid = True
                self.frame_start_snapshots.append(image)

            put_u16le(resp, 0, chunk_index)
            if self.active_snapshot_valid:
                chunk = get_chunk(self.active_snapshot, DATA_PAYLOAD_SIZE, DATA_CHUNK_SIZE, chunk_index)
                resp[2:2 + len(chunk)] = chunk
            return build_packet(CMD_DATA, bytes(resp))

        return build_packet(0, bytes(resp))


# ── master: app_state.cpp ────────────────────────────────────────────────────
class CommState:
    INFO_REQUEST = 0
    DATA_REQUEST = 1


class Master:
    def __init__(self, data_capacity):
        self.state = CommState.INFO_REQUEST
        self.data_capacity = data_capacity
        self.data_chunk_size = 0
        self.data_total_chunks = 0
        self.data_last_chunk_size = 0
        self.data_payload_size = 0
        self.next_index = 0
        self.expected_index = 0
        self.last_tx_index = 0
        self.has_expected_data = False
        self.last_tx_was_data = False
        self.data_assembled = bytearray(data_capacity)
        self.chunk_received = bytearray(data_capacity)
        self.chunks_received = 0
        self.data_ready = None
        self.data_ready_valid = False
        self.tx_info = 0
        self.tx_data = 0
        self.tx_frame_start = 0
        self.no_pkt = 0
        self.wrong_cmd = 0
        self.bad_payload = 0
        self.wrong_index = 0
        self.incomplete_frames = 0
        self.stall_ticks = 0
        self.resync_count = 0
        self.progressed = False

    def prepare_tx(self):
        payload = bytearray(FRAME_PAYLOAD_SIZE)
        if self.state == CommState.INFO_REQUEST:
            self.last_tx_was_data = False
            self.tx_info += 1
            return build_packet(CMD_INFO, bytes(payload))
        else:
            put_u16le(payload, 0, self.next_index)
            pkt = build_packet(CMD_DATA, bytes(payload))
            self.tx_data += 1
            if self.next_index == 0:
                self.tx_frame_start += 1
            self.last_tx_was_data = True
            self.last_tx_index = self.next_index
            if self.next_index + 1 < self.data_total_chunks:
                self.next_index += 1
            else:
                self.next_index = 0
            return pkt

    def handle_rx(self, pkt):
        self.progressed = False
        if self.state == CommState.INFO_REQUEST:
            if pkt is None:
                self.no_pkt += 1
                return False
            if pkt["cmd"] != CMD_INFO:
                self.wrong_cmd += 1
                return False
            if pkt["payload_size"] < 6:
                self.bad_payload += 1
                return False

            chunk_size = u16le(pkt["payload"], 0)
            total_chunks = u16le(pkt["payload"], 2)
            last_chunk_size = u16le(pkt["payload"], 4)

            if chunk_size == 0:
                chunk_size = 1
            max_total32 = self.data_capacity // chunk_size
            if max_total32 == 0:
                max_total32 = 1
            if max_total32 > 0xFFFF:
                max_total32 = 0xFFFF
            max_total = max_total32
            if total_chunks == 0 or total_chunks > max_total:
                total_chunks = max_total

            if last_chunk_size == 0 or last_chunk_size > chunk_size:
                last_chunk_size = chunk_size

            self.data_chunk_size = chunk_size
            self.data_total_chunks = total_chunks
            self.data_last_chunk_size = last_chunk_size
            self.data_payload_size = (total_chunks - 1) * chunk_size + last_chunk_size

            self.next_index = 0
            self.expected_index = 0
            self.has_expected_data = False
            self.chunks_received = 0
            self.chunk_received[:self.data_total_chunks] = bytes(self.data_total_chunks)
            self.state = CommState.DATA_REQUEST
            self.progressed = True
            return False

        else:  # DATA_REQUEST
            if pkt is None:
                self.no_pkt += 1
                return False
            if pkt["cmd"] != CMD_DATA:
                self.wrong_cmd += 1
                return False
            if pkt["payload_size"] < 2:
                self.bad_payload += 1
                return False

            echoed_index = u16le(pkt["payload"], 0)
            if not self.has_expected_data:
                return False
            if echoed_index != self.expected_index:
                self.wrong_index += 1
                return False

            this_chunk_size = (self.data_last_chunk_size
                                if echoed_index == self.data_total_chunks - 1
                                else self.data_chunk_size)

            copy_len = pkt["payload_size"] - 2
            if copy_len > this_chunk_size:
                copy_len = this_chunk_size

            if echoed_index == 0:
                self.chunks_received = 0
                self.chunk_received[:self.data_total_chunks] = bytes(self.data_total_chunks)
            if not self.chunk_received[echoed_index]:
                self.chunk_received[echoed_index] = 1
                self.chunks_received += 1

            offset = echoed_index * self.data_chunk_size
            self.data_assembled[offset:offset + copy_len] = pkt["payload"][2:2 + copy_len]

            if echoed_index + 1 < self.data_total_chunks:
                self.progressed = True
                self._update_expected()
                return False

            if self.chunks_received != self.data_total_chunks:
                self.incomplete_frames += 1
                self.state = CommState.DATA_REQUEST
                self.progressed = True
                self._update_expected()
                return False

            self.data_ready = bytes(self.data_assembled[:self.data_payload_size])
            self.data_ready_valid = True

            self.state = CommState.DATA_REQUEST
            self.progressed = True
            self._update_expected()
            return True

    def _update_expected(self):
        if self.state == CommState.DATA_REQUEST and self.last_tx_was_data:
            self.expected_index = self.last_tx_index
            self.has_expected_data = True

    def finish_tick(self):
        self._update_expected()
        if self.progressed:
            self.stall_ticks = 0
        else:
            self.stall_ticks += 1
            if self.stall_ticks >= STALL_RESYNC_THRESHOLD:
                self.stall_ticks = 0
                self.resync_count += 1
                self.next_index = 0
                self.expected_index = 0
                self.has_expected_data = False
                self.last_tx_was_data = False
                self.chunks_received = 0
                if self.data_total_chunks:
                    self.chunk_received[:self.data_total_chunks] = bytes(self.data_total_chunks)
                self.state = CommState.INFO_REQUEST


# ── simulation driver ───────────────────────────────────────────────────────
def make_test_images():
    test_images = [
        bytes((i * 37 + 5) % 256 for i in range(DATA_PAYLOAD_SIZE)),
        bytes((i * 53 + 11) % 256 for i in range(DATA_PAYLOAD_SIZE)),
        bytes((i * 97 + 23) % 256 for i in range(DATA_PAYLOAD_SIZE)),
    ]
    return test_images


def run_protocol(target_frames=2, max_ticks=200, drop_master_rx_ticks=None):
    test_images = make_test_images()
    master = Master(DATA_CAPACITY)
    slave = Slave(test_images)

    s_tx_buf = bytes(BUF_SIZE)  # slave's tx buffer, memset(0) at startup (rtos.cpp)

    frames_received = 0
    mismatches = 0
    tick = 0
    log = []
    drop_master_rx_ticks = drop_master_rx_ticks or set()

    while frames_received < target_frames and tick < max_ticks:
        tick += 1
        state_before = master.state
        requested_index = master.next_index if state_before == CommState.DATA_REQUEST else None

        master_tx = master.prepare_tx()
        assert len(master_tx) == BUF_SIZE

        # full-duplex exchange: this tick's rx = previous tick's prepared tx
        rx_for_master = s_tx_buf
        rx_for_slave = master_tx
        if tick in drop_master_rx_ticks:
            rx_for_master = bytes(BUF_SIZE)

        pkt_master = parse_packet(rx_for_master)
        data_ready = master.handle_rx(pkt_master)
        master.finish_tick()

        pkt_slave = parse_packet(rx_for_slave)
        s_tx_buf = slave.handle(pkt_slave)
        assert len(s_tx_buf) == BUF_SIZE

        if state_before == CommState.INFO_REQUEST:
            tx_desc = "TX cmd=INFO"
        else:
            tx_desc = f"TX cmd=DATA idx={requested_index}"

        if pkt_master is None:
            rx_desc = "RX FAIL" + (" (forced drop)" if tick in drop_master_rx_ticks else "")
        elif pkt_master["cmd"] == CMD_INFO:
            rx_desc = f"RX cmd=INFO payload={pkt_master['payload_size']}"
        else:
            idx = u16le(pkt_master["payload"], 0)
            rx_desc = f"RX cmd=DATA idx={idx}"

        log.append(f"tick={tick:3d} {tx_desc:18s} -> {rx_desc}")

        if data_ready:
            frames_received += 1
            matching_image = next((image for image in test_images if master.data_ready == image), None)
            ok = matching_image is not None
            log.append(f"  [FRAME {frames_received} READY] size={master.data_payload_size} "
                       f"chunks={master.data_total_chunks} chunk_size={master.data_chunk_size} "
                       f"last_chunk_size={master.data_last_chunk_size} "
                       f"MATCH={'OK' if ok else 'MISMATCH!!'}")
            if not ok:
                mismatches += 1
                for i in range(DATA_PAYLOAD_SIZE):
                    expected_values = sorted({image[i] for image in test_images})
                    if master.data_ready[i] not in expected_values:
                        log.append(f"  first invalid byte at {i}: got {master.data_ready[i]} expected one of {expected_values}")
                        break

    return {
        "log": log,
        "tick": tick,
        "frames_received": frames_received,
        "mismatches": mismatches,
        "slave_frame_starts": len(slave.frame_start_snapshots),
        "master": master,
    }


def print_result(result, title):
    print(f"=== {title} ===")
    print("\n".join(result["log"]))
    print()
    print(f"total ticks: {result['tick']}")
    print(f"frames received: {result['frames_received']}")
    print(f"mismatches: {result['mismatches']}")
    print(f"slave frame starts: {result['slave_frame_starts']}")
    master = result["master"]
    print(f"master tx counters: tx_info={master.tx_info} tx_data={master.tx_data} "
          f"tx_frame_start={master.tx_frame_start}")
    print(f"master rx counters: no_pkt={master.no_pkt} wrong_cmd={master.wrong_cmd} "
          f"bad_payload={master.bad_payload} wrong_index={master.wrong_index} "
          f"incomplete={master.incomplete_frames} resync={master.resync_count}")
    passed = result["frames_received"] >= 2 and result["mismatches"] == 0 and master.wrong_index == 0
    print(f"RESULT: {'PASS' if passed else 'FAIL'}")
    print()
    return passed


def run():
    happy = run_protocol(target_frames=2, max_ticks=200)
    happy_ok = print_result(happy, "happy path")

    # A short RX drop should discard only that incomplete frame. It should not
    # commit mixed/stale image data, and it should not require a watchdog resync.
    short_drop = run_protocol(target_frames=2, max_ticks=120, drop_master_rx_ticks={20})
    short_drop_ok = print_result(short_drop, "single RX drop / skip incomplete frame")

    # Drop master RX long enough to trip the stall watchdog, then verify it
    # returns to CMD_INFO and receives valid frames again.
    drops = set(range(20, 20 + STALL_RESYNC_THRESHOLD + 5))
    resync = run_protocol(target_frames=2, max_ticks=260, drop_master_rx_ticks=drops)
    resync_ok = print_result(resync, "forced master RX drop / resync recovery")

    if (happy_ok and short_drop_ok and short_drop["master"].incomplete_frames >= 1
            and resync_ok and resync["master"].resync_count >= 1):
        print("RESULT: PASS - cached-INFO streaming, incomplete-frame skip, and resync recovery all work")
        return 0
    else:
        print("RESULT: FAIL - protocol simulation did not satisfy all checks")
        return 1


if __name__ == "__main__":
    raise SystemExit(run())
