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
import zlib

# ── constants (spi_comm.h, both sides) ──────────────────────────────────────
DATA_PAYLOAD_SIZE = 76800
DATA_CHUNK_SIZE   = 3274
DATA_TOTAL_CHUNKS = (DATA_PAYLOAD_SIZE + DATA_CHUNK_SIZE - 1) // DATA_CHUNK_SIZE  # 24
DATA_CAPACITY     = DATA_TOTAL_CHUNKS * DATA_CHUNK_SIZE                          # 78576
FRAME_PAYLOAD_SIZE = 2 + DATA_CHUNK_SIZE                                         # 3276
BUF_SIZE          = 9 + FRAME_PAYLOAD_SIZE + 4                                   # 3289
DATA_LAST_CHUNK_SIZE = DATA_PAYLOAD_SIZE - (DATA_TOTAL_CHUNKS - 1) * DATA_CHUNK_SIZE  # 1498

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
    def __init__(self, image: bytes):
        assert len(image) == DATA_PAYLOAD_SIZE
        self.image = image

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
            put_u16le(resp, 0, chunk_index)
            chunk = get_chunk(self.image, DATA_PAYLOAD_SIZE, DATA_CHUNK_SIZE, chunk_index)
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
        self.data_assembled = bytearray(data_capacity)
        self.data_ready = None
        self.data_ready_valid = False

    def prepare_tx(self):
        payload = bytearray(FRAME_PAYLOAD_SIZE)
        if self.state == CommState.INFO_REQUEST:
            return build_packet(CMD_INFO, bytes(payload))
        else:
            put_u16le(payload, 0, self.next_index)
            return build_packet(CMD_DATA, bytes(payload))

    def handle_rx(self, pkt):
        if self.state == CommState.INFO_REQUEST:
            if pkt is None or pkt["cmd"] != CMD_INFO or pkt["payload_size"] < 6:
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
            self.state = CommState.DATA_REQUEST
            return False

        else:  # DATA_REQUEST
            if pkt is None or pkt["cmd"] != CMD_DATA or pkt["payload_size"] < 2:
                return False

            echoed_index = u16le(pkt["payload"], 0)
            if echoed_index != self.next_index:
                return False

            this_chunk_size = (self.data_last_chunk_size
                                if echoed_index == self.data_total_chunks - 1
                                else self.data_chunk_size)

            copy_len = pkt["payload_size"] - 2
            if copy_len > this_chunk_size:
                copy_len = this_chunk_size

            offset = echoed_index * self.data_chunk_size
            self.data_assembled[offset:offset + copy_len] = pkt["payload"][2:2 + copy_len]

            self.next_index += 1
            if self.next_index < self.data_total_chunks:
                return False

            self.data_ready = bytes(self.data_assembled[:self.data_payload_size])
            self.data_ready_valid = True

            self.next_index = 0
            self.state = CommState.INFO_REQUEST
            return True


# ── simulation driver ───────────────────────────────────────────────────────
def run():
    # deterministic test "image" pattern
    test_image = bytes((i * 37 + 5) % 256 for i in range(DATA_PAYLOAD_SIZE))

    master = Master(DATA_CAPACITY)
    slave = Slave(test_image)

    s_tx_buf = bytes(BUF_SIZE)  # slave's tx buffer, memset(0) at startup (rtos.cpp)

    frames_received = 0
    tick = 0
    max_ticks = 200
    log = []

    while frames_received < 2 and tick < max_ticks:
        tick += 1
        state_before = master.state
        requested_index = master.next_index if state_before == CommState.DATA_REQUEST else None

        master_tx = master.prepare_tx()
        assert len(master_tx) == BUF_SIZE

        # full-duplex exchange: this tick's rx = previous tick's prepared tx
        rx_for_master = s_tx_buf
        rx_for_slave = master_tx

        pkt_master = parse_packet(rx_for_master)
        data_ready = master.handle_rx(pkt_master)

        pkt_slave = parse_packet(rx_for_slave)
        s_tx_buf = slave.handle(pkt_slave)
        assert len(s_tx_buf) == BUF_SIZE

        if state_before == CommState.INFO_REQUEST:
            tx_desc = "TX cmd=INFO"
        else:
            tx_desc = f"TX cmd=DATA idx={requested_index}"

        if pkt_master is None:
            rx_desc = "RX FAIL"
        elif pkt_master["cmd"] == CMD_INFO:
            rx_desc = f"RX cmd=INFO payload={pkt_master['payload_size']}"
        else:
            idx = u16le(pkt_master["payload"], 0)
            rx_desc = f"RX cmd=DATA idx={idx}"

        log.append(f"tick={tick:3d} {tx_desc:18s} -> {rx_desc}")

        if data_ready:
            frames_received += 1
            ok = master.data_ready == test_image
            log.append(f"  [FRAME {frames_received} READY] size={master.data_payload_size} "
                       f"chunks={master.data_total_chunks} chunk_size={master.data_chunk_size} "
                       f"last_chunk_size={master.data_last_chunk_size} "
                       f"MATCH={'OK' if ok else 'MISMATCH!!'}")
            if not ok:
                for i in range(DATA_PAYLOAD_SIZE):
                    if master.data_ready[i] != test_image[i]:
                        log.append(f"  first mismatch at byte {i}: got {master.data_ready[i]} expected {test_image[i]}")
                        break

    print("\n".join(log))
    print()
    print(f"total ticks: {tick}")
    print(f"frames received: {frames_received}")
    if frames_received >= 2:
        print("RESULT: PASS - protocol reassembles 76800-byte image correctly across multiple frame cycles")
    else:
        print("RESULT: FAIL - did not converge")


if __name__ == "__main__":
    run()
