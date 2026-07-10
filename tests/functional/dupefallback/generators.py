"""Deterministic container generators for the DupeArticleFallback harness.

Every generator writes STORE/COPY-mode framing around opaque payload bytes -
byte-exactly what the C++ mappers parse (see daemon/postprocess/ContentMap.cpp).

The zip_deflated/seven_zip_lzma*/HAVE_7Z group below is the exception: those
build REAL compressed (and, for the encrypted variant, header-encrypted)
archives by shelling out to a local 7z binary - genuine compression that
ContentMap's BuildZipMap/BuildSevenZipMap refuse to map (Method != 0 / a
non-Copy coder), which is exactly what forces the M4 decompression-donor
path (option DupeStreamDecompress) instead of the M2 byte-copy path.
"""
import hashlib
import io
import os
import shutil
import struct
import subprocess
import zipfile

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    HAVE_CRYPTO = True
except ImportError:
    HAVE_CRYPTO = False


def _find_7z():
    """Locate a real 7z/7za/7zr binary on PATH. Its path is also fed to the
    daemon's SevenZipCmd option (see harness.py's SCENARIO_OPTIONS) so
    Unpack::MakeExtractor can shell out to the SAME binary that built these
    fixtures to extract them back out."""
    for name in ('7z', '7za', '7zr'):
        path = shutil.which(name)
        if path:
            return path
    return None


SEVENZIP_PATH = _find_7z()
HAVE_7Z = SEVENZIP_PATH is not None


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


def _derive_rar3_key_iv(password, salt):
    """Pure-Python reproduction of StreamCrypto::DeriveRar3 (rar3 AES-128-CBC
    key schedule): SHA-1 over UTF-16LE password + 8-byte salt, re-fed each of
    0x40000 rounds together with a 3-byte little-endian round counter; the 16
    IV bytes are sampled every rounds/16 rounds from the running digest's last
    byte (via a copy-and-finalize snapshot that does not disturb the running
    hash); the key is the final digest's first 16 bytes, byte-swizzled per
    4-byte group (key[i*4+j] = digest[i*4+3-j]). See rar3_kdf_self_test()
    below for the byte-exact cross-check against the C++ vector."""
    seed = password.encode('utf-16-le') + salt
    h = hashlib.sha1()
    rounds = 0x40000
    step = rounds // 16
    iv = bytearray(16)
    for i in range(rounds):
        h.update(seed)
        h.update(bytes([i & 0xff, (i >> 8) & 0xff, (i >> 16) & 0xff]))
        if i % step == 0:
            iv[i // step] = h.copy().digest()[19]
    digest = h.digest()
    key = bytearray(16)
    for i in range(4):
        for j in range(4):
            key[i * 4 + j] = digest[i * 4 + 3 - j]
    return bytes(key), bytes(iv)


def rar3_kdf_self_test():
    """Cross-check _derive_rar3_key_iv against the pinned real-WinRAR-derived
    vector in tests/postprocess/StreamCrypto.cpp
    (StreamCryptoDeriveRar3VectorTest, password "123"). Raises AssertionError
    if this pure-Python KDF ever drifts from the C++ StreamCrypto::DeriveRar3
    it must match bit-for-bit before any encrypted fixture can be trusted."""
    salt = bytes([0x92, 0x80, 0x37, 0x09, 0x6e, 0x88, 0x92, 0xdb])
    key, iv = _derive_rar3_key_iv("123", salt)
    expect_key = bytes([0x56, 0xb3, 0x08, 0x9b, 0xf3, 0x26, 0x3f, 0xfa,
                        0x21, 0xae, 0x16, 0x43, 0x5e, 0x80, 0x72, 0xf6])
    expect_iv = bytes([0x8c, 0x84, 0x7e, 0xfd, 0x84, 0x80, 0xe0, 0x41,
                       0x95, 0x50, 0xee, 0x18, 0xb6, 0x88, 0x61, 0xff])
    assert key == expect_key, 'rar3 KDF key mismatch: %s != %s' % (key.hex(), expect_key.hex())
    assert iv == expect_iv, 'rar3 KDF iv mismatch: %s != %s' % (iv.hex(), expect_iv.hex())


if HAVE_CRYPTO:
    # fail fast at import time: an encrypted fixture built on a drifted KDF
    # would silently produce volumes that decrypt to garbage everywhere.
    rar3_kdf_self_test()


def rar3_store_volumes_encrypted(inner_name, data, volume_size, password):
    """Split ``data`` into PASSWORD-ENCRYPTED RAR3 store-mode volumes (MAIN +
    FILE + ENDARC), matching what ContentMap.cpp's contiguous cipher-composite
    mapper expects (see BuildRarMap's "anyEncrypted" branch):

    * one AES-128-CBC stream for the WHOLE inner file (key/salt/IV identical
      across every volume - a differing salt would break the cross-volume
      "same key" invariant the mapper enforces);
    * the salt is deterministic from (password, inner_name, volume_size), not
      random, so fixtures are reproducible;
    * every volume's FILE header carries the SALT (0x0400) and PASSWORD
      (0x0004) flags, with the 8-byte salt appended right after the name -
      byte-exactly the layout RarReader.cpp's ReadRar3File expects;
    * PACK_SIZE per volume is the number of ciphertext bytes physically
      stored there. Real WinRAR cuts this contiguous cipher stream at
      ARBITRARY plaintext-byte offsets (proven in M3 Task 3: observed chunk
      sizes 9960/9959/1393 - none a multiple of 16), so non-last volumes here
      carry an UNROUNDED ``volume_size``-byte slice on purpose (pass a
      volume_size that is not itself a multiple of 16 to exercise the
      cross-member block-straddle assembly path the M3 gate lift added).
      Only the LAST volume's slice is padded up to a whole AES block
      (ceil16), since the overall cipher stream can only be truncated at its
      very end - this keeps sum(PACK_SIZE) == ceil16(len(data)) exactly, the
      geometry gate BuildRarMap checks."""
    if not HAVE_CRYPTO:
        raise RuntimeError('cryptography is not installed')

    salt = hashlib.sha1(('rar3-store-salt|%s|%s|%d' %
                         (password, inner_name, volume_size)).encode()).digest()[:8]
    key, iv = _derive_rar3_key_iv(password, salt)

    pad = (-len(data)) % 16
    encryptor = Cipher(algorithms.AES(key), modes.CBC(iv)).encryptor()
    ciphertext = encryptor.update(data + b'\x00' * pad) + encryptor.finalize()

    def file_block(cipher_chunk, split_before, split_after):
        flags = (0x8000 | 0x0004 | 0x0400 |
                 (0x01 if split_before else 0) | (0x02 if split_after else 0))
        name = inner_name.encode()
        head = struct.pack('<HBHH', 0, 0x74, flags, 32 + len(name) + 8)
        head += struct.pack('<II', len(cipher_chunk), len(data))    # pack, unp
        head += b'\x00'                                      # host os
        head += struct.pack('<I', 0)                         # file crc (unchecked)
        head += struct.pack('<I', 0)                         # ftime
        head += bytes([29, 0x30])                            # unp ver, method (store)
        head += struct.pack('<H', len(name))
        head += struct.pack('<I', 0x20)                      # attributes
        head += name
        head += salt
        return head + cipher_chunk

    starts = list(range(0, len(data), volume_size)) or [0]
    volumes = []
    for i, start in enumerate(starts):
        is_last = i == len(starts) - 1
        end = len(ciphertext) if is_last else start + volume_size
        cipher_chunk = ciphertext[start:end]
        vol = bytes([0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00])
        vol += struct.pack('<HBHH', 0, 0x73, 0x0011, 13) + b'\x00' * 6
        vol += file_block(cipher_chunk, start > 0, not is_last)
        vol += struct.pack('<HBHH', 0, 0x7b, 0, 7)
        volumes.append(vol)
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


def zip_deflated(files):
    """A real DEFLATE-compressed zip holding ``files`` = [(name, bytes), ...]
    (stdlib zipfile.ZIP_DEFLATED - genuine compression, unlike zip_store's
    forged STORE framing above). BuildZipMap rejects any entry whose Method
    != 0 ("compressed zip entry (only stored maps)"), so a donor built with
    this generator can never map through M2 - it exercises the M4
    decompression-donor path exclusively. Deterministic (fixed date_time and
    external attributes; the payload bytes themselves come from the caller's
    own deterministic _payload(size, seed))."""
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED) as zf:
        for name, data in files:
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            zf.writestr(info, data)
    return buf.getvalue()


def _seven_zip_pack(files, workdir, out_name, extra_args):
    if not HAVE_7Z:
        raise RuntimeError('7z is not installed')
    names = []
    for name, data in files:
        path = os.path.join(workdir, name)
        os.makedirs(os.path.dirname(path) or workdir, exist_ok=True)
        with open(path, 'wb') as f:
            f.write(data)
        names.append(name)
    out = os.path.join(workdir, out_name)
    subprocess.run([SEVENZIP_PATH, 'a', '-bd', '-m0=LZMA2'] + extra_args + [out] + names,
                   cwd=workdir, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(out, 'rb') as f:
        return f.read()


def seven_zip_lzma(files, workdir):
    """A real LZMA2-compressed 7z archive holding ``files`` =
    [(name, bytes), ...], built by shelling out to the local 7z binary (see
    HAVE_7Z/SEVENZIP_PATH). BuildSevenZipMap only maps a Copy-only coder
    ("non-Copy 7z coder (only copy mode maps)"), so this donor can never map
    through M2 - only through the M4 decompression path. ``workdir`` is a
    scratch directory the CALLER creates and removes; raises if 7z is not
    installed (callers must check HAVE_7Z first and SKIP gracefully)."""
    return _seven_zip_pack(files, workdir, 'out.7z', [])


def seven_zip_lzma_encrypted(files, workdir, password):
    """A HEADER-ENCRYPTED LZMA2 7z archive (-mhe=on -p<password>: both file
    names and data are encrypted - real 7z header encryption), holding
    ``files`` = [(name, bytes), ...]. Locks in the POSIX password-quoting fix
    (M4 Task 2, commit 3c3e9130): Unpack::MakePassword's non-Windows branch
    passes -p<password> as ONE raw argv element via fork+execvp (no shell,
    no quote-stripping) - exactly how this generator invokes 7z here, so the
    donor password travels identically on both sides of the fixture."""
    return _seven_zip_pack(files, workdir, 'out_enc.7z',
                            ['-mhe=on', '-p' + password])


def split_bytes(data, sizes):
    """Split ``data`` at the given prefix sizes; the last piece takes the rest."""
    pieces = []
    pos = 0
    for size in sizes:
        pieces.append(data[pos:pos + size])
        pos += size
    pieces.append(data[pos:])
    return [piece for piece in pieces if piece]
