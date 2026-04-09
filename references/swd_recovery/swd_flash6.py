#!/usr/bin/env python3
"""
SWD Flash Recovery v6.

Two fixes over v5:
  1. Arduino sketch uses per-word noInterrupts (< 2ms windows)
  2. This script reconnects SWD every RECONNECT_EVERY words
     to keep the connection fresh (proven by swd_recovery.ino)

Usage: python3 swd_flash6.py /dev/ttyACM0
"""

import sys, os, time, struct, glob, serial

BATCH_SIZE = 16        # words per serial command
RECONNECT_EVERY = 256  # reconnect SWD every N words written


def parse_ihex(path):
    records = {}
    base = 0
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line.startswith(':'): continue
            raw = bytes.fromhex(line[1:])
            count, addr, rtype = raw[0], (raw[1] << 8) | raw[2], raw[3]
            payload = raw[4:4 + count]
            if rtype == 0x00:
                records[base + addr] = payload
            elif rtype == 0x02:
                base = ((payload[0] << 8) | payload[1]) << 4
            elif rtype == 0x04:
                base = ((payload[0] << 8) | payload[1]) << 16
            elif rtype == 0x01:
                break

    addrs = sorted(records)
    blocks, start, data = [], addrs[0], bytearray(records[addrs[0]])
    for a in addrs[1:]:
        if a == start + len(data):
            data += records[a]
        else:
            blocks.append((start, bytes(data)))
            start, data = a, bytearray(records[a])
    blocks.append((start, bytes(data)))
    return blocks


def find_hex_file():
    base = os.path.expanduser("~/.arduino15/packages/adafruit/hardware/nrf52")
    pattern = os.path.join(base, "*/bootloader/feather_nrf52832/"
                           "feather_nrf52832_bootloader*_s132_*.hex")
    files = sorted(glob.glob(pattern))
    if files: return files[-1]
    sys.exit("ERROR: Bootloader hex not found.")


class SWDBridge:
    def __init__(self, port):
        self.ser = serial.Serial(port, 115200, timeout=10)
        print("  Waiting for bridge...")
        deadline = time.time() + 15
        while time.time() < deadline:
            line = self.ser.readline().decode('ascii', errors='ignore').strip()
            if 'SWD_BRIDGE_READY' in line:
                print("  Ready!")
                return
            if line:
                print(f"  > {line}")
        raise RuntimeError("Bridge didn't send READY")

    def _resp(self):
        r = self.ser.read(1)
        if not r: raise TimeoutError("No response")
        if r == b'K': return
        if r == b'F':
            msg = self.ser.readline().decode('ascii', errors='ignore').strip()
            raise RuntimeError(f"Bridge: {msg}")
        raise RuntimeError(f"Bad: 0x{r[0]:02X}")

    def ping(self):
        self.ser.write(b'P'); self._resp()

    def connect(self):
        self.ser.write(b'C'); self._resp()

    def disconnect(self):
        self.ser.write(b'D'); self._resp()

    def chip_erase(self):
        self.ser.timeout = 15
        self.ser.write(b'E'); self._resp()
        self.ser.timeout = 10

    def nvmc_write_enable(self):
        self.ser.write(b'N'); self._resp()

    def nvmc_readonly(self):
        self.ser.write(b'O'); self._resp()

    def write_words(self, addr, words):
        assert 1 <= len(words) <= 16
        buf = b'W' + struct.pack('<I', addr) + bytes([len(words)])
        for w in words: buf += struct.pack('<I', w)
        self.ser.write(buf)
        self._resp()

    def read_words(self, addr, count):
        assert 1 <= count <= 8
        self.ser.write(b'V' + struct.pack('<I', addr) + bytes([count]))
        self._resp()
        data = self.ser.read(count * 4)
        if len(data) != count * 4:
            raise TimeoutError(f"Short read: {len(data)}/{count*4}")
        return [struct.unpack_from('<I', data, i * 4)[0] for i in range(count)]

    def reset_target(self):
        self.ser.write(b'X'); self._resp()

    def reconnect(self):
        """Fresh SWD session — keeps link reliable."""
        self.disconnect()
        time.sleep(0.05)
        self.connect()

    def close(self):
        self.ser.close()


def flash_data(bridge, blocks, label):
    total_bytes = sum(len(d) for _, d in blocks)
    written_bytes = 0
    batches = 0
    words_since_reconnect = 0
    reconnects = 0
    t_start = time.time()

    bridge.nvmc_write_enable()

    for base_addr, data in blocks:
        # Pad to word alignment
        pad = (4 - len(data) % 4) % 4
        if pad: data = data + b'\xFF' * pad

        words = [struct.unpack_from('<I', data, i)[0]
                 for i in range(0, len(data), 4)]

        for i in range(0, len(words), BATCH_SIZE):
            batch = words[i:i + BATCH_SIZE]
            addr = base_addr + i * 4
            written_bytes += len(batch) * 4

            # Skip all-erased batches
            if all(w == 0xFFFFFFFF for w in batch):
                continue

            # Periodic SWD reconnect for reliability
            if words_since_reconnect >= RECONNECT_EVERY:
                bridge.nvmc_readonly()
                bridge.reconnect()
                bridge.nvmc_write_enable()
                words_since_reconnect = 0
                reconnects += 1

            bridge.write_words(addr, batch)
            words_since_reconnect += len(batch)
            batches += 1

            if batches % 20 == 0:
                _show_progress(label, written_bytes, total_bytes, t_start)

    bridge.nvmc_readonly()
    elapsed = time.time() - t_start
    rate = total_bytes / elapsed / 1024 if elapsed > 0 else 0
    print(f"\r  {label}: [{'#' * 50}] 100%  "
          f"{rate:.1f} KB/s  {elapsed:.0f}s  "
          f"({batches} writes, {reconnects} reconnects)")
    return batches


def _show_progress(label, written, total, t_start):
    elapsed = time.time() - t_start
    pct = min(100, written * 100 // total)
    rate = written / elapsed / 1024 if elapsed > 0 else 0
    bar = '#' * (pct // 2) + '-' * (50 - pct // 2)
    print(f"\r  {label}: [{bar}] {pct:3d}%  {rate:.1f} KB/s", end='', flush=True)


def verify_data(bridge, blocks, label):
    """Spot-check first 8 + last 8 words of each block."""
    errors = 0
    checked = 0
    for base_addr, data in blocks:
        if len(data) < 4: continue

        # Check start of block
        n = min(8, len(data) // 4)
        expected = [struct.unpack_from('<I', data, i * 4)[0] for i in range(n)]
        actual = bridge.read_words(base_addr, n)
        for i in range(n):
            if expected[i] != actual[i]:
                print(f"\n  FAIL 0x{base_addr+i*4:08X}: "
                      f"exp 0x{expected[i]:08X} got 0x{actual[i]:08X}")
                errors += 1
        checked += n

        # Check end of block if big enough
        total_words = len(data) // 4
        if total_words > 16:
            end_n = min(8, total_words)
            end_off = (total_words - end_n) * 4
            end_addr = base_addr + end_off
            expected2 = [struct.unpack_from('<I', data, end_off + i * 4)[0]
                         for i in range(end_n)]
            actual2 = bridge.read_words(end_addr, end_n)
            for i in range(end_n):
                if expected2[i] != actual2[i]:
                    print(f"\n  FAIL 0x{end_addr+i*4:08X}: "
                          f"exp 0x{expected2[i]:08X} got 0x{actual2[i]:08X}")
                    errors += 1
            checked += end_n

    status = 'OK' if errors == 0 else f'{errors} ERRORS'
    print(f"  {label}: {status} ({checked} words checked)")
    return errors


def main():
    if len(sys.argv) < 2:
        sys.exit(f"Usage: {sys.argv[0]} <serial_port>")

    port = sys.argv[1]

    print("Finding hex file...")
    hex_path = find_hex_file()
    print(f"  {os.path.basename(hex_path)}")

    print("Parsing...")
    all_blocks = parse_ihex(hex_path)
    flash_bl = [(a, d) for a, d in all_blocks if a < 0x10000000]
    uicr_bl = [(a, d) for a, d in all_blocks if a >= 0x10000000]
    flash_size = sum(len(d) for _, d in flash_bl)
    print(f"  {flash_size} bytes ({flash_size // 1024}KB)")

    print(f"\nOpening {port}...")
    bridge = SWDBridge(port)
    bridge.ping()

    print("SWD connect...")
    bridge.connect()
    print("  OK")

    # Quick read before erase
    mbr = bridge.read_words(0x00000000, 2)
    print(f"  MBR[0]=0x{mbr[0]:08X} MBR[1]=0x{mbr[1]:08X}")

    print("\nChip erase...")
    t0 = time.time()
    bridge.chip_erase()
    print(f"  Done ({time.time()-t0:.1f}s)")

    # Reconnect after erase
    bridge.reconnect()

    # Verify erase
    vals = bridge.read_words(0x00000000, 4)
    if all(v == 0xFFFFFFFF for v in vals):
        print("  Erase verified OK")
    else:
        print(f"  ERASE FAILED: {[hex(v) for v in vals]}")
        bridge.close(); sys.exit(1)

    # Flash
    print(f"\nWriting {flash_size // 1024}KB...")
    t0 = time.time()
    n = flash_data(bridge, flash_bl, "Flash")
    elapsed = time.time() - t0
    print(f"  Total: {elapsed:.0f}s")

    # Quick check: MBR should be valid
    print("\nQuick check...")
    bridge.reconnect()
    quick = bridge.read_words(0x00000000, 4)
    expected_sp = struct.unpack_from('<I', flash_bl[0][1], 0)[0]
    print(f"  MBR[0]=0x{quick[0]:08X} (expect 0x{expected_sp:08X})")
    if quick[0] != expected_sp:
        print("  WARNING: MBR mismatch!")

    # Write UICR
    print("\nWriting UICR...")
    bridge.nvmc_write_enable()
    for addr, data in uicr_bl:
        for i in range(0, len(data), 4):
            word = struct.unpack_from('<I', data, i)[0]
            if word != 0xFFFFFFFF:
                bridge.write_words(addr + i, [word])
                print(f"  0x{addr+i:08X} = 0x{word:08X}")
    bridge.nvmc_readonly()

    # Full verify with fresh connection
    print("\nVerifying...")
    bridge.reconnect()

    errors = verify_data(bridge, flash_bl, "Flash")

    # Check UICR
    for addr, data in uicr_bl:
        for i in range(0, len(data), 4):
            word = struct.unpack_from('<I', data, i)[0]
            if word != 0xFFFFFFFF:
                actual = bridge.read_words(addr + i, 1)[0]
                tag = "OK" if actual == word else "FAIL"
                print(f"  UICR 0x{addr+i:08X}: 0x{actual:08X} {tag}")
                if actual != word: errors += 1

    if errors == 0:
        print(f"\nAll verified OK! Resetting target...")
        bridge.reset_target()
        print("Done — board should show alternating red/blue LEDs")
    else:
        print(f"\n{errors} error(s) — not resetting")

    bridge.close()


if __name__ == '__main__':
    main()
