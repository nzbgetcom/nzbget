"""Deterministic container generators for the DupeArticleFallback harness.

Every generator writes STORE/COPY-mode framing around opaque payload bytes -
byte-exactly what the C++ mappers parse (see daemon/postprocess/ContentMap.cpp).
"""
import struct


def rar3_store_volumes(inner_name, data, volume_size, method=0x30):
    """Split ``data`` into RAR3 store-mode volumes (MAIN + FILE + ENDARC).

    ``method`` lets a scenario forge a "compressed" archive (framing says
    0x33) around opaque bytes without any real compressor."""
    def file_block(chunk, split_before, split_after):
        flags = 0x8000 | (0x01 if split_before else 0) | (0x02 if split_after else 0)
        name = inner_name.encode()
        head = struct.pack('<HBHH', 0, 0x74, flags, 32 + len(name))
        head += struct.pack('<II', len(chunk), len(data))    # pack, unp
        head += b'\x00'                                      # host os
        head += struct.pack('<I', 0)                         # file crc (unchecked)
        head += struct.pack('<I', 0)                         # ftime
        head += bytes([29, method])                          # unp ver, method
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


def zip_store(files):
    """A stored (method 0) zip holding ``files`` = [(name, bytes), ...]."""
    import zlib
    out = bytearray()
    local_offsets = []
    for name, data in files:
        local_offsets.append(len(out))
        encoded = name.encode()
        out += struct.pack('<IHHHHHIIIHH', 0x04034b50, 20, 0, 0, 0, 0,
                           zlib.crc32(data) & 0xffffffff, len(data), len(data),
                           len(encoded), 0)
        out += encoded + data
    cd_start = len(out)
    for (name, data), local in zip(files, local_offsets):
        encoded = name.encode()
        out += struct.pack('<IHHHHHHIIIHHHHHII', 0x02014b50, 20, 20, 0, 0, 0, 0,
                           zlib.crc32(data) & 0xffffffff, len(data), len(data),
                           len(encoded), 0, 0, 0, 0, 0, local)
        out += encoded
    cd_size = len(out) - cd_start
    out += struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, len(files), len(files),
                       cd_size, cd_start, 0)
    return bytes(out)


def _7z_number(value):
    out = bytearray()
    first = 0
    mask = 0x80
    extra = 0
    while extra < 8 and value >= 1 << (7 * (extra + 1)):
        first |= mask
        mask >>= 1
        extra += 1
    if extra < 8:
        first |= value >> (8 * extra)
    out.append(first & 0xff)
    for i in range(extra):
        out.append((value >> (8 * i)) & 0xff)
    return bytes(out)


def seven_zip_copy(files):
    """A 7z archive, Copy coder, one folder per file, plain header."""
    header = bytearray()
    header += b'\x01\x04\x06' + _7z_number(0) + _7z_number(len(files))
    header += b'\x09'
    for _, data in files:
        header += _7z_number(len(data))
    header += b'\x00\x07\x0b' + _7z_number(len(files)) + b'\x00'
    for _ in files:
        header += _7z_number(1) + b'\x01\x00'      # one coder, id size 1, Copy
    header += b'\x0c'
    for _, data in files:
        header += _7z_number(len(data))
    header += b'\x00\x00\x05' + _7z_number(len(files))
    names = bytearray(b'\x00')
    for name, _ in files:
        names += name.encode('utf-16-le') + b'\x00\x00'
    header += b'\x11' + _7z_number(len(names)) + names
    header += b'\x00\x00'
    data_size = sum(len(data) for _, data in files)
    out = bytearray(b'7z\xbc\xaf\x27\x1c\x00\x04')
    out += struct.pack('<I', 0)
    out += struct.pack('<QQ', data_size, len(header))
    out += struct.pack('<I', 0)
    for _, data in files:
        out += data
    out += header
    return bytes(out)


def split_bytes(data, sizes):
    """Split ``data`` at the given prefix sizes; the last piece takes the rest."""
    pieces = []
    pos = 0
    for size in sizes:
        pieces.append(data[pos:pos + size])
        pos += size
    pieces.append(data[pos:])
    return [piece for piece in pieces if piece]
