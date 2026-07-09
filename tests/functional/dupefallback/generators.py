"""Deterministic container generators for the DupeArticleFallback harness.

Every generator writes STORE/COPY-mode framing around opaque payload bytes -
byte-exactly what the C++ mappers parse (see daemon/postprocess/ContentMap.cpp).
"""
import struct


def rar3_store_volumes(inner_name, data, volume_size):
    """Split ``data`` into RAR3 store-mode volumes (MAIN + FILE + ENDARC)."""
    def file_block(chunk, split_before, split_after):
        flags = 0x8000 | (0x01 if split_before else 0) | (0x02 if split_after else 0)
        name = inner_name.encode()
        head = struct.pack('<HBHH', 0, 0x74, flags, 32 + len(name))
        head += struct.pack('<II', len(chunk), len(data))    # pack, unp
        head += b'\x00'                                      # host os
        head += struct.pack('<I', 0)                         # file crc (unchecked)
        head += struct.pack('<I', 0)                         # ftime
        head += bytes([29, 0x30])                            # unp ver, method: store
        head += struct.pack('<H', len(name))
        head += struct.pack('<I', 0x20)                      # attributes
        head += name
        return head + chunk

    volumes = []
    pos = 0
    while pos < len(data):
        chunk = data[pos:pos + volume_size]
        vol = bytes([0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00])
        vol += struct.pack('<HBHH', 0, 0x73, 0x0011, 13) + b'\x00' * 6
        vol += file_block(chunk, pos > 0, pos + volume_size < len(data))
        vol += struct.pack('<HBHH', 0, 0x7b, 0, 7)
        volumes.append(vol)
        pos += volume_size
    return volumes
