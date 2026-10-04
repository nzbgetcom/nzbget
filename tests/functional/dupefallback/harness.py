#!/usr/bin/env python3
#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2026 Denis <denis@nzbget.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""
Cross-platform functional harness for the DupeArticleFallback feature.

It drives nzbget's own test NNTP server (``nzbget --nserv``) plus a scratch
nzbget daemon, entirely offline, and verifies that a download missing articles
on the news server is completed by borrowing the equivalent articles from a
duplicate posting of the same content.

The same scenarios run on Linux, macOS and Android; the only thing that differs
is *where* the processes/files live, which is isolated behind the ``Target``
abstraction:

* ``LocalTarget`` runs nserv + the daemon as local subprocesses (Linux, macOS).
* ``AdbTarget`` runs them inside a connected Android emulator/device over adb,
  forwarding the RPC port back to the host.

Scenarios (all use the ``!serverlist`` nserv message-id suffix to make an
article "missing" on the active server, so no real Usenet access is needed):

* complementary - two postings, each missing different articles; neither
  completes alone, together they do.
* cutover       - the primary is missing most of a file; after a few
  recoveries the file leads with the duplicate (DupeArticleFallback cutover).
* leadswitch    - the top-scored duplicate shares the primary's hole; after a
  few consecutive lead misses the lead rotates to the next duplicate, which
  completes the file (lead demotion).
* manydonors    - more duplicates than the donor cache holds, to exercise the
  cache-eviction path (regression for the use-after-free crash).
* stream        - the donor is segmented differently; missing byte ranges are
  repaired on stream level in post-processing.
* liveoverlap   - DupeArticleFallback=live: a damaged file is stream-repaired
  DURING the download of the rest of the collection (log-order proof).
* livegate      - the same fixture under plain "stream" must not run live.
* livelastfile  - live mode, single-file collection: the live dispatch is
  skipped for the collection's last file (post-processing handles it).
* repost        - a 4-member opaque "rar+par2 release" reposted byte-identically
  under different segmentation; the damaged archive volume is repaired, while
  the PAR2 hole stays untouched and no donor PAR2 article is requested
  (final status FAILURE/PAR by design - the stand-in par2 is random bytes).
* repostrenamed - a 3-member repost whose members were RENAMED (different
  release base name, same volume suffixes): exact-name pairing cannot fire,
  proving the unique-suffix-key tier pairs the damaged member with its donor
  twin end-to-end.
* repostobfuscated - an 8-member repost of equal-size volumes with
  obfuscated names on BOTH sides and a shuffled donor order: names, suffixes,
  positions and article sizes all fail to identify the twins, so stream repair
  must probe the donor's members until one is byte-identical, for every
  damaged member.
* repostdonorgaps - the donor repost misses articles of its own exactly where
  the identity probes land first; missing probes must be replaced by other
  eligible donor articles instead of rejecting the true twin.
* xpackbare     - M2 cross-packing: a bare .mkv completed with a hole is
  repaired from a duplicate that posted the SAME movie packed into store-mode
  RAR3 volumes (different framing/offsets/segmentation), which M1 cannot pair;
  the ContentMap pass locates and patches the missing bytes inside the volumes.
* xpackrar      - store-rar target repaired from a bare donor, including a
  degraded volume whose header hole must exclude it from the map.
* xpackrar2rar  - rar-to-rar cross-packing with different volume sizes on
  each side (member-wise M1 cannot window these).
* xpackzip      - bare target repaired from a SPANNED STORED ZIP donor
  (z01+z02+zip).
* xpack7z       - bare target repaired from a 7z-COPY donor posted as
  .7z.001/.002 splits.
* xpacksplit    - store-rar target repaired from RAW SPLITS
  (movie.mkv.001/.002/.003).
* xpackcompressed - the mechanism ladder on a COMPRESSED archive: M2 never
  maps it (method gate), a byte-identical repost still repairs it via M1.
* xpackneg      - the negative: a same-size, wrong-bytes donor set must be
  rejected by the inner probes; nothing may be written.
* xcrypt_encplain  - M3 password-assisted cross-packing: an ENCRYPTED
  store-rar target (its own password known via its NZB) with a data hole,
  repaired from a BARE unencrypted donor; asserts byte-identical ciphertext.
* xcrypt_plainenc  - reverse direction: a BARE target repaired from an
  ENCRYPTED store-rar donor whose password travels via the donor's NZB.
* xcrypt_diffpass  - both sides encrypted under DIFFERENT passwords; proves
  the donor's and target's crypto contexts never mix.
* xcrypt_wrongpass - the negative: an encrypted donor whose supplied
  password does NOT match; rejected by the content-identity probe, nothing
  written. The four xcrypt_* scenarios SKIP gracefully when the
  ``cryptography`` package is not installed (see generators.HAVE_CRYPTO).
* xdecomp_zip    - M4 decompression-assisted donor extraction: a bare
  movie.mkv target with holes, repaired from a REAL DEFLATE-compressed zip
  donor of the identical file. M2 never maps a compressed zip entry, so only
  materializing the donor and shelling out to the configured SevenZipCmd
  (option DupeStreamDecompress=yes) can recover it.
* xdecomp_7z     - same shape, donor is a REAL LZMA2-compressed 7z archive.
* xdecomp_storetarget - a store-mode rar3 TARGET (not bare) repaired from a
  compressed-7z donor, proving the M4 path composes with the M2 plain
  target map, not just the bare/identity map.
* xdecomp_enc7z  - the POSIX password-quoting proof: a bare target repaired
  from a HEADER-ENCRYPTED 7z donor (-mhe=on), its password threaded via the
  donor's own NZB exactly like xcrypt_plainenc threads an encrypted-rar
  donor's password.
* xdecomp_enctarget - the M3+M4 composition: an ENCRYPTED store-rar target
  (password via its own NZB) repaired from a compressed 7z donor; the
  extracted plaintext is re-encrypted under the target's AES-CBC stream
  context and the ciphertext written into the hole.
* xdecomp_neg    - the negative: a compressed donor with the right inner
  size but the WRONG bytes; rejected by the identity probe, nothing written.
* xdecomp_symlink - the link fail-close: a compressed donor whose archive
  contains a symlink is rejected before selecting or patching anything;
  cleanup unlinks without following. POSIX-only.
* xdecomp_off    - the opt-in gate: the same compressed-7z-donor setup as
  xdecomp_7z, but DupeStreamDecompress is OMITTED (default no); the
  decompression path must never run and the item stays unrepaired. The eight
  xdecomp_* scenarios SKIP gracefully when no local 7z/7za/7zr binary is on
  PATH (see generators.HAVE_7Z).

Usage:
    harness.py --nzbget /path/to/nzbget [--target local|adb]
               [--scenario all|complementary|cutover|leadswitch|cutovertruth|manydonors|stream|liveoverlap|livegate|livelastfile|repost|repostrenamed|repostobfuscated|repostdonorgaps|xpackbare|xpackrar|xpackrar2rar|xpack2sets|xpackzip|xpack7z|xpacksplit|xpackcompressed|xpackneg|xcrypt_encplain|xcrypt_plainenc|xcrypt_diffpass|xcrypt_wrongpass|xdecomp_zip|xdecomp_7z|xdecomp_storetarget|xdecomp_enc7z|xdecomp_enctarget|xdecomp_neg|xdecomp_symlink|xdecomp_off]
               [--serial NNN] [--keep]
"""

import argparse
import base64
import os
import random
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import socketserver
import threading
import zlib
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
try:
    from xmlrpc.client import ServerProxy
except ImportError:
    from xmlrpclib import ServerProxy  # type: ignore  # python 2 fallback

# deterministic container generators for the xpack scenarios
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generators


# --------------------------------------------------------------------------- #
# NZB generation (nserv message-id format: <path?part=offset:size[!servers]>)
# --------------------------------------------------------------------------- #

def _nzb_file_block(served_path, subject_name, file_size, seg_size, missing_parts):
    """One <file>...</file> block for nserv-served content (msgid format:
    <path?part=offset:size[!servers]>)."""
    import html
    n = (file_size + seg_size - 1) // seg_size
    lines = [
        '<file poster="dupefallback@test" date="1700000000" '
        'subject="&quot;%s&quot; yEnc (1/%d)">' % (html.escape(subject_name), n),
        '<groups><group>alt.binaries.test</group></groups>',
        '<segments>',
    ]
    for i in range(1, n + 1):
        off = (i - 1) * seg_size
        size = min(seg_size, file_size - off)
        miss = '!2' if i in missing_parts else ''
        msgid = '%s?%d=%d:%d%s' % (served_path, i, off, size, miss)
        lines.append('<segment bytes="%d" number="%d">%s</segment>'
                     % (size, i, html.escape(msgid)))
    lines += ['</segments>', '</file>']
    return lines


def build_nzb(served_path, subject_name, file_size, seg_size, missing_parts,
              password=None):
    """Return NZB XML for a single file served by nserv.

    ``missing_parts`` is a set of 1-based part numbers to mark with ``!2`` so
    they are served only by nserv instance 2 (i.e. missing on the active
    instance-1 server, forcing a fallback). ``password`` is forwarded to
    build_multi_nzb (see there for how it reaches the daemon)."""
    return build_multi_nzb([(served_path, subject_name, file_size, seg_size,
                             missing_parts)], password=password)


def build_multi_nzb(members, password=None):
    """Return NZB XML containing one <file> block per member tuple
    (served_path, subject_name, file_size, seg_size, missing_parts).
    Member subject names MUST be distinct: duplicate parsed filenames make
    nzbget fall back to raw subjects and set ManyDupeFiles.

    ``password``, if given, is emitted as a <head><meta type="password">
    block - the real NZB convention indexers use to advertise an archive's
    password. NzbFile::Parse (daemon/queue/NzbFile.cpp) matches any <meta
    type="password"> element regardless of nesting and stores its text as
    nzbFile.GetPassword(); Scanner::AddFileToQueue then copies that into the
    queued NzbInfo's "*Unpack:Password" parameter (daemon/queue/Scanner.cpp).
    This happens on the SAME on-disk-file parse path whether the NZB arrived
    via directory scan or the RPC "append" method (DownloadXmlCommand ->
    Scanner::AddExternalFile writes the posted content to NzbDir and scans it
    exactly like a dropped file) - no RPC signature change is needed to carry
    a password end to end. StreamRepair.cpp later reads that same parameter
    for both the target (m_targetPassword, in CollectTargets) and each donor
    (DonorSource::Password, in CollectDonors) - both captured from the LIVE
    NzbInfo under the queue guard. The later ParseDonorNzb re-parse of the raw
    .nzb CANNOT see intake- or API-set passwords, so donor capture must happen
    from the live NzbInfo, not the re-parse."""
    import html
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<!DOCTYPE nzb PUBLIC "-//newzBin//DTD NZB 1.0//EN" '
        '"http://www.newzbin.com/DTD/nzb/nzb-1.0.dtd">',
        '<nzb xmlns="http://www.newzbin.com/DTD/2003/nzb">',
    ]
    if password:
        lines += ['<head>',
                  '<meta type="password">%s</meta>' % html.escape(password),
                  '</head>']
    for member in members:
        lines += _nzb_file_block(*member)
    lines.append('</nzb>')
    return '\n'.join(lines) + '\n'


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    p = s.getsockname()[1]
    s.close()
    return p


# --------------------------------------------------------------------------- #
# Targets
# --------------------------------------------------------------------------- #

class LocalTarget:
    """Run nserv + daemon as local subprocesses (Linux, macOS)."""
    name = 'local'

    def __init__(self, nzbget_bin, workdir):
        self.nzbget = nzbget_bin
        self.work = workdir
        self.procs = []

    def path(self, *parts):
        return os.path.join(self.work, *parts)

    def makedirs(self, *parts):
        d = self.path(*parts)
        os.makedirs(d, exist_ok=True)
        return d

    def write_file(self, rel, data):
        full = self.path(rel)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, 'wb') as f:
            f.write(data)

    def read_file(self, rel):
        with open(self.path(rel), 'rb') as f:
            return f.read()

    def exists(self, rel):
        return os.path.exists(self.path(rel))

    def find_files(self, *parts):
        """Return rel paths (work-relative, '/'-joined) of all files under the
        given subdir."""
        base = self.path(*parts)
        out = []
        if os.path.isdir(base):
            for dp, _, files in os.walk(base):
                for fn in files:
                    rel = os.path.relpath(os.path.join(dp, fn), self.work)
                    out.append(rel.replace(os.sep, '/'))
        return out

    def spawn(self, args, output_rel=None):
        if output_rel:
            with open(self.path(output_rel), 'wb') as output:
                p = subprocess.Popen(args, stdout=output, stderr=subprocess.STDOUT)
        else:
            p = subprocess.Popen(args, stdout=subprocess.DEVNULL,
                                  stderr=subprocess.DEVNULL)
        self.procs.append(p)
        return p

    def rpc_host(self):
        return '127.0.0.1'

    def forward_rpc(self, device_port):
        _ = device_port  # local: the daemon already listens on localhost; no-op

    def teardown(self, keep):
        for p in self.procs:
            try:
                p.kill()
            except Exception:
                pass
        if not keep and os.path.exists(self.work):
            shutil.rmtree(self.work, ignore_errors=True)


class AdbTarget:
    """Run nserv + daemon inside an Android emulator/device over adb.

    Files live under a device-local workdir; the RPC control port is forwarded
    back to the host so the same XML-RPC client works unchanged."""
    name = 'adb'
    DEVICE_ROOT = '/data/local/tmp/nzbget-dupefallback'

    def __init__(self, nzbget_device_path, workdir, serial=None, rpc_port=None):
        # nzbget_device_path is the path to the nzbget binary ALREADY on device
        self.nzbget = nzbget_device_path
        self.work = self.DEVICE_ROOT
        self.host_stage = workdir            # host-side staging dir
        self.serial = serial
        self.rpc_port = rpc_port
        self.forwarded = None
        os.makedirs(self.host_stage, exist_ok=True)
        self._adb('shell', 'rm', '-rf', self.work)
        self._adb('shell', 'mkdir', '-p', self.work)

    def _adb(self, *args):
        cmd = ['adb']
        if self.serial:
            cmd += ['-s', self.serial]
        cmd += list(args)
        return subprocess.run(cmd, capture_output=True, text=True)

    def path(self, *parts):
        return '/'.join([self.work] + list(parts))

    def makedirs(self, *parts):
        d = self.path(*parts)
        self._adb('shell', 'mkdir', '-p', d)
        return d

    def write_file(self, rel, data):
        full = self.path(rel)
        self._adb('shell', 'mkdir', '-p', full.rsplit('/', 1)[0])
        stage = os.path.join(self.host_stage, rel.replace('/', '_'))
        with open(stage, 'wb') as f:
            f.write(data)
        self._adb('push', stage, full)

    def read_file(self, rel):
        stage = os.path.join(self.host_stage, 'pull_' + rel.replace('/', '_'))
        self._adb('pull', self.path(rel), stage)
        with open(stage, 'rb') as f:
            return f.read()

    def exists(self, rel):
        r = self._adb('shell', 'ls', self.path(rel))
        return 'No such file' not in (r.stdout + r.stderr)

    def find_files(self, *parts):
        base = self.path(*parts)
        r = self._adb('shell', 'find', base, '-type', 'f')
        out = []
        for line in r.stdout.splitlines():
            line = line.strip()
            if line.startswith(self.work + '/'):
                out.append(line[len(self.work) + 1:])
        return out

    def spawn(self, args, output_rel=None):
        # launch detached on device; ports are device-local
        output = self._q(self.path(output_rel)) if output_rel else '/dev/null'
        remote = ' '.join(self._q(a) for a in args) + ' >' + output + ' 2>&1 &'
        self._adb('shell', remote)
        return None

    @staticmethod
    def _q(a):
        return "'" + str(a).replace("'", "'\\''") + "'" if ' ' in str(a) else str(a)

    def rpc_host(self):
        return '127.0.0.1'

    def forward_rpc(self, device_port):
        self.rpc_port = device_port
        self._adb('forward', 'tcp:%d' % device_port, 'tcp:%d' % device_port)
        self.forwarded = device_port

    def teardown(self, keep):
        self._adb('shell', 'pkill', '-f', self.nzbget)
        if self.forwarded:
            self._adb('forward', '--remove', 'tcp:%d' % self.forwarded)
        if not keep:
            self._adb('shell', 'rm', '-rf', self.work)
        shutil.rmtree(self.host_stage, ignore_errors=True)


# --------------------------------------------------------------------------- #
# Daemon control
# --------------------------------------------------------------------------- #

class Daemon:
    def __init__(self, target, nntp_port, rpc_port):
        self.t = target
        self.nntp_port = nntp_port
        self.rpc_port = rpc_port
        self.datadir = target.makedirs('data')
        for d in ('dst', 'inter', 'nzb', 'queue', 'tmp', 'scripts', 'web'):
            target.makedirs('main', d)
        self.conf_rel = 'nzbget.conf'
        self.proxy = None
        self.newznab = None
        self.fake_nntp = None

    def write_config(self, extra_options):
        w = self.t.path
        cfg = [
            'MainDir=%s' % w('main'),
            'DestDir=%s' % w('main', 'dst'),
            'InterDir=%s' % w('main', 'inter'),
            'NzbDir=%s' % w('main', 'nzb'),
            'QueueDir=%s' % w('main', 'queue'),
            'TempDir=%s' % w('main', 'tmp'),
            'LogFile=%s' % w('nzbget.log'),
            # Pin every path that nzbget would otherwise default to a
            # compiled-in location (e.g. /downloads/scripts): an uncreatable
            # ScriptDir/WebDir is a fatal config error that pauses the whole
            # queue, which cross-compiled (Android) builds trip over.
            'ScriptDir=%s' % w('main', 'scripts'),
            'WebDir=%s' % w('main', 'web'),
            'LockFile=%s' % w('nzbget.lock'),
            'ConfigTemplate=', 'RequiredDir=',
            'WriteLog=append', 'OutputMode=log', 'ControlIP=127.0.0.1',
            'ControlPort=%d' % self.rpc_port,
            'ControlUsername=', 'ControlPassword=',
            'Server1.Host=127.0.0.1', 'Server1.Port=%d' % self.nntp_port,
            'Server1.Connections=4', 'Server1.Level=0', 'Server1.Encryption=no',
            'DirectWrite=yes', 'ArticleCache=0', 'ContinuePartial=no',
            'FileNaming=nzb', 'DupeCheck=yes', 'NzbCleanupDisk=no',
            'HealthCheck=none', 'ArticleRetries=1', 'ParCheck=manual',
            'ParRename=no', 'RarRename=no', 'Unpack=no', 'DirectUnpack=no',
            'DirectRename=no', 'UpdateCheck=none',
        ] + extra_options
        self.t.write_file(self.conf_rel, ('\n'.join(cfg) + '\n').encode())

    def start_nserv(self, capture_requests=False, extra_args=(), port=None):
        # A single instance (-i 1) binds only nntp_port. Instance 1 already
        # returns "430 not found" for "!2" message-ids (its id 1 is not in the
        # server-list [2]), which is how a "missing" article is simulated on the
        # only server the daemon uses. A second instance would just bind
        # nntp_port+1 and risk colliding with the control port.
        self.t.spawn([self.t.nzbget, '--nserv', '-d', self.datadir,
                      '-p', str(port or self.nntp_port), '-i', '1',
                      '-v', '2' if capture_requests else '0'] + list(extra_args),
                     output_rel='nserv.log' if capture_requests else None)

    def start(self):
        self.t.spawn([self.t.nzbget, '-c', self.t.path(self.conf_rel), '-s'])

    def api(self):
        host = self.t.rpc_host()
        return ServerProxy('http://%s:%d/xmlrpc' % (host, self.rpc_port))

    def wait_ready(self, timeout=30):
        api = self.api()
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                api.status()
                return api
            except Exception:
                time.sleep(0.3)
        raise RuntimeError('nzbget did not become ready')

    def append(self, api, name, nzb_xml, paused, dupekey, score):
        content = base64.standard_b64encode(nzb_xml.encode()).decode()
        return api.append(name, content, 'test', 0, False, paused,
                           dupekey, score, 'score', [])

    def wait_history(self, api, name, timeout=180):
        deadline = time.time() + timeout
        while time.time() < deadline:
            for h in api.history():
                if h.get('NZBName') == name or h.get('NZBFilename') == name + '.nzb':
                    return h
            time.sleep(0.5)
        raise RuntimeError('timeout waiting for %s in history' % name)


class FlakyNntpProxy:
    """A news server in front of nserv that, on the first BODY request whose
    message-id contains ``trigger``, drops every open connection (leaving that
    request unanswered) and turns new connections away with "502 too many
    connections" for ``window`` seconds: a provider enforcing a per-user
    connection limit while another client holds the account's connections.
    With ``window`` None the server stays down for good."""

    def __init__(self, listen_port, upstream_port, trigger, window):
        self.upstream_port = upstream_port
        self.trigger = trigger.encode()
        self.window = window
        self.triggered_at = None
        self.rejected = 0
        self.live = []
        self.lock = threading.Lock()
        self.srv = socket.socket()
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind(('127.0.0.1', listen_port))
        self.srv.listen(64)
        threading.Thread(target=self._serve, daemon=True).start()

    @staticmethod
    def _kill(sock):
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        sock.close()

    def _serve(self):
        while True:
            try:
                client, _ = self.srv.accept()
            except OSError:
                return
            with self.lock:
                if self.triggered_at and (self.window is None or
                                          time.time() - self.triggered_at < self.window):
                    self.rejected += 1
                    client.sendall(b'502 too many connections\r\n')
                    self._kill(client)
                    continue
                upstream = socket.create_connection(('127.0.0.1', self.upstream_port))
                self.live += [client, upstream]
            threading.Thread(target=self._pipe, args=(client, upstream, True), daemon=True).start()
            threading.Thread(target=self._pipe, args=(upstream, client, False), daemon=True).start()

    def _pipe(self, src, dst, from_client):
        try:
            while True:
                data = src.recv(65536)
                if not data:
                    break
                if from_client and b'BODY <' in data and self.trigger in data:
                    with self.lock:
                        if not self.triggered_at:
                            self.triggered_at = time.time()
                            for sock in self.live:
                                self._kill(sock)
                            self.live = []
                            return
                dst.sendall(data)
        except OSError:
            pass
        finally:
            self._kill(src)
            self._kill(dst)

    def close(self):
        self._kill(self.srv)
        with self.lock:
            for sock in self.live:
                self._kill(sock)


class CorruptingNntpProxy:
    """A news server in front of nserv that changes one byte inside the yEnc
    data of every BODY response for a message-id containing ``marker``,
    leaving the declared crc32 as it was: a provider serving a corrupt copy."""

    def __init__(self, listen_port, upstream_port, marker):
        self.upstream_port = upstream_port
        self.marker = marker.encode()
        self.corrupted = 0
        self.srv = socket.socket()
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind(('127.0.0.1', listen_port))
        self.srv.listen(64)
        threading.Thread(target=self._serve, daemon=True).start()

    def _serve(self):
        while True:
            try:
                client, _ = self.srv.accept()
            except OSError:
                return
            upstream = socket.create_connection(('127.0.0.1', self.upstream_port))
            state = {'corrupt': False}
            threading.Thread(target=self._requests, args=(client, upstream, state), daemon=True).start()
            threading.Thread(target=self._responses, args=(upstream, client, state), daemon=True).start()

    def _requests(self, client, upstream, state):
        try:
            while True:
                data = client.recv(65536)
                if not data:
                    break
                if b'BODY <' in data and self.marker in data:
                    state['corrupt'] = True
                upstream.sendall(data)
        except OSError:
            pass

    def _responses(self, upstream, client, state):
        buf = b''
        try:
            while True:
                data = upstream.recv(65536)
                if not data:
                    break
                if not state['corrupt']:
                    client.sendall(data)
                    continue
                buf += data
                head = buf.find(b'=ypart')
                line_end = buf.find(b'\r\n', head) if head >= 0 else -1
                if line_end < 0 or len(buf) < line_end + 500:
                    continue
                pos = line_end + 300
                # a plain data byte: never a line break, escape or dot-stuffing
                if buf[pos] not in b'\r\n=.' and (buf[pos] + 1) % 256 not in b'\r\n=.\0':
                    buf = buf[:pos] + bytes([(buf[pos] + 1) % 256]) + buf[pos + 1:]
                    self.corrupted += 1
                state['corrupt'] = False
                client.sendall(buf)
                buf = b''
        except OSError:
            pass

    def close(self):
        try:
            self.srv.close()
        except OSError:
            pass


class RewritingNntpProxy:
    """A news server in front of nserv that rewrites ``old`` to ``new`` in
    its replies (same length): a provider that says "451" for a missing
    article instead of "430"."""

    def __init__(self, listen_port, upstream_port, old, new):
        assert len(old) == len(new)
        self.upstream_port = upstream_port
        self.old, self.new = old, new
        self.rewritten = 0
        self.srv = socket.socket()
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind(('127.0.0.1', listen_port))
        self.srv.listen(64)
        threading.Thread(target=self._serve, daemon=True).start()

    def _serve(self):
        while True:
            try:
                client, _ = self.srv.accept()
            except OSError:
                return
            upstream = socket.create_connection(('127.0.0.1', self.upstream_port))
            threading.Thread(target=self._pipe, args=(client, upstream, False), daemon=True).start()
            threading.Thread(target=self._pipe, args=(upstream, client, True), daemon=True).start()

    def _pipe(self, src, dst, rewrite):
        try:
            while True:
                data = src.recv(65536)
                if not data:
                    break
                if rewrite and self.old in data:
                    self.rewritten += data.count(self.old)
                    data = data.replace(self.old, self.new)
                dst.sendall(data)
        except OSError:
            pass
        finally:
            for sock in (src, dst):
                try:
                    sock.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                sock.close()

    def close(self):
        try:
            self.srv.close()
        except OSError:
            pass


class DelayingNntpProxy(RewritingNntpProxy):
    """A news server in front of nserv that holds every request naming a
    marker for that marker's delay (seconds) before passing it on: a slow
    server, or a duplicate whose articles are slow to come (in practice,
    requests for articles that no server has any more, each waiting out its
    timeout). ``delays`` is a list of (marker, seconds)."""

    def __init__(self, listen_port, upstream_port, delays):
        self.delays = delays
        self.delayed = 0
        super().__init__(listen_port, upstream_port, b'', b'')

    def _pipe(self, src, dst, rewrite):
        try:
            while True:
                data = src.recv(65536)
                if not data:
                    break
                if not rewrite:
                    for marker, delay in self.delays:
                        if marker in data:
                            self.delayed += 1
                            time.sleep(delay)
                            break
                dst.sendall(data)
        except OSError:
            pass
        finally:
            for sock in (src, dst):
                try:
                    sock.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                sock.close()
class FakeNntp:
    """A news server that holds exactly the articles in ``alive`` (message-ids without
    the angle brackets): STAT and BODY (a yEnc article with a valid checksum) answer
    for them, 430 for the others. ``delays`` maps a message-id prefix to seconds to
    wait before answering; ``reply451`` ids answer 451. Counts what was asked."""

    def __init__(self, port, alive=()):
        outer = self
        self.alive = set(alive)
        self.delays = {}
        self.reply451 = set()
        self.stats = 0
        self.bodies = 0
        self.sessions = 0
        self.max_sessions = 0
        self.open_sessions = 0
        self.lock = threading.Lock()

        class Handler(socketserver.StreamRequestHandler):
            def handle(self):
                with outer.lock:
                    outer.sessions += 1
                    outer.open_sessions += 1
                    outer.max_sessions = max(outer.max_sessions, outer.open_sessions)
                try:
                    self.wfile.write(b'200 fake\r\n')
                    for raw in self.rfile:
                        line = raw.decode('latin1').strip()
                        cmd, _, arg = line.partition(' ')
                        cmd = cmd.upper()
                        mid = arg.strip().strip('<>')
                        if cmd == 'STAT' or cmd == 'BODY':
                            for prefix, delay in outer.delays.items():
                                if mid.startswith(prefix):
                                    time.sleep(delay)
                            with outer.lock:
                                if cmd == 'STAT':
                                    outer.stats += 1
                                else:
                                    outer.bodies += 1
                            if mid in outer.reply451:
                                self.wfile.write(b'451 not here\r\n')
                            elif mid not in outer.alive:
                                self.wfile.write(b'430 no such article\r\n')
                            elif cmd == 'STAT':
                                self.wfile.write(('223 0 <%s>\r\n' % mid).encode())
                            else:
                                self.wfile.write(('222 0 <%s>\r\n' % mid).encode() + FakeNntp.article(mid) + b'.\r\n')
                        elif cmd == 'GROUP':
                            self.wfile.write(('211 1 1 1 %s\r\n' % arg).encode())
                        elif cmd == 'MODE':
                            self.wfile.write(b'200 reader\r\n')
                        elif cmd == 'QUIT':
                            self.wfile.write(b'205 bye\r\n')
                            break
                        else:
                            self.wfile.write(b'500 what\r\n')
                        self.wfile.flush()
                finally:
                    with outer.lock:
                        outer.open_sessions -= 1

        class Server(socketserver.ThreadingTCPServer):
            allow_reuse_address = True
            daemon_threads = True

        self.server = Server(('127.0.0.1', port), Handler)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    @staticmethod
    def article(mid):
        data = (mid * 4).encode()[:64]
        enc = bytearray()
        for b in data:
            c = (b + 42) & 255
            if c in (0, 10, 13, 61):
                enc += b'=' + bytes([(c + 64) & 255])
            else:
                enc.append(c)
        crc = zlib.crc32(data) & 0xffffffff
        return (b'=ybegin part=1 total=1 line=128 size=%d name=a.mkv\r\n=ypart begin=1 end=%d\r\n' % (len(data), len(data))
                + bytes(enc) + b'\r\n=yend size=%d part=1 pcrc32=%08x\r\n' % (len(data), crc))

    def close(self):
        self.server.shutdown()
        self.server.server_close()


class FakeNewznab:
    """A Newznab indexer for the duplicate search: an HTTP server whose
    ``respond(params, path)`` returns (status, body bytes); every request is
    recorded in ``requests`` (the query parameters, apikey included)."""

    def __init__(self):
        outer = self
        self.requests = []
        self.lock = threading.Lock()
        self.respond = lambda params, path: (200, newznab_xml([]))

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                parts = urllib.parse.urlsplit(self.path)
                params = {k: v[0] for k, v in urllib.parse.parse_qs(parts.query).items()}
                with outer.lock:
                    outer.requests.append(dict(params, _path=parts.path))
                status, body = outer.respond(params, parts.path)
                self.send_response(status)
                self.send_header('Content-Type', 'application/xml')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *args):
                pass

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.port = self.server.server_address[1]
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def close(self):
        self.server.shutdown()
        self.server.server_close()


def newznab_xml(items, error=None):
    """A Newznab search page; items are dicts (title, link, size, grabs, date, indexer)."""
    if error:
        return ('<?xml version="1.0" encoding="UTF-8"?>\n<error code="%d" description="%s"/>' % error).encode()
    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<rss xmlns:newznab="http://www.newznab.com/DTD/2010/feeds/attributes/" version="2.0"><channel>',
           '<title>Fake</title>']
    for it in items:
        out.append('<item><title>%s</title><link>%s</link><size>%d</size>'
                   '<newznab:attr name="size" value="%d"/><newznab:attr name="grabs" value="%d"/>'
                   '<newznab:attr name="usenetdate" value="%s"/><newznab:attr name="hydraIndexerName" value="%s"/></item>'
                   % (it['title'], it['link'], it.get('size', 0), it.get('size', 0), it.get('grabs', 0),
                      it.get('date', 'Tue, 10 Jun 2025 01:10:05 +0000'), it.get('indexer', 'Fake')))
    out.append('</channel></rss>')
    return '\n'.join(out).encode()


# --------------------------------------------------------------------------- #
# Scenarios
# --------------------------------------------------------------------------- #

def _payload(size, seed):
    r = random.Random(seed)
    return bytes(r.getrandbits(8) for _ in range(size)) if size < 1 else \
        r.randbytes(size) if hasattr(r, 'randbytes') else \
        bytes(bytearray(r.getrandbits(8) for _ in range(size)))


def _place_copy(target, subdir, data, filename='file.bin'):
    """Write a payload copy under data/<subdir>/<filename> and return the
    served path (relative to the nserv data dir)."""
    target.write_file(os.path.join('data', subdir, filename), data)
    return '%s/%s' % (subdir, filename)


def scenario_complementary(daemon, t):
    """Primary missing parts 3,5,7; donor missing 2,8 (different) — only
    together do they complete. Byte-identical result expected."""
    size, seg = 5_000_000, 500_000
    data = _payload(size, 849)
    pp = _place_copy(t, 'primA', data)
    dp = _place_copy(t, 'primB', data)
    primary = build_nzb(pp, 'ReleaseA.bin', size, seg, {3, 5, 7})
    donor = build_nzb(dp, 'obf-b.bin', size, seg, {2, 8})
    api = daemon.wait_ready()
    daemon.append(api, 'DonorB', donor, True, 'comp-key', 50)
    daemon.append(api, 'ReleaseA', primary, False, 'comp-key', 100)
    h = daemon.wait_history(api, 'ReleaseA')
    ok = h['Status'].startswith('SUCCESS')
    recov = int(h.get('DupeRecoveredArticles', 0))
    integ = _verify_output(t, data)
    return ('complementary', ok and integ and recov == 3,
            'status=%s recovered=%d integrity=%s' % (h['Status'], recov, integ))


def scenario_cutover(daemon, t):
    """Primary missing 10 of 20 articles => file cuts over to the duplicate."""
    size, seg = 10_000_000, 500_000
    data = _payload(size, 1206)
    pp = _place_copy(t, 'cutA', data)
    dp = _place_copy(t, 'cutB', data)
    primary = build_nzb(pp, 'CutA.bin', size, seg, set(range(2, 12)))
    donor = build_nzb(dp, 'obf-cut.bin', size, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonCut', donor, True, 'cut-key', 50)
    daemon.append(api, 'CutA', primary, False, 'cut-key', 100)
    h = daemon.wait_history(api, 'CutA')
    ok = h['Status'].startswith('SUCCESS')
    recov = int(h.get('DupeRecoveredArticles', 0))
    cut = _grep_log(t, 'Leading with duplicate collections')
    integ = _verify_output(t, data)
    # Only REACTIVE recoveries count: fresh articles served proactively after
    # the cutover trips must not inflate the metric, so the count can never
    # exceed the 10 articles the primary is actually missing (proactive
    # inflation would push it towards 19) (any proactive round, incl. donor
    # round >= 2 under the primary-last order). The exact value below 10 is
    # timing-dependent - it is the number of primary failures already in
    # flight when cutover trips (>= the 3 recoveries that trip it), which
    # grows under system load beyond the 4 concurrent connections.
    return ('cutover', ok and integ and 3 <= recov <= 10 and cut == 1,
            'status=%s recovered=%d cutover_logs=%d integrity=%s'
            % (h['Status'], recov, cut, integ))


def scenario_leadswitch(daemon, t):
    """Primary AND the top-scored duplicate share the same hole (parts 2-12);
    a second, lower-scored duplicate covers it. After a few consecutive lead
    misses the file must switch its lead to the next duplicate instead of
    paying a wasted fetch per article on the holed one, and still complete
    byte-identically from the second duplicate."""
    size, seg = 10_000_000, 500_000
    data = _payload(size, 4242)
    pp = _place_copy(t, 'leadP', data)
    dhp = _place_copy(t, 'leadH', data)
    dlp = _place_copy(t, 'leadL', data)
    holes = set(range(2, 13))
    primary = build_nzb(pp, 'LeadA.bin', size, seg, holes)
    donor_high = build_nzb(dhp, 'obf-lh.bin', size, seg, holes)
    donor_low = build_nzb(dlp, 'obf-ll.bin', size, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'LeadHigh', donor_high, True, 'lead-key', 60)
    daemon.append(api, 'LeadLow', donor_low, True, 'lead-key', 50)
    daemon.append(api, 'LeadA', primary, False, 'lead-key', 100)
    h = daemon.wait_history(api, 'LeadA')
    ok = h['Status'].startswith('SUCCESS')
    recov = int(h.get('DupeRecoveredArticles', 0))
    # exactly ONE switch: more would mean stale in-flight failures of the old
    # lead cascade-demoted the new (complete) lead - the regression the
    # per-article lead snapshot exists to prevent
    switched = _grep_log(t, 'Switching lead duplicate collection')
    integ = _verify_output(t, data)
    # Only REACTIVE recoveries count (proactive traffic after cutover must
    # not inflate the metric), bounding the count by the 11 articles the
    # primary is missing; the exact value below that is timing-dependent -
    # how many articles were already mid-fallback when the proactive
    # pre-assignment took over grows under system load (any proactive round,
    # incl. donor round >= 2 under the primary-last order).
    return ('leadswitch', ok and integ and 3 <= recov <= 11 and switched == 1,
            'status=%s recovered=%d lead_switch_logs=%d integrity=%s'
            % (h['Status'], recov, switched, integ))


def scenario_cutovertruth(daemon, t):
    """Counter honesty under cutover: the lead duplicate misses articles the
    PRIMARY actually has (parts 20-23); the second duplicate serves them at
    round 2. Those proactive successes prove nothing about the primary and
    must NOT count as recovered - only reactive recoveries of the primary's
    own 11 missing articles may. Without the !DupeProactive guard the
    primary-last cutover order counts them (recov ~12-15 here)."""
    size, seg = 20_000_000, 500_000          # 40 articles
    data = _payload(size, 1207)
    pp = _place_copy(t, 'ctrA', data)
    dhp = _place_copy(t, 'ctrH', data)
    dlp = _place_copy(t, 'ctrL', data)
    primary = build_nzb(pp, 'CtrA.bin', size, seg, set(range(2, 13)))       # 11 missing
    donor_high = build_nzb(dhp, 'obf-cth.bin', size, seg, set(range(20, 24)))
    donor_low = build_nzb(dlp, 'obf-ctl.bin', size, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'CtrHigh', donor_high, True, 'ctr-key', 60)
    daemon.append(api, 'CtrLow', donor_low, True, 'ctr-key', 50)
    daemon.append(api, 'CtrA', primary, False, 'ctr-key', 100)
    h = daemon.wait_history(api, 'CtrA')
    ok = h['Status'].startswith('SUCCESS')
    recov = int(h.get('DupeRecoveredArticles', 0))
    switched = _grep_log(t, 'Switching lead duplicate collection')
    integ = _verify_output(t, data)
    # bound = the primary's 11 missing articles: proactive traffic (incl. the
    # lead's private holes at parts 20-23, served by the second duplicate)
    # never counts, so the bound is timing-independent. The lead's 4
    # consecutive misses may legitimately rotate the lead once.
    return ('cutovertruth', ok and integ and 3 <= recov <= 11 and switched <= 1,
            'status=%s recovered=%d lead_switch_logs=%d integrity=%s'
            % (h['Status'], recov, switched, integ))


def scenario_manydonors(daemon, t, ndonors=18):
    """More duplicates than the donor cache (16) => exercises cache eviction
    (regression for the use-after-free crash). Primary missing 2,3,4."""
    size, seg = 3_000_000, 500_000
    data = _payload(size, 77)
    pp = _place_copy(t, 'manyPrim', data)
    primary = build_nzb(pp, 'ManyPrim.bin', size, seg, {2, 3, 4})
    api = daemon.wait_ready()
    for i in range(ndonors):
        dp = _place_copy(t, 'manyD%02d' % i, data)
        donor = build_nzb(dp, 'obf-d%02d.bin' % i, size, seg, set())
        daemon.append(api, 'ManyD%02d' % i, donor, True, 'many-key', 50 + i)
    daemon.append(api, 'ManyPrim', primary, False, 'many-key', 100)
    h = daemon.wait_history(api, 'ManyPrim')
    ok = h['Status'].startswith('SUCCESS')
    recov = int(h.get('DupeRecoveredArticles', 0))
    integ = _verify_output(t, data)
    # crash regression: the daemon must still answer RPC afterwards
    alive = True
    try:
        api.status()
    except Exception:
        alive = False
    return ('manydonors', ok and integ and recov >= 3 and alive,
            'status=%s recovered=%d integrity=%s daemon_alive=%s'
            % (h['Status'], recov, integ, alive))


def scenario_stream(daemon, t):
    """Primary (.mkv, 500 KB parts) missing a middle block; the donor posted
    the SAME content split into 250 KB parts (different article count) and
    sits paused in the queue under the same dupe-key. Article-level fallback
    cannot borrow across segmentations, so the post-processing stream repair
    must fetch the missing byte ranges from the donor. A DECOY duplicate of
    the same byte size but different content carries a HIGHER dupe-score, so
    it is tried first and must be rejected by the identity probe (the
    negative half of the test: no corruption from a same-size impostor).
    Every hole gets filled and there is no par2 here, so the byte-based
    health recount (StreamRepairController::RepairCompleted crediting each
    fully-repaired file's encoded failed size back to health) drives
    CalcHealth() to 1000: the history status now completes as SUCCESS
    (moved to the final destination) instead of parking at FAILURE/HEALTH -
    that status flip is asserted directly below, alongside byte integrity,
    the repair logs and the DupeRecoveredArticles counter."""
    size, seg_primary, seg_donor = 6_000_000, 500_000, 250_000
    data = _payload(size, 4242)
    decoy_data = _payload(size, 999)  # same size, different bytes
    pp = _place_copy(t, 'streamA', data, 'file.mkv')
    dp = _place_copy(t, 'streamB', data, 'file.mkv')
    xp = _place_copy(t, 'streamX', decoy_data, 'file.mkv')
    primary = build_nzb(pp, 'StreamA.mkv', size, seg_primary, {5, 6})
    donor = build_nzb(dp, 'obf-stream.mkv', size, seg_donor, set())
    decoy = build_nzb(xp, 'obf-decoy.mkv', size, seg_donor, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DecoyStream', decoy, True, 'stream-key', 75)
    daemon.append(api, 'DonStream', donor, True, 'stream-key', 50)
    daemon.append(api, 'StreamA', primary, False, 'stream-key', 100)
    h = daemon.wait_history(api, 'StreamA')
    recov = int(h.get('DupeRecoveredArticles', 0))
    queued = _grep_log(t, 'Queueing stream repair')
    repaired = _grep_log(t, 'donor article(s)')
    rejected = _grep_log(t, 'content identity not confirmed')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    success = 'SUCCESS' in h['Status']
    return ('stream',
            success and integ and recov == 2 and queued >= 1 and repaired >= 1 and rejected >= 1,
            'status=%s recovered=%d queued_logs=%d repair_logs=%d rejected_logs=%d integrity=%s'
            % (h['Status'], recov, queued, repaired, rejected, integ))


def scenario_streamtimeout(daemon, t):
    """DupeStreamTimeout: the only duplicate is stalled (every request for its
    articles waits 8 s, longer than the timeout), so the repair of the
    primary's 4 MB hole recovers nothing. With DupeStreamTimeout=5 it stops
    after 5 seconds without progress, logs why, and the download finishes at
    once as a failure (no par2 here) instead of sitting in stream repair, so a
    client can grab another release."""
    size, seg_primary, seg_donor = 6_000_000, 500_000, 250_000
    data = _payload(size, 4343)
    pp = _place_copy(t, 'slowA', data, 'file.mkv')
    dp = _place_copy(t, 'slowB', data, 'file.mkv')
    primary = build_nzb(pp, 'SlowA.mkv', size, seg_primary, set(range(2, 10)))
    donor = build_nzb(dp, 'obf-slow.mkv', size, seg_donor, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonSlow', donor, True, 'slow-key', 50)
    daemon.append(api, 'SlowA', primary, False, 'slow-key', 100)
    start = time.time()
    h = daemon.wait_history(api, 'SlowA', timeout=120)
    took = time.time() - start
    stopped = _grep_log(t, 'nothing recovered for 5 seconds (option DupeStreamTimeout)')
    interrupted = _grep_log(t, 'Stream repair interrupted')
    ok = stopped == 1 and interrupted == 0 and 'SUCCESS' not in h['Status'] and took < 40 and \
        daemon.proxy.delayed > 0
    return ('streamtimeout', ok, 'status=%s took=%.0fs stopped_logs=%d interrupted_logs=%d delayed=%d'
            % (h['Status'], took, stopped, interrupted, daemon.proxy.delayed))


def scenario_streamslowprogress(daemon, t):
    """DupeStreamTimeout counts time WITHOUT progress: a duplicate that answers
    slowly but keeps delivering is not cut off, even
    when the whole repair takes longer than the timeout (5 s), as long as it is
    not 3 times slower than downloading: the download is slow here too (2 s
    per request, the duplicate 0.5 s). The file is repaired in full."""
    size, seg_primary, seg_donor = 6_000_000, 500_000, 250_000
    data = _payload(size, 4343)
    pp = _place_copy(t, 'slowA', data, 'file.mkv')
    dp = _place_copy(t, 'slowB', data, 'file.mkv')
    primary = build_nzb(pp, 'SlowA.mkv', size, seg_primary, set(range(2, 10)))
    donor = build_nzb(dp, 'obf-slow.mkv', size, seg_donor, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonSlow', donor, True, 'slow-key', 50)
    daemon.append(api, 'SlowA', primary, False, 'slow-key', 100)
    h = daemon.wait_history(api, 'SlowA', timeout=180)
    stopped = _grep_log(t, '(option DupeStreamTimeout)')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    ok = stopped == 0 and integ and 'SUCCESS' in h['Status'] and daemon.proxy.delayed > 5
    return ('streamslowprogress', ok, 'status=%s stopped_logs=%d integrity=%s delayed=%d'
            % (h['Status'], stopped, integ, daemon.proxy.delayed))


def scenario_streamtooslow(daemon, t):
    """A repair that keeps recovering, but so slowly that the rest would take
    more than 3 times as long as downloading it at nzbget's average speed, is
    stopped (another release is quicker): the download itself is fast here,
    every request to the duplicate waits 1 s. (A 40 MB download first gives
    nzbget an average speed to compare with.)"""
    warm = 40_000_000
    wp = _place_copy(t, 'warm', _payload(warm, 77), 'file.bin')
    api = daemon.wait_ready()
    daemon.append(api, 'Warm', build_nzb(wp, 'warm.bin', warm, 500_000, set()), False, 'warm-key', 0)
    daemon.wait_history(api, 'Warm')
    size, seg_primary, seg_donor = 6_000_000, 500_000, 250_000
    data = _payload(size, 4343)
    pp = _place_copy(t, 'slowA', data, 'file.mkv')
    dp = _place_copy(t, 'slowB', data, 'file.mkv')
    primary = build_nzb(pp, 'SlowA.mkv', size, seg_primary, set(range(2, 10)))
    donor = build_nzb(dp, 'obf-slow.mkv', size, seg_donor, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonSlow', donor, True, 'slow-key', 50)
    daemon.append(api, 'SlowA', primary, False, 'slow-key', 100)
    h = daemon.wait_history(api, 'SlowA', timeout=180)
    slow = _grep_log(t, 'more than 3 times a download at')
    stalled = _grep_log(t, 'nothing recovered for')
    ok = slow == 1 and stalled == 0 and 'SUCCESS' not in h['Status']
    return ('streamtooslow', ok, 'status=%s too_slow_logs=%d stalled_logs=%d' % (h['Status'], slow, stalled))


def scenario_streamdeaddonor(daemon, t):
    """Before stream repair reads from a duplicate, a few of its articles are
    checked (STAT) on the servers: a dead duplicate (none of its articles
    exists any more) scored above the good one is skipped at once instead of
    being asked for every missing range, and the good one repairs the file."""
    size, seg_primary, seg_donor = 6_000_000, 500_000, 250_000
    data = _payload(size, 4545)
    pp = _place_copy(t, 'deadA', data, 'file.mkv')
    gp = _place_copy(t, 'deadG', data, 'file.mkv')
    xp = _place_copy(t, 'deadX', data, 'file.mkv')
    primary = build_nzb(pp, 'DeadA.mkv', size, seg_primary, {5, 6})
    good = build_nzb(gp, 'obf-good.mkv', size, seg_donor, set())
    dead = build_nzb(xp, 'obf-dead.mkv', size, seg_donor, set(range(size // seg_donor)))
    api = daemon.wait_ready()
    daemon.append(api, 'DeadDonor', dead, True, 'dead-key', 75)
    daemon.append(api, 'GoodDonor', good, True, 'dead-key', 50)
    daemon.append(api, 'DeadA', primary, False, 'dead-key', 100)
    h = daemon.wait_history(api, 'DeadA')
    skipped = _grep_log(t, 'Skipping duplicate DeadDonor for stream repair: none of 10 sampled articles')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    ok = skipped == 1 and integ and 'SUCCESS' in h['Status']
    return ('streamdeaddonor', ok, 'status=%s skipped_logs=%d integrity=%s' % (h['Status'], skipped, integ))


def scenario_liveoverlap(daemon, t):
    """DupeArticleFallback=live: FileA (differently-segmented donor queued)
    completes with a hole while the big FileB still downloads (the global
    DownloadRate throttle keeps B busy). The live pass must repair A DURING
    the download - asserted by log order: 'Starting live stream repair'
    strictly before the collection's 'completely downloaded' line - while
    the post-processing stage stays the accounting authority, taking the
    fully repaired release to SUCCESS with byte-identical output."""
    size_a, seg_a = 2_000_000, 200_000
    size_b, seg_b = 24_000_000, 500_000
    data_a = _payload(size_a, 7401)
    data_b = _payload(size_b, 7402)
    pa = _place_copy(t, 'liveA', data_a, 'a.mkv')
    pb = _place_copy(t, 'liveB', data_b, 'b.mkv')
    da = _place_copy(t, 'liveDon', data_a, 'a.mkv')
    primary = build_multi_nzb([
        (pa, 'LiveA.mkv', size_a, seg_a, {4, 5, 6}),
        (pb, 'LiveB.mkv', size_b, seg_b, set()),
    ])
    donor = build_nzb(da, 'obf-live.mkv', size_a, 160_000, set())
    api = daemon.wait_ready()
    daemon.append(api, 'LiveDonor', donor, True, 'live-key', 50)
    daemon.append(api, 'LiveMain', primary, False, 'live-key', 100)
    h = daemon.wait_history(api, 'LiveMain')
    ok = h['Status'].startswith('SUCCESS')
    live = _grep_log(t, 'Starting live stream repair')
    overlapped = _log_before(t, 'Starting live stream repair',
                             'completely downloaded')
    integ = (_verify_output(t, data_a, '.mkv', dirs=(('main', 'dst'), ('main', 'inter'))) and
             _verify_output(t, data_b, '.mkv', dirs=(('main', 'dst'), ('main', 'inter'))))
    return ('liveoverlap', ok and integ and live == 1 and overlapped,
            'status=%s live_logs=%d overlapped=%s integrity=%s'
            % (h['Status'], live, overlapped, integ))


def scenario_livegate(daemon, t):
    """Same shape as liveoverlap but DupeArticleFallback=stream: the live
    pass must NOT run (option gate); the repair happens in post-processing
    as before and the release still completes SUCCESS byte-identically."""
    size_a, seg_a = 2_000_000, 200_000
    size_b, seg_b = 4_000_000, 500_000
    data_a = _payload(size_a, 7403)
    data_b = _payload(size_b, 7404)
    pa = _place_copy(t, 'gateA', data_a, 'a.mkv')
    pb = _place_copy(t, 'gateB', data_b, 'b.mkv')
    da = _place_copy(t, 'gateDon', data_a, 'a.mkv')
    primary = build_multi_nzb([
        (pa, 'GateA.mkv', size_a, seg_a, {4, 5, 6}),
        (pb, 'GateB.mkv', size_b, seg_b, set()),
    ])
    donor = build_nzb(da, 'obf-gate.mkv', size_a, 160_000, set())
    api = daemon.wait_ready()
    daemon.append(api, 'GateDonor', donor, True, 'gate-key', 50)
    daemon.append(api, 'GateMain', primary, False, 'gate-key', 100)
    h = daemon.wait_history(api, 'GateMain')
    ok = h['Status'].startswith('SUCCESS')
    live = _grep_log(t, 'Starting live stream repair')
    repaired = _grep_log(t, 'donor article(s)')
    integ = (_verify_output(t, data_a, '.mkv', dirs=(('main', 'dst'), ('main', 'inter'))) and
             _verify_output(t, data_b, '.mkv', dirs=(('main', 'dst'), ('main', 'inter'))))
    return ('livegate', ok and integ and live == 0 and repaired >= 1,
            'status=%s live_logs=%d repair_logs=%d integrity=%s'
            % (h['Status'], live, repaired, integ))


def scenario_livelastfile(daemon, t):
    """DupeArticleFallback=live with a SINGLE damaged file: when its job is
    captured no other file remains queued, so the live dispatch is skipped
    (the post-processing stage starts moments later and repairs it there) -
    asserts the last-file guard against a pointless race with the imminent
    post-processing stage."""
    size, seg = 2_000_000, 200_000
    data = _payload(size, 7405)
    pp = _place_copy(t, 'lastA', data, 'a.mkv')
    dp = _place_copy(t, 'lastDon', data, 'a.mkv')
    primary = build_nzb(pp, 'LastA.mkv', size, seg, {4, 5, 6})
    donor = build_nzb(dp, 'obf-last.mkv', size, 160_000, set())
    api = daemon.wait_ready()
    daemon.append(api, 'LastDonor', donor, True, 'last-key', 50)
    daemon.append(api, 'LastMain', primary, False, 'last-key', 100)
    h = daemon.wait_history(api, 'LastMain')
    ok = h['Status'].startswith('SUCCESS')
    live = _grep_log(t, 'Starting live stream repair')
    repaired = _grep_log(t, 'donor article(s)')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return ('livelastfile', ok and integ and live == 0 and repaired >= 1,
            'status=%s live_logs=%d repair_logs=%d integrity=%s'
            % (h['Status'], live, repaired, integ))


def scenario_repost(daemon, t):
    """M1 same-bytes matching: a 4-member "release" (three equal-size rar
    volumes + a small par2) where the payloads are random bytes standing in
    for a PASSWORD-PROTECTED, COMPRESSED archive - stream repair never
    interprets them, which is the point. The primary posting is missing
    blocks in part01 and in the par2; the donor is a REPOST: byte-identical
    members under the same names, cut into different article sizes. Suffix/
    name pairing must repair the damaged archive volume. The PAR2 file must
    remain exactly as downloaded, with no donor PAR2 requests: names, sizes,
    and partial byte identity do not authorize borrowing another PAR set.
    Expected history status is FAILURE/PAR because the stand-in parity is
    opaque random bytes. Capture nserv requests when running this scenario."""
    seg_primary, seg_donor = 500_000, 300_000
    vol = 1_500_000
    members = [
        ('repostA/x.part01.rar', 'Rel.part01.rar', vol, seg_primary, {2}),
        ('repostA/x.part02.rar', 'Rel.part02.rar', vol, seg_primary, set()),
        ('repostA/x.part03.rar', 'Rel.part03.rar', vol, seg_primary, set()),
        ('repostA/x.par2', 'Rel.vol00+01.par2', 80_000, 70_000, {1}),
    ]
    payloads = {}
    for i, m in enumerate(members):
        data = _payload(m[2], 7000 + i)
        payloads[m[1]] = data
        t.write_file(os.path.join('data', m[0]), data)

    donor_members = [(m[0].replace('repostA', 'repostB'), m[1], m[2], seg_donor, set())
                     for m in members]
    for m in donor_members:
        t.write_file(os.path.join('data', m[0]), payloads[m[1]])

    primary = build_multi_nzb(members)
    donor = build_multi_nzb(donor_members)
    api = daemon.wait_ready()
    daemon.append(api, 'DonRepost', donor, True, 'repost-key', 50)
    daemon.append(api, 'RelRepost', primary, False, 'repost-key', 100)
    h = daemon.wait_history(api, 'RelRepost')
    recov = int(h.get('DupeRecoveredArticles', 0))
    queued = _grep_log(t, 'Queueing stream repair')
    repaired = _grep_log(t, 'donor article(s)')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ_rar = _verify_output(t, payloads['Rel.part01.rar'], '.rar', dirs=both_dirs)
    par_payload = payloads['Rel.vol00+01.par2']
    untouched_par = _verify_output(t, b'\0' * 70_000 + par_payload[70_000:],
                                    '.par2', dirs=both_dirs)
    intact_2 = _verify_output(t, payloads['Rel.part02.rar'], '.rar', dirs=both_dirs)
    intact_3 = _verify_output(t, payloads['Rel.part03.rar'], '.rar', dirs=both_dirs)
    # nserv flushes command output every 100 ms; wait for the final fetch.
    time.sleep(0.25)
    requests = re.findall(r'Received: (?:BODY|ARTICLE) [^\r\n]*',
                          t.read_file('nserv.log').decode(errors='replace'))
    donor_par = sum('repostB/x.par2?' in request for request in requests)
    donor_rar = sum('repostB/x.part01.rar?' in request for request in requests)
    return ('repost',
            integ_rar and untouched_par and intact_2 and intact_3 and
            donor_rar > 0 and donor_par == 0 and
            h['Status'] == 'FAILURE/PAR' and recov == 1 and queued == 1 and repaired >= 1,
            'status=%s recovered=%d queued_logs=%d repair_logs=%d '
            'rar=%s par2_untouched=%s intact=%s/%s donor_rar=%d donor_par2=%d'
            % (h['Status'], recov, queued, repaired,
               integ_rar, untouched_par, intact_2, intact_3, donor_rar, donor_par))


def scenario_repostrenamed(daemon, t):
    """M1 tier-2 pairing end-to-end: the donor is a byte-identical repost
    whose members were RENAMED (different release base name, same volume
    suffixes), so exact-name pairing cannot fire and the unique-suffix-key
    tier must pair the damaged member with its donor twin. No par2 members,
    and every hole gets filled, so the byte-based health recount now takes
    the item all the way to SUCCESS (moved to the final destination) instead
    of parking at FAILURE/HEALTH; byte integrity and the counters are the
    pass criteria."""
    seg_primary, seg_donor = 500_000, 300_000
    vol = 1_500_000
    members = [
        ('renA/x.part01.rar', 'Rel.part01.rar', vol, seg_primary, set()),
        ('renA/x.part02.rar', 'Rel.part02.rar', vol, seg_primary, {2}),
        ('renA/x.part03.rar', 'Rel.part03.rar', vol, seg_primary, set()),
    ]
    payloads = {}
    for i, m in enumerate(members):
        data = _payload(m[2], 8100 + i)
        payloads[m[1]] = data
        t.write_file(os.path.join('data', m[0]), data)

    donor_members = [(m[0].replace('renA', 'renB'), m[1].replace('Rel.', 'Other.'),
                      m[2], seg_donor, set()) for m in members]
    for dm, m in zip(donor_members, members):
        t.write_file(os.path.join('data', dm[0]), payloads[m[1]])

    primary = build_multi_nzb(members)
    donor = build_multi_nzb(donor_members)
    api = daemon.wait_ready()
    daemon.append(api, 'DonRenamed', donor, True, 'ren-key', 50)
    daemon.append(api, 'RelRenamed', primary, False, 'ren-key', 100)
    h = daemon.wait_history(api, 'RelRenamed')
    recov = int(h.get('DupeRecoveredArticles', 0))
    repaired = _grep_log(t, 'donor article(s)')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=both_dirs)
                for m in members)
    return ('repostrenamed', integ and recov >= 1 and repaired >= 1,
            'status=%s recovered=%d repair_logs=%d integrity=%s'
            % (h['Status'], recov, repaired, integ))


def scenario_repostobfuscated(daemon, t):
    """M1 sibling search end-to-end: a dupe of eight equal-size volumes whose
    names say nothing on either side (as when an indexer deobfuscated the
    release name but the posting kept random file names). The donor lists
    its members in a different order and cuts them into different article
    sizes, so no name, suffix, position or size-step tier can pair a damaged
    member with its twin. The twins of the damaged members sit beyond the
    first four donor members, which is all the old candidate cap probed:
    every damaged member must still be repaired byte-exactly."""
    seg_primary, seg_donor = 400_000, 300_000
    vol = 1_200_000
    names = ['Zk4q.bin', 'Yq8w.bin', 'Xa1e.bin', 'Wd3r.bin',
             'Vf7t.bin', 'Ug2y.bin', 'Th5u.bin', 'Sj6i.bin']
    damaged = {1, 4, 6, 7}
    members = [('obfA/f%d.bin' % i, names[i], vol, seg_primary,
                {2} if i in damaged else set()) for i in range(8)]
    payloads = []
    for i, m in enumerate(members):
        data = _payload(vol, 8300 + i)
        payloads.append(data)
        t.write_file(os.path.join('data', m[0]), data)

    donor_order = [0, 2, 3, 5, 1, 4, 6, 7]
    donor_members = []
    for pos, i in enumerate(donor_order):
        path = 'obfB/g%d.bin' % pos
        t.write_file(os.path.join('data', path), payloads[i])
        donor_members.append((path, 'd%02dx%d.bin' % (pos, 9 - pos), vol, seg_donor, set()))

    primary = build_multi_nzb(members)
    donor = build_multi_nzb(donor_members)
    api = daemon.wait_ready()
    daemon.append(api, 'DonObf', donor, True, 'obf-key', 50)
    daemon.append(api, 'RelObf', primary, False, 'obf-key', 100)
    h = daemon.wait_history(api, 'RelObf')
    recov = int(h.get('DupeRecoveredArticles', 0))
    repaired = _grep_log(t, 'donor article(s)')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[i], '.bin', dirs=both_dirs) for i in range(8))
    return ('repostobfuscated', integ and recov >= len(damaged) and repaired >= len(damaged),
            'status=%s recovered=%d repair_logs=%d integrity=%s'
            % (h['Status'], recov, repaired, integ))


def scenario_repostdonorgaps(daemon, t):
    """Probe replacement end-to-end: the donor is a byte-identical repost
    (different segmentation) that misses articles of its own - the four it
    misses are exactly the first two identity probes and their first two
    replacements. A probe the donor cannot supply is inconclusive, not a
    mismatch: verification must draw further probes until it can compare
    the twin, then repair the primary's hole. Nothing may be rejected."""
    size, seg_primary, seg_donor = 6_000_000, 500_000, 250_000
    data = _payload(size, 8500)
    pp = _place_copy(t, 'gapsA', data, 'file.mkv')
    dp = _place_copy(t, 'gapsB', data, 'file.mkv')
    # primary part 6 = donor parts 11-12; the clear probe candidates are the
    # remaining donor parts minus a one-part margin, and the spread picks
    # (then the spread replacements) are donor parts 6/20, then 5/19
    primary = build_nzb(pp, 'GapsA.mkv', size, seg_primary, {6})
    donor = build_nzb(dp, 'obf-gaps.mkv', size, seg_donor, {5, 6, 19, 20})
    api = daemon.wait_ready()
    daemon.append(api, 'DonGaps', donor, True, 'gaps-key', 50)
    daemon.append(api, 'RelGaps', primary, False, 'gaps-key', 100)
    h = daemon.wait_history(api, 'RelGaps')
    repaired = _grep_log(t, 'donor article(s)')
    rejected = _grep_log(t, 'content identity not confirmed')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    success = 'SUCCESS' in h['Status']
    return ('repostdonorgaps', success and integ and repaired >= 1 and rejected == 0,
            'status=%s repair_logs=%d rejected_logs=%d integrity=%s'
            % (h['Status'], repaired, rejected, integ))


def scenario_xpackbare(daemon, t):
    """M2 cross-packing smoke test: the primary posts movie.mkv BARE and
    completes with a hole; the only duplicate posts the SAME movie packed
    into store-mode RAR3 volumes (different framing, different offsets,
    different segmentation). M1 cannot pair bare against rar volumes -
    the ContentMap pass must locate the missing bytes inside the donor's
    volumes and patch them. No par2, and the hole is fully filled, so the
    byte-based health recount now completes the item as SUCCESS (moved to
    the final destination) instead of parking at FAILURE/HEALTH; integrity,
    the cross-packing logs and the counter are the pass criteria."""
    size, seg_primary, seg_donor = 6_000_000, 500_000, 300_000
    data = _payload(size, 5150)
    pp = _place_copy(t, 'xpbA', data, 'movie.mkv')
    primary = build_nzb(pp, 'movie.mkv', size, seg_primary, {5, 6})

    volumes = generators.rar3_store_volumes('movie.mkv', data, 2_000_000)
    donor_members = []
    for i, vol in enumerate(volumes, 1):
        rel = 'xpbB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        donor_members.append((rel, 'Rel.part%02d.rar' % i, len(vol), seg_donor, set()))
    donor = build_multi_nzb(donor_members)

    api = daemon.wait_ready()
    daemon.append(api, 'DonXpb', donor, True, 'xpb-key', 50)
    daemon.append(api, 'RelXpb', primary, False, 'xpb-key', 100)
    h = daemon.wait_history(api, 'RelXpb')
    recov = int(h.get('DupeRecoveredArticles', 0))
    xpack = _grep_log(t, 'Cross-packing repair of')
    repaired = _grep_log(t, 'cross-packing')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return ('xpackbare', integ and recov >= 1 and xpack >= 1 and repaired >= 1,
            'status=%s recovered=%d xpack_logs=%d repair_logs=%d integrity=%s'
            % (h['Status'], recov, xpack, repaired, integ))


def _xpack_run(daemon, t, tag, primary_members, donor_members, payloads,
               primary_password=None, donor_password=None):
    """Append donor (paused) + primary under one dupe-key, wait for history,
    return (history, per-payload integrity dict, log counters).
    ``primary_password``/``donor_password`` attach a <meta type="password">
    block to the respective NZB (see build_multi_nzb) for the M3
    password-assisted xcrypt scenarios; both default to no password, which is
    the M2 (unencrypted) behavior every other xpack* scenario relies on."""
    primary = build_multi_nzb(primary_members, password=primary_password)
    donor = build_multi_nzb(donor_members, password=donor_password)
    api = daemon.wait_ready()
    daemon.append(api, 'Don' + tag, donor, True, tag + '-key', 50)
    daemon.append(api, 'Rel' + tag, primary, False, tag + '-key', 100)
    h = daemon.wait_history(api, 'Rel' + tag)
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integrity = {name: _verify_output(t, data, os.path.splitext(name)[1], dirs=both_dirs)
                 for name, data in payloads.items()}
    counters = {
        'recov': int(h.get('DupeRecoveredArticles', 0)),
        'xpack': _grep_log(t, 'Cross-packing repair of'),
        'repaired': _grep_log(t, 'cross-packing'),
        'rejected': _grep_log(t, 'content identity not confirmed'),
        'missing': _grep_log(t, 'still missing after stream repair'),
    }
    return h, integrity, counters


def scenario_xpackrar(daemon, t):
    """Store-rar target repaired from a BARE donor, with degradation: vol2
    has a data hole (repairable through the map), vol3 lost its first part
    including the rar headers (that volume must be excluded and stay
    damaged for par2 - which doesn't exist here). This is the PARTIAL-repair
    proof for the health recount: RepairCompleted only credits a target's
    encoded failed size back to health when EVERY one of its holes is
    filled, so vol3's unrepaired header hole means the item still stays
    FAILURE/HEALTH - never a false SUCCESS - and that is asserted directly
    below."""
    size = 6_000_000
    data = _payload(size, 6100)
    volumes = generators.rar3_store_volumes('movie.mkv', data, 2_000_000)
    payloads, members = {}, []
    missing = [set(), {2}, {1}]        # vol2: data hole; vol3: header hole
    for i, (vol, miss) in enumerate(zip(volumes, missing), 1):
        rel = 'xprA/rel.part%02d.rar' % i
        name = 'Rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        payloads[name] = vol
        members.append((rel, name, len(vol), 500_000, miss))
    dp = _place_copy(t, 'xprB', data, 'movie.mkv')
    donor_members = [(dp, 'movie.mkv', size, 300_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xpr', members, donor_members, payloads)
    not_success = 'SUCCESS' not in h['Status']
    ok = (integ['Rel.part01.rar'] and integ['Rel.part02.rar'] and
          not integ['Rel.part03.rar'] and              # header-holed vol stays damaged
          not_success and                               # partial repair never parks as SUCCESS
          c['recov'] >= 1 and c['xpack'] >= 1 and c['repaired'] >= 1 and
          c['missing'] >= 1)
    return ('xpackrar', ok,
            'status=%s recovered=%d xpack=%d repaired=%d missing=%d integ=%s'
            % (h['Status'], c['recov'], c['xpack'], c['repaired'], c['missing'],
               {k: v for k, v in integ.items()}))


def scenario_xpackrar2rar(daemon, t):
    """rar-to-rar with DIFFERENT volume sizes (3x2MB target, 4x1.5MB donor):
    member-wise M1 cannot window these (sizes differ by 25%), the inner
    stream matches exactly."""
    size = 6_000_000
    data = _payload(size, 6200)
    volumes = generators.rar3_store_volumes('movie.mkv', data, 2_000_000)
    payloads, members = {}, []
    for i, vol in enumerate(volumes, 1):
        rel = 'xr2A/rel.part%02d.rar' % i
        name = 'Rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        payloads[name] = vol
        members.append((rel, name, len(vol), 500_000, {2} if i == 1 else set()))
    donor_members = []
    for i, vol in enumerate(generators.rar3_store_volumes('movie.mkv', data, 1_500_000), 1):
        rel = 'xr2B/other.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        donor_members.append((rel, 'Other.part%02d.rar' % i, len(vol), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'Xr2', members, donor_members, payloads)
    ok = all(integ.values()) and c['recov'] >= 1 and c['repaired'] >= 1
    return ('xpackrar2rar', ok, 'status=%s recovered=%d repaired=%d integ=%s'
            % (h['Status'], c['recov'], c['repaired'], all(integ.values())))


def scenario_xpack2sets(daemon, t):
    """TWO damaged store-rar sets in one item, repaired from ONE duplicate
    carrying both sets at different volume sizes. Locks R>1 correctness of
    per-donor-set map reuse: both sets must repair byte-identically and the
    per-pair identity gate (inner-size equality + probes) must still route
    each repair set to ITS donor set only."""
    size = 6_000_000
    data_one = _payload(size, 6300)
    data_two = _payload(size, 6301)
    payloads, members = {}, []
    for tag, data in (('one', data_one), ('two', data_two)):
        volumes = generators.rar3_store_volumes('movie_%s.mkv' % tag, data, 2_000_000)
        for i, vol in enumerate(volumes, 1):
            rel = 'x2sA/%s.part%02d.rar' % (tag, i)
            name = 'Rel%s.part%02d.rar' % (tag.capitalize(), i)
            t.write_file(os.path.join('data', rel), vol)
            payloads[name] = vol
            members.append((rel, name, len(vol), 500_000, {2} if i == 1 else set()))
    donor_members = []
    for tag, data in (('one', data_one), ('two', data_two)):
        volumes = generators.rar3_store_volumes('movie_%s.mkv' % tag, data, 1_500_000)
        for i, vol in enumerate(volumes, 1):
            rel = 'x2sB/o%s.part%02d.rar' % (tag, i)
            t.write_file(os.path.join('data', rel), vol)
            donor_members.append(
                (rel, 'O%s.part%02d.rar' % (tag.capitalize(), i), len(vol), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'X2s', members, donor_members, payloads)
    ok = all(integ.values()) and c['repaired'] >= 2 and c['recov'] >= 2
    return ('xpack2sets', ok, 'status=%s recovered=%d repaired=%d integ=%s'
            % (h['Status'], c['recov'], c['repaired'], all(integ.values())))


def scenario_xpackzip(daemon, t):
    """Bare target repaired from a SPANNED STORED ZIP donor (z01+z02+zip).
    No par2 and the hole is fully filled, so the byte-based health recount
    takes the item to SUCCESS (moved to the final destination) - asserted
    directly below as the cross-packing proof of the recount."""
    size = 6_000_000
    data = _payload(size, 6300)
    pp = _place_copy(t, 'xpzA', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    payloads = {'movie.mkv': data}
    zip_bytes = generators.zip_store([('movie.mkv', data)])
    pieces = generators.split_bytes(zip_bytes, [2_500_000, 2_500_000])
    suffixes = ['z01', 'z02', 'zip']
    donor_members = []
    for piece, suffix in zip(pieces, suffixes):
        rel = 'xpzB/rel.%s' % suffix
        t.write_file(os.path.join('data', rel), piece)
        donor_members.append((rel, 'Rel.%s' % suffix, len(piece), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'Xpz', members, donor_members, payloads)
    ok = (integ['movie.mkv'] and c['recov'] >= 1 and c['repaired'] >= 1 and
          'SUCCESS' in h['Status'])
    return ('xpackzip', ok, 'status=%s recovered=%d repaired=%d integrity=%s'
            % (h['Status'], c['recov'], c['repaired'], integ['movie.mkv']))


def scenario_xpack7z(daemon, t):
    """Bare target repaired from a 7z-COPY donor posted as .7z.001/.002."""
    size = 6_000_000
    data = _payload(size, 6400)
    pp = _place_copy(t, 'xp7A', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    payloads = {'movie.mkv': data}
    archive = generators.seven_zip_copy([('movie.mkv', data)])
    pieces = generators.split_bytes(archive, [3_000_100])
    donor_members = []
    for i, piece in enumerate(pieces, 1):
        rel = 'xp7B/rel.7z.%03d' % i
        t.write_file(os.path.join('data', rel), piece)
        donor_members.append((rel, 'Rel.7z.%03d' % i, len(piece), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'Xp7', members, donor_members, payloads)
    ok = integ['movie.mkv'] and c['recov'] >= 1 and c['repaired'] >= 1
    return ('xpack7z', ok, 'status=%s recovered=%d repaired=%d integrity=%s'
            % (h['Status'], c['recov'], c['repaired'], integ['movie.mkv']))


def scenario_xpacksplit(daemon, t):
    """Store-rar target repaired from RAW SPLITS (movie.mkv.001/.002/.003)."""
    size = 6_000_000
    data = _payload(size, 6500)
    volumes = generators.rar3_store_volumes('movie.mkv', data, 2_000_000)
    payloads, members = {}, []
    for i, vol in enumerate(volumes, 1):
        rel = 'xpsA/rel.part%02d.rar' % i
        name = 'Rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        payloads[name] = vol
        members.append((rel, name, len(vol), 500_000, {3} if i == 1 else set()))
    donor_members = []
    for i, piece in enumerate(generators.split_bytes(data, [2_000_000, 2_000_000]), 1):
        rel = 'xpsB/movie.mkv.%03d' % i
        t.write_file(os.path.join('data', rel), piece)
        donor_members.append((rel, 'movie.mkv.%03d' % i, len(piece), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'Xps', members, donor_members, payloads)
    ok = all(integ.values()) and c['recov'] >= 1 and c['repaired'] >= 1
    return ('xpacksplit', ok, 'status=%s recovered=%d repaired=%d integ=%s'
            % (h['Status'], c['recov'], c['repaired'], all(integ.values())))


def scenario_xpackcompressed(daemon, t):
    """The mechanism ladder on a COMPRESSED archive: M2 must never map it
    (method gate), but a byte-identical repost still repairs it via M1 -
    exactly the promise the docs make for compressed/encrypted content."""
    size = 4_000_000
    data = _payload(size, 6600)      # opaque stand-in for compressed bytes
    volumes = generators.rar3_store_volumes('movie.mkv', data, 2_000_000, method=0x33)
    payloads, members = {}, []
    for i, vol in enumerate(volumes, 1):
        rel = 'xpcA/rel.part%02d.rar' % i
        name = 'Rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        payloads[name] = vol
        members.append((rel, name, len(vol), 500_000, {2} if i == 1 else set()))
    donor_members = []
    for i, vol in enumerate(volumes, 1):
        rel = 'xpcB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        donor_members.append((rel, 'Rel.part%02d.rar' % i, len(vol), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'Xpc', members, donor_members, payloads)
    repaired_m1 = _grep_log(t, 'donor article(s)')
    ok = (all(integ.values()) and c['recov'] >= 1 and repaired_m1 >= 1 and
          c['xpack'] == 0)          # M1 filled the holes; M2 never needed
    return ('xpackcompressed', ok,
            'status=%s recovered=%d m1_repairs=%d xpack=%d integ=%s'
            % (h['Status'], c['recov'], repaired_m1, c['xpack'], all(integ.values())))


def scenario_xpackneg(daemon, t):
    """The negative: a donor set with the RIGHT inner size but the WRONG
    bytes (different payload packed into store-rar volumes). The inner
    probes must reject it; nothing may be written."""
    size = 6_000_000
    data = _payload(size, 6700)
    decoy = _payload(size, 6800)
    pp = _place_copy(t, 'xpnA', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    donor_members = []
    for i, vol in enumerate(generators.rar3_store_volumes('movie.mkv', decoy, 2_000_000), 1):
        rel = 'xpnB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        donor_members.append((rel, 'Rel.part%02d.rar' % i, len(vol), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'Xpn', members, donor_members,
                             {'movie.mkv': data, 'decoy': decoy})
    ok = (c['rejected'] >= 1 and c['recov'] == 0 and c['missing'] >= 1 and
          not integ['movie.mkv'] and not integ['decoy'])
    return ('xpackneg', ok,
            'status=%s recovered=%d rejected=%d missing=%d file_matches=%s/%s'
            % (h['Status'], c['recov'], c['rejected'], c['missing'],
               integ['movie.mkv'], integ['decoy']))


def scenario_xcrypt_encplain(daemon, t):
    """M3 password-assisted cross-packing: the TARGET is a password-ENCRYPTED
    store-rar release (one continuous AES-128-CBC stream across volumes -
    ContentMap.cpp's cipher-composite map - built with a deliberately
    non-16-aligned volume_size to exercise real WinRAR's arbitrary volume
    cuts, see rar3_store_volumes_encrypted). Its own password travels via its
    NZB <head><meta type="password">; the target's ContentMap is built with
    that password directly (no ladder needed on the target side - see
    ExecCrossPackRepair). A data hole sits in the MIDDLE volume (volume 1's
    and 3's headers+salt stay intact, only volume 2 loses a non-header
    segment). The donor is the SAME movie posted BARE and unencrypted -
    cross-packing decrypts the target's plaintext space with its own key,
    patches from the donor's plaintext, and re-encrypts back into the
    on-disk ciphertext. Byte-identical ENCRYPTED volumes afterward prove the
    whole decrypt/patch/re-encrypt round trip."""
    if not generators.HAVE_CRYPTO:
        return ('xcrypt_encplain', None, 'SKIP: cryptography not installed')
    size = 6_000_000
    data = _payload(size, 7100)
    password = 'target-pw-A'
    volumes = generators.rar3_store_volumes_encrypted('movie.mkv', data, 2_000_003, password)
    payloads, members = {}, []
    missing = [set(), {2}, set()]      # only volume 2 loses a non-header segment
    for i, (vol, miss) in enumerate(zip(volumes, missing), 1):
        rel = 'xceA/rel.part%02d.rar' % i
        name = 'Rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        payloads[name] = vol
        members.append((rel, name, len(vol), 500_000, miss))
    dp = _place_copy(t, 'xceB', data, 'movie.mkv')
    donor_members = [(dp, 'movie.mkv', size, 300_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xce', members, donor_members, payloads,
                             primary_password=password)
    ok = all(integ.values()) and c['recov'] >= 1 and c['xpack'] >= 1 and c['repaired'] >= 1
    return ('xcrypt_encplain', ok,
            'status=%s recovered=%d xpack=%d repaired=%d integ=%s'
            % (h['Status'], c['recov'], c['xpack'], c['repaired'], all(integ.values())))


def scenario_xcrypt_plainenc(daemon, t):
    """Reverse direction: a BARE unencrypted target with a hole is repaired
    from a password-ENCRYPTED store-rar donor. The donor's password travels
    via its own NZB <meta type="password">; ExecCrossPackRepair's M3 retry
    ladder tries the donor without a password first (fails: "encrypted
    archive data"), then retries with donor.Password and succeeds."""
    if not generators.HAVE_CRYPTO:
        return ('xcrypt_plainenc', None, 'SKIP: cryptography not installed')
    size = 6_000_000
    data = _payload(size, 7200)
    password = 'donor-pw-B'
    pp = _place_copy(t, 'xpeA', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    payloads = {'movie.mkv': data}
    donor_members = []
    for i, vol in enumerate(
            generators.rar3_store_volumes_encrypted('movie.mkv', data, 2_000_003, password), 1):
        rel = 'xpeB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        donor_members.append((rel, 'Rel.part%02d.rar' % i, len(vol), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'Xpe', members, donor_members, payloads,
                             donor_password=password)
    ok = integ['movie.mkv'] and c['recov'] >= 1 and c['repaired'] >= 1
    return ('xcrypt_plainenc', ok, 'status=%s recovered=%d repaired=%d integrity=%s'
            % (h['Status'], c['recov'], c['repaired'], integ['movie.mkv']))


def scenario_xcrypt_diffpass(daemon, t):
    """Both sides encrypted with DIFFERENT passwords and different volume
    sizes: the target (password A) has a data hole in its middle volume; the
    donor (password B) is a repost of the same movie under its own key. This
    proves the two crypto contexts never mix - the donor's plaintext is
    recovered with ITS key (VerifyDonorSetEncrypted/ReadDonorInner use the
    donor's own ContentMap+RunCrypto) and the patch is re-encrypted with the
    TARGET's key (PatchFromDonorSetEncrypted uses repairSet.Map's RunCrypto)."""
    if not generators.HAVE_CRYPTO:
        return ('xcrypt_diffpass', None, 'SKIP: cryptography not installed')
    size = 6_000_000
    data = _payload(size, 7300)
    pw_a, pw_b = 'password-A', 'password-B'
    volumes = generators.rar3_store_volumes_encrypted('movie.mkv', data, 2_000_003, pw_a)
    payloads, members = {}, []
    missing = [set(), {2}, set()]
    for i, (vol, miss) in enumerate(zip(volumes, missing), 1):
        rel = 'xdpA/rel.part%02d.rar' % i
        name = 'Rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        payloads[name] = vol
        members.append((rel, name, len(vol), 500_000, miss))
    donor_members = []
    for i, vol in enumerate(
            generators.rar3_store_volumes_encrypted('movie.mkv', data, 1_500_001, pw_b), 1):
        rel = 'xdpB/other.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        donor_members.append((rel, 'Other.part%02d.rar' % i, len(vol), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'Xdp', members, donor_members, payloads,
                             primary_password=pw_a, donor_password=pw_b)
    ok = all(integ.values()) and c['recov'] >= 1 and c['xpack'] >= 1 and c['repaired'] >= 1
    return ('xcrypt_diffpass', ok,
            'status=%s recovered=%d xpack=%d repaired=%d integ=%s'
            % (h['Status'], c['recov'], c['xpack'], c['repaired'], all(integ.values())))


def scenario_xcrypt_wrongpass(daemon, t):
    """NEGATIVE: a bare unencrypted target with a hole; the encrypted donor's
    NZB advertises a password that does NOT match the one it was actually
    encrypted with. RAR3 carries no stored password-check value (real WinRAR
    fact, mirrored in StreamCryptoRar3WrongPasswordTest) - so the M3 retry
    ladder's BuildMap call SUCCEEDS with a wrong key instead of failing
    closed there; the mismatch is only caught downstream, when the wrongly
    "decrypted" donor plaintext is content-identity-probed against the
    target's known bytes and rejected ("content identity not confirmed").
    Nothing may be written and the target must stay exactly as it arrived -
    the same fail-closed guarantee xpackneg proves for the plain case."""
    if not generators.HAVE_CRYPTO:
        return ('xcrypt_wrongpass', None, 'SKIP: cryptography not installed')
    size = 6_000_000
    data = _payload(size, 7400)
    real_password = 'correct-pw'
    wrong_password = 'incorrect-pw'
    pp = _place_copy(t, 'xwpA', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    payloads = {'movie.mkv': data}
    donor_members = []
    for i, vol in enumerate(
            generators.rar3_store_volumes_encrypted('movie.mkv', data, 2_000_003, real_password), 1):
        rel = 'xwpB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        donor_members.append((rel, 'Rel.part%02d.rar' % i, len(vol), 300_000, set()))

    h, integ, c = _xpack_run(daemon, t, 'Xwp', members, donor_members, payloads,
                             donor_password=wrong_password)
    ok = c['rejected'] >= 1 and c['recov'] == 0 and not integ['movie.mkv']
    return ('xcrypt_wrongpass', ok,
            'status=%s recovered=%d rejected=%d integrity=%s'
            % (h['Status'], c['recov'], c['rejected'], integ['movie.mkv']))


def scenario_xdecomp_zip(daemon, t):
    """M4 decompression donor: a bare movie.mkv target with holes, repaired
    from a REAL DEFLATE-compressed zip donor (generators.zip_deflated)
    holding the identical movie.mkv. M2 never maps this (BuildZipMap rejects
    any Method != 0 entry), so only the M4 ladder - materialize the donor's
    articles, shell out to the configured SevenZipCmd to extract movie.mkv,
    verify the extracted bytes against the target's own downloaded bytes,
    then patch the holes - can repair it. Requires DupeStreamDecompress=yes
    (see SCENARIO_OPTIONS) and a local 7z (generators.HAVE_7Z); SKIPs
    gracefully without one. No par2 and the hole is fully filled, so the
    byte-based health recount takes the item to SUCCESS - asserted directly
    below as the decompression proof of the recount."""
    if not generators.HAVE_7Z:
        return ('xdecomp_zip', None, 'SKIP: 7z not installed')
    size = 4_000_000
    data = _payload(size, 8200)
    pp = _place_copy(t, 'xdzA', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    payloads = {'movie.mkv': data}
    archive = generators.zip_deflated([('movie.mkv', data)])
    rel = 'xdzB/rel.zip'
    t.write_file(os.path.join('data', rel), archive)
    donor_members = [(rel, 'Rel.zip', len(archive), 300_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xdz', members, donor_members, payloads)
    decompressed = _grep_log(t, '(decompressed)')
    ok = (integ['movie.mkv'] and c['recov'] >= 1 and decompressed >= 1 and
          'SUCCESS' in h['Status'])
    return ('xdecomp_zip', ok,
            'status=%s recovered=%d decompressed_logs=%d integrity=%s'
            % (h['Status'], c['recov'], decompressed, integ['movie.mkv']))


def scenario_xdecomp_7z(daemon, t):
    """Same shape as xdecomp_zip, but the donor is a REAL LZMA2-compressed 7z
    archive (generators.seven_zip_lzma) - BuildSevenZipMap rejects its
    non-Copy coder, so only the M4 decompression ladder can repair the bare
    target."""
    if not generators.HAVE_7Z:
        return ('xdecomp_7z', None, 'SKIP: 7z not installed')
    size = 4_000_000
    data = _payload(size, 8300)
    pp = _place_copy(t, 'xd7A', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    payloads = {'movie.mkv': data}
    with tempfile.TemporaryDirectory(prefix='xd7z-') as workdir:
        archive = generators.seven_zip_lzma([('movie.mkv', data)], workdir)
    rel = 'xd7B/rel.7z'
    t.write_file(os.path.join('data', rel), archive)
    donor_members = [(rel, 'Rel.7z', len(archive), 300_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xd7', members, donor_members, payloads)
    decompressed = _grep_log(t, '(decompressed)')
    ok = integ['movie.mkv'] and c['recov'] >= 1 and decompressed >= 1
    return ('xdecomp_7z', ok,
            'status=%s recovered=%d decompressed_logs=%d integrity=%s'
            % (h['Status'], c['recov'], decompressed, integ['movie.mkv']))


def scenario_xdecomp_storetarget(daemon, t):
    """The M4 ladder against a non-bare TARGET: a store-mode rar3 target
    (generators.rar3_store_volumes, the same M2 target-side generator
    xpackrar uses - no external tool needed to CREATE it) with a data hole
    in its second volume, repaired from a REAL LZMA2-compressed 7z donor of
    the same inner movie.mkv. Proves the M4 extracted-donor path reuses the
    plain (M2) inner-space target map, not just the identity/bare map."""
    if not generators.HAVE_7Z:
        return ('xdecomp_storetarget', None, 'SKIP: 7z not installed')
    size = 4_000_000
    data = _payload(size, 8400)
    volumes = generators.rar3_store_volumes('movie.mkv', data, 2_000_000)
    payloads, members = {}, []
    for i, vol in enumerate(volumes, 1):
        rel = 'xdsA/rel.part%02d.rar' % i
        name = 'Rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        payloads[name] = vol
        members.append((rel, name, len(vol), 500_000, {2} if i == 2 else set()))
    with tempfile.TemporaryDirectory(prefix='xds-') as workdir:
        archive = generators.seven_zip_lzma([('movie.mkv', data)], workdir)
    rel = 'xdsB/rel.7z'
    t.write_file(os.path.join('data', rel), archive)
    donor_members = [(rel, 'Rel.7z', len(archive), 300_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xds', members, donor_members, payloads)
    decompressed = _grep_log(t, '(decompressed)')
    ok = all(integ.values()) and c['recov'] >= 1 and decompressed >= 1
    return ('xdecomp_storetarget', ok,
            'status=%s recovered=%d decompressed_logs=%d integ=%s'
            % (h['Status'], c['recov'], decompressed, all(integ.values())))


def scenario_xdecomp_enc7z(daemon, t):
    """The POSIX password-quoting proof: a bare target repaired from a
    HEADER-ENCRYPTED LZMA2 7z donor (generators.seven_zip_lzma_encrypted,
    ``-mhe=on -p<password>``), the donor's password traveling via its own
    NZB <meta type="password"> exactly like xcrypt_plainenc threads an
    encrypted-rar donor's password. Unpack::MakeExtractor/MakePassword must
    pass that password to 7z as a single raw argv element on POSIX (M4 Task
    2, commit 3c3e9130) - a quote-wrapped password would make 7z see the
    literal quote characters and fail extraction, and this scenario would
    then FAIL (not SKIP)."""
    if not generators.HAVE_7Z:
        return ('xdecomp_enc7z', None, 'SKIP: 7z not installed')
    size = 4_000_000
    data = _payload(size, 8500)
    password = 'decomp-pw-7z'
    pp = _place_copy(t, 'xdeA', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    payloads = {'movie.mkv': data}
    with tempfile.TemporaryDirectory(prefix='xde-') as workdir:
        archive = generators.seven_zip_lzma_encrypted([('movie.mkv', data)], workdir, password)
    rel = 'xdeB/rel.7z'
    t.write_file(os.path.join('data', rel), archive)
    donor_members = [(rel, 'Rel.7z', len(archive), 300_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xde', members, donor_members, payloads,
                             donor_password=password)
    decompressed = _grep_log(t, '(decompressed)')
    ok = integ['movie.mkv'] and c['recov'] >= 1 and decompressed >= 1
    return ('xdecomp_enc7z', ok,
            'status=%s recovered=%d decompressed_logs=%d integrity=%s'
            % (h['Status'], c['recov'], decompressed, integ['movie.mkv']))


def scenario_xdecomp_enctarget(daemon, t):
    """The M3+M4 composition: a password-ENCRYPTED store-rar TARGET (same
    generator as xcrypt_encplain) with a data hole in its middle volume,
    repaired from a COMPRESSED 7z donor of the same movie.mkv. The donor
    cannot map for byte-copy (it is compressed), so M4 materializes and
    extracts it to plaintext; that plaintext is then re-encrypted under the
    target's own AES-CBC stream context (the M3 write core) and the
    ciphertext written into the hole. Byte-identical ENCRYPTED volumes prove
    the extract -> re-encrypt -> patch round trip. Requires both a crypto
    module (encrypted target generator) and a 7z binary (compressed donor)."""
    if not generators.HAVE_CRYPTO:
        return ('xdecomp_enctarget', None, 'SKIP: cryptography not installed')
    if not generators.HAVE_7Z:
        return ('xdecomp_enctarget', None, 'SKIP: 7z not installed')
    size = 6_000_000
    data = _payload(size, 8600)
    password = 'target-pw-D'
    volumes = generators.rar3_store_volumes_encrypted('movie.mkv', data, 2_000_003, password)
    payloads, members = {}, []
    missing = [set(), {2}, set()]      # only volume 2 loses a non-header segment
    for i, (vol, miss) in enumerate(zip(volumes, missing), 1):
        rel = 'xdeA/rel.part%02d.rar' % i
        name = 'Rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        payloads[name] = vol
        members.append((rel, name, len(vol), 500_000, miss))
    with tempfile.TemporaryDirectory(prefix='xde-') as workdir:
        archive = generators.seven_zip_lzma([('movie.mkv', data)], workdir)
    rel = 'xdeB/rel.7z'
    t.write_file(os.path.join('data', rel), archive)
    donor_members = [(rel, 'Rel.7z', len(archive), 300_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xde', members, donor_members, payloads,
                             primary_password=password)
    decompressed = _grep_log(t, '(decompressed)')
    ok = all(integ.values()) and c['recov'] >= 1 and decompressed >= 1
    return ('xdecomp_enctarget', ok,
            'status=%s recovered=%d decompressed_logs=%d integ=%s'
            % (h['Status'], c['recov'], decompressed, all(integ.values())))


def scenario_xdecomp_neg(daemon, t):
    """The negative: a bare target repaired attempt from a REAL 7z donor
    holding the RIGHT-size but WRONG-bytes inner file (a decoy payload of
    the same size). VerifyDonorInnerFile's identity probe must reject it
    (`content identity not confirmed`) before any write; nothing may be
    written and neither file may end up matching the other."""
    if not generators.HAVE_7Z:
        return ('xdecomp_neg', None, 'SKIP: 7z not installed')
    size = 4_000_000
    data = _payload(size, 8600)
    decoy = _payload(size, 8700)
    pp = _place_copy(t, 'xdnA', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    with tempfile.TemporaryDirectory(prefix='xdn-') as workdir:
        archive = generators.seven_zip_lzma([('movie.mkv', decoy)], workdir)
    rel = 'xdnB/rel.7z'
    t.write_file(os.path.join('data', rel), archive)
    donor_members = [(rel, 'Rel.7z', len(archive), 300_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xdn', members, donor_members,
                             {'movie.mkv': data, 'decoy': decoy})
    decompressed = _grep_log(t, '(decompressed)')
    ok = (c['rejected'] >= 1 and c['recov'] == 0 and decompressed == 0 and
          not integ['movie.mkv'] and not integ['decoy'])
    return ('xdecomp_neg', ok,
            'status=%s recovered=%d rejected=%d decompressed_logs=%d file_matches=%s/%s'
            % (h['Status'], c['recov'], c['rejected'], decompressed,
               integ['movie.mkv'], integ['decoy']))


def scenario_xdecomp_symlink(daemon, t):
    """A compressed donor with a valid movie plus a symlink must be rejected
    before selecting or patching the movie (`archive contains link`). The
    link is RELATIVE and in-tree on purpose: 7-Zip 23.01+ refuses to create
    absolute or ``..`` link targets at extraction time (exit code 2, link
    skipped), which fails the extract step before the daemon's own link
    check ever runs - only a link every extractor generation materializes
    can probe that check. A relative link to the valid movie.mkv is also the
    attack shape the check guards against: followed naively it would
    "verify" trivially. Cleanup must unlink the extracted symlink, preserve
    the outside sentinel, and remove every stream-decompression scratch
    directory."""
    if not generators.HAVE_7Z:
        return ('xdecomp_symlink', None, 'SKIP: 7z not installed')
    if t.name != 'local':
        return ('xdecomp_symlink', None, 'SKIP: POSIX local target required')

    size = 1_000_000
    data = _payload(size, 8750)
    sentinel_data = b'outside-sentinel-must-survive'
    t.write_file(os.path.join('outside', 'sentinel'), sentinel_data)

    primary_path = _place_copy(t, 'xdlA', data, 'movie.mkv')
    members = [(primary_path, 'movie.mkv', size, 250_000, {2})]
    archive = generators.zip_deflated_with_symlink(
        [('movie.mkv', data)], 'movie-link.mkv', 'movie.mkv')
    rel = 'xdlB/rel.zip'
    t.write_file(os.path.join('data', rel), archive)
    donor_members = [(rel, 'Rel.zip', len(archive), 200_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xdl', members, donor_members,
                             {'movie.mkv': data})
    rejected_links = _grep_log(t, 'archive contains link')
    # Newer 7-Zip versions reject the dangerous absolute link before NZBGet
    # can inspect the extracted tree. Require that specific diagnostic AND
    # the failed-extraction path; a generic extraction error is insufficient.
    extractor_rejected = (
        _grep_log(t, 'ERROR: Dangerous link path was ignored') >= 1 and
        _grep_log(t, 'Skipping decompression of Rel.zip of duplicate DonXdl: '
                     'extraction failed') >= 1)
    try:
        sentinel_survived = t.read_file(os.path.join('outside', 'sentinel')) == sentinel_data
    except Exception:
        sentinel_survived = False

    scratch_dirs = []
    for root, dirs, _ in os.walk(t.path('main')):
        scratch_dirs.extend(os.path.join(root, name) for name in dirs
                            if name.startswith('.stream-decompress.'))

    ok = (h['Status'] == 'FAILURE/HEALTH' and c['recov'] == 0 and
          (rejected_links >= 1 or extractor_rejected) and not integ['movie.mkv'] and
          sentinel_survived and not scratch_dirs)
    return ('xdecomp_symlink', ok,
            'status=%s recovered=%d rejected_links=%d extractor_rejected=%s integrity=%s '
            'sentinel=%s scratch_dirs=%d'
            % (h['Status'], c['recov'], rejected_links, extractor_rejected, integ['movie.mkv'],
               sentinel_survived, len(scratch_dirs)))


def scenario_xdecomp_off(daemon, t):
    """The opt-in gate proof: the SAME bare-target-vs-compressed-7z-donor
    setup as xdecomp_7z, but DupeStreamDecompress is OMITTED (default no -
    see SCENARIO_OPTIONS, which deliberately does NOT add it here). The
    decompression path must never run (no `(decompressed)` log) and the
    item must stay unrepaired - M2 already rejected this donor's non-Copy
    coder, and M4 is off, so nothing else can map it."""
    if not generators.HAVE_7Z:
        return ('xdecomp_off', None, 'SKIP: 7z not installed')
    size = 4_000_000
    data = _payload(size, 8800)
    pp = _place_copy(t, 'xdoA', data, 'movie.mkv')
    members = [(pp, 'movie.mkv', size, 500_000, {5, 6})]
    payloads = {'movie.mkv': data}
    with tempfile.TemporaryDirectory(prefix='xdo-') as workdir:
        archive = generators.seven_zip_lzma([('movie.mkv', data)], workdir)
    rel = 'xdoB/rel.7z'
    t.write_file(os.path.join('data', rel), archive)
    donor_members = [(rel, 'Rel.7z', len(archive), 300_000, set())]

    h, integ, c = _xpack_run(daemon, t, 'Xdo', members, donor_members, payloads)
    decompressed = _grep_log(t, '(decompressed)')
    ok = decompressed == 0 and c['recov'] == 0 and not integ['movie.mkv']
    return ('xdecomp_off', ok,
            'status=%s recovered=%d decompressed_logs=%d integrity=%s'
            % (h['Status'], c['recov'], decompressed, integ['movie.mkv']))


def scenario_wholefile(daemon, t):
    """A volume none of whose articles is available anywhere: the primary
    lists it, every server lacks it. The byte-identical renamed repost is
    proven on the set's other damaged volume first (its bytes verify), and
    then the missing volume is recreated whole from the twin member the
    suffix pairs it with. Every hole filled and no par2, so the release
    completes SUCCESS byte-identically."""
    seg_primary, seg_donor = 500_000, 300_000
    vol = 1_500_000
    n = (vol + seg_primary - 1) // seg_primary
    members = [
        ('wholeA/x.part01.rar', 'Rel.part01.rar', vol, seg_primary, set()),
        ('wholeA/x.part02.rar', 'Rel.part02.rar', vol, seg_primary, {2}),
        ('wholeA/x.part03.rar', 'Rel.part03.rar', vol, seg_primary, set(range(1, n + 1))),
        ('wholeA/x.part04.rar', 'Rel.part04.rar', vol, seg_primary, set()),
    ]
    payloads = {}
    for i, m in enumerate(members):
        data = _payload(m[2], 9300 + i)
        payloads[m[1]] = data
        t.write_file(os.path.join('data', m[0]), data)
    donor_members = [(m[0].replace('wholeA', 'wholeB'), m[1].replace('Rel.', 'Other.'),
                      m[2], seg_donor, set()) for m in members]
    for dm, m in zip(donor_members, members):
        t.write_file(os.path.join('data', dm[0]), payloads[m[1]])
    api = daemon.wait_ready()
    daemon.append(api, 'DonWhole', build_multi_nzb(donor_members), True, 'whole-key', 50)
    daemon.append(api, 'RelWhole', build_multi_nzb(members), False, 'whole-key', 100)
    h = daemon.wait_history(api, 'RelWhole')
    queued = _grep_log(t, 'no article available')
    recreated = _grep_log(t, 'Recreating Rel.part03.rar')
    repaired = _grep_log(t, 'donor article(s)')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=both_dirs) for m in members)
    return ('wholefile', integ and queued == 1 and recreated == 1 and repaired >= 2,
            'status=%s queued_logs=%d recreated_logs=%d repair_logs=%d integrity=%s'
            % (h['Status'], queued, recreated, repaired, integ))


def _wholefile_fixture(t, tag):
    seg_primary, seg_donor = 500_000, 300_000
    vol = 1_500_000
    n = (vol + seg_primary - 1) // seg_primary
    members = [
        ('%sA/x.part01.rar' % tag, 'Rel.part01.rar', vol, seg_primary, set()),
        ('%sA/x.part02.rar' % tag, 'Rel.part02.rar', vol, seg_primary, {2}),
        ('%sA/x.part03.rar' % tag, 'Rel.part03.rar', vol, seg_primary, set(range(1, n + 1))),
        ('%sA/x.part04.rar' % tag, 'Rel.part04.rar', vol, seg_primary, set()),
    ]
    payloads = {}
    for i, m in enumerate(members):
        data = _payload(m[2], 9300 + i)
        payloads[m[1]] = data
        t.write_file(os.path.join('data', m[0]), data)
    donor_members = [(m[0].replace('%sA' % tag, '%sB' % tag), m[1].replace('Rel.', 'Other.'),
                      m[2], seg_donor, set()) for m in members]
    for dm, m in zip(donor_members, members):
        t.write_file(os.path.join('data', dm[0]), payloads[m[1]])
    return members, donor_members, payloads


def scenario_wholefileretry(daemon, t):
    """Corner case: "Retry failed articles" on a release whose volume was
    recreated whole from a duplicate. The recreated volume (no own article
    ever arrived) must survive the retry - it must not be deleted as an
    empty failed file and re-downloaded from the dead primary - and the
    release must again end byte-identical."""
    members, donor_members, payloads = _wholefile_fixture(t, 'wr')
    api = daemon.wait_ready()
    daemon.append(api, 'DonWR', build_multi_nzb(donor_members), True, 'wr-key', 50)
    daemon.append(api, 'RelWR', build_multi_nzb(members), False, 'wr-key', 100)
    h = daemon.wait_history(api, 'RelWR')
    nzbid = h['NZBID']
    first_integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=(('main', 'dst'), ('main', 'inter')))
                      for m in members)
    api.editqueue('HistoryRetryFailed', 0, '', [nzbid])
    deadline = time.time() + 180
    time.sleep(3)
    while time.time() < deadline:
        if not any(g['NZBID'] == nzbid for g in api.listgroups()):
            break
        time.sleep(1)
    h2 = [x for x in api.history() if x['NZBID'] == nzbid][0]
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=(('main', 'dst'), ('main', 'inter')))
                for m in members)
    return ('wholefileretry', first_integ and integ,
            'status=%s retry_status=%s first_integrity=%s integrity_after_retry=%s'
            % (h['Status'], h2['Status'], first_integ, integ))


def scenario_wholefilepar(daemon, t):
    """Fix 2 with a real par2 index (no recovery slices, so par2 cannot
    cover a missing volume): the whole-file job counts as damage par2 can't
    cover, stream repair runs before par-check, recreates the volume, and
    the full par-check that follows verifies all four volumes against the
    par2 checksums - SUCCESS/PAR only if the recreated volume is exact."""
    members, donor_members, payloads = _wholefile_fixture(t, 'wp')
    par = generators.par2_index([(m[1], payloads[m[1]]) for m in members])
    t.write_file(os.path.join('data', 'wpA/rel.par2'), par)
    members = members + [('wpA/rel.par2', 'Rel.par2', len(par), 500_000, set())]
    api = daemon.wait_ready()
    daemon.append(api, 'DonWP', build_multi_nzb(donor_members), True, 'wp-key', 50)
    daemon.append(api, 'RelWP', build_multi_nzb(members), False, 'wp-key', 100)
    h = daemon.wait_history(api, 'RelWP')
    recreated = _grep_log(t, 'Recreating Rel.part03.rar')
    par_ok = _grep_log(t, 'repair not needed') + _grep_log(t, 'Repair not needed') + _grep_log(t, 'all files are correct')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=both_dirs) for m in members[:4])
    return ('wholefilepar', integ and recreated == 1 and h['ParStatus'] == 'SUCCESS',
            'status=%s par=%s recreated_logs=%d par_ok_logs=%d integrity=%s'
            % (h['Status'], h['ParStatus'], recreated, par_ok, integ))


def scenario_wholefilefailretry(daemon, t):
    """Corner case: a release that still FAILS after a volume was recreated
    (another volume is missing on the donor too), so the recreated volume
    stays in the intermediate directory. "Retry failed articles" must not
    delete the recreated volume as an empty failed file."""
    members, donor_members, payloads = _wholefile_fixture(t, 'wf')
    # part04 is missing everywhere, on the primary and the duplicate
    members[3] = members[3][:4] + ({2},)
    donor_members[3] = donor_members[3][:4] + (set(range(2, 4)),)
    api = daemon.wait_ready()
    daemon.append(api, 'DonWF', build_multi_nzb(donor_members), True, 'wf-key', 50)
    daemon.append(api, 'RelWF', build_multi_nzb(members), False, 'wf-key', 100)
    h = daemon.wait_history(api, 'RelWF')
    nzbid = h['NZBID']
    inter = (('main', 'inter'),)
    before = _verify_output(t, payloads['Rel.part03.rar'], '.rar', dirs=inter)
    api.editqueue('HistoryRetryFailed', 0, '', [nzbid])
    time.sleep(3)
    deadline = time.time() + 180
    while time.time() < deadline and any(g['NZBID'] == nzbid for g in api.listgroups()):
        time.sleep(1)
    h2 = [x for x in api.history() if x['NZBID'] == nzbid][0]
    after = _verify_output(t, payloads['Rel.part03.rar'], '.rar', dirs=inter)
    return ('wholefilefailretry', h['Status'].startswith('FAILURE') and before and after,
            'status=%s retry_status=%s part03_before=%s part03_after_retry=%s'
            % (h['Status'], h2['Status'], before, after))


def scenario_wholefileonly(daemon, t):
    """Fix 2 in the most common shape: one volume missing entirely, every
    other volume complete - no damaged file exists to prove the duplicate
    on. The duplicate must be proven byte-identical on an intact volume and
    the missing one recreated; SUCCESS byte-identically."""
    members, donor_members, payloads = _wholefile_fixture(t, 'wo')
    members[1] = members[1][:4] + (set(),)               # part02 complete too
    api = daemon.wait_ready()
    daemon.append(api, 'DonWO', build_multi_nzb(donor_members), True, 'wo-key', 50)
    daemon.append(api, 'RelWO', build_multi_nzb(members), False, 'wo-key', 100)
    h = daemon.wait_history(api, 'RelWO')
    proven = _grep_log(t, 'verified on intact file')
    recreated = _grep_log(t, 'Recreating Rel.part03.rar')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=both_dirs) for m in members)
    return ('wholefileonly', integ and proven == 1 and recreated == 1,
            'status=%s proven_logs=%d recreated_logs=%d integrity=%s' % (h['Status'], proven, recreated, integ))


def scenario_wholefilewrongdonor(daemon, t):
    """The safety net of that proof: a duplicate with the same volume names
    and sizes but DIFFERENT bytes (another packing) must fail the intact-file
    proof, so the missing volume is not recreated from it and nothing is
    written; the release stays a failure."""
    members, donor_members, payloads = _wholefile_fixture(t, 'ww')
    members[1] = members[1][:4] + (set(),)
    for i, dm in enumerate(donor_members):                 # same sizes, other bytes
        t.write_file(os.path.join('data', dm[0]), _payload(dm[2], 9700 + i))
    api = daemon.wait_ready()
    daemon.append(api, 'DonWW', build_multi_nzb(donor_members), True, 'ww-key', 50)
    daemon.append(api, 'RelWW', build_multi_nzb(members), False, 'ww-key', 100)
    h = daemon.wait_history(api, 'RelWW')
    proven = _grep_log(t, 'verified on intact file')
    recreated = _grep_log(t, 'Recreating')
    return ('wholefilewrongdonor', proven == 0 and recreated == 0 and h['Status'].startswith('FAILURE'),
            'status=%s proven_logs=%d recreated_logs=%d' % (h['Status'], proven, recreated))


def _wholefile_run(daemon, t, tag, members, donor_members):
    api = daemon.wait_ready()
    daemon.append(api, 'Don' + tag, build_multi_nzb(donor_members), True, tag + '-key', 50)
    daemon.append(api, 'Rel' + tag, build_multi_nzb(members), False, tag + '-key', 100)
    return api, daemon.wait_history(api, 'Rel' + tag)


def scenario_wholefilenofirst(daemon, t):
    """Corner case: the donor's twin of the missing volume lacks its FIRST
    article, which is where the decoded file size is normally learned. Every
    yEnc article declares the file size, so a later article must size the
    file and the rest of the volume is still recreated (the first part stays
    a hole, so the release fails without par2 - but the bytes it got must be
    there and correct)."""
    members, donor_members, payloads = _wholefile_fixture(t, 'nf1')
    members[1] = members[1][:4] + (set(),)
    donor_members[2] = donor_members[2][:4] + ({1},)
    api, h = _wholefile_run(daemon, t, 'NF1', members, donor_members)
    recreated = _grep_log(t, 'Recreating Rel.part03.rar')
    data = None
    for base in (('main', 'dst'), ('main', 'inter')):
        for rel in t.find_files(*base):
            if rel.endswith('Rel.part03.rar'):
                data = t.read_file(rel)
    expected = payloads['Rel.part03.rar']
    tail_ok = bool(data) and len(data) == len(expected) and data[300_000:] == expected[300_000:]
    return ('wholefilenofirst', recreated == 1 and tail_ok,
            'status=%s recreated_logs=%d recreated_tail_matches=%s' % (h['Status'], recreated, tail_ok))


def scenario_wholefilepartial(daemon, t):
    """Corner case: the donor's twin has a hole of its own, so the missing
    volume is recreated only partly. Nothing may be credited to health for
    it (the release stays a failure), the recreated bytes must be correct,
    and a retry must keep them."""
    members, donor_members, payloads = _wholefile_fixture(t, 'wpt')
    members[1] = members[1][:4] + (set(),)
    donor_members[2] = donor_members[2][:4] + ({3},)
    api, h = _wholefile_run(daemon, t, 'WPT', members, donor_members)
    nzbid = h['NZBID']
    recreated = _grep_log(t, 'Recreating Rel.part03.rar')

    def part03():
        for base in (('main', 'dst'), ('main', 'inter')):
            for rel in t.find_files(*base):
                if rel.endswith('Rel.part03.rar'):
                    return t.read_file(rel)
        return None
    expected = payloads['Rel.part03.rar']
    first = part03()
    head_ok = bool(first) and first[:600_000] == expected[:600_000]
    api.editqueue('HistoryRetryFailed', 0, '', [nzbid])
    time.sleep(3)
    deadline = time.time() + 180
    while time.time() < deadline and any(g['NZBID'] == nzbid for g in api.listgroups()):
        time.sleep(1)
    h2 = [x for x in api.history() if x['NZBID'] == nzbid][0]
    second = part03()
    kept = bool(second) and second[:600_000] == expected[:600_000]
    return ('wholefilepartial', recreated == 1 and head_ok and kept and h['Status'].startswith('FAILURE')
            and h2['Status'].startswith('FAILURE'),
            'status=%s retry_status=%s recreated_logs=%d head_ok=%s kept_after_retry=%s'
            % (h['Status'], h2['Status'], recreated, head_ok, kept))


def scenario_wholefiletwo(daemon, t):
    """Corner case: two volumes missing entirely, both carried by the
    duplicate. Each must be recreated from its own twin member (never both
    from one), and the release completes SUCCESS byte-identically."""
    members, donor_members, payloads = _wholefile_fixture(t, 'w2')
    n = 3
    members[3] = members[3][:4] + (set(range(1, n + 1)),)
    api, h = _wholefile_run(daemon, t, 'W2', members, donor_members)
    recreated = _grep_log(t, 'Recreating Rel.part0')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=both_dirs) for m in members)
    return ('wholefiletwo', integ and recreated == 2,
            'status=%s recreated_logs=%d integrity=%s' % (h['Status'], recreated, integ))


def scenario_failoverlive(daemon, t):
    """Corner case: HealthCheck=dupe parks a download while a live stream
    repair pass (DupeArticleFallback=live) still works on one of its files.
    The pass must be detached cleanly - no crash, no write into the parked
    item - and the backup must be fetched and complete."""
    seg = 100_000
    holed = _payload(4_000_000, 9950)
    dead_size = 2_900_000
    n_dead = dead_size // seg
    t.write_file(os.path.join('data', 'flA/a.bin'), holed)
    members = [('flA/a.bin', 'A.bin', len(holed), seg, set(range(2, 40)))]
    for i in range(6):
        t.write_file(os.path.join('data', 'flA/d%d.bin' % i), _payload(dead_size, 9960 + i))
        members.append(('flA/d%d.bin' % i, 'Dead%d.bin' % i, dead_size, seg, set(range(1, n_dead + 1))))
    # the backup carries A byte-identically (other segmentation) plus its own content
    t.write_file(os.path.join('data', 'flB/a.bin'), holed)
    data = _payload(3_000_000, 9970)
    t.write_file(os.path.join('data', 'flB/b.bin'), data)
    backup = build_multi_nzb([('flB/a.bin', 'A.bin', len(holed), 70_000, set()),
                              ('flB/b.bin', 'Backup.bin', len(data), seg, set())])
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(members), True, 'fl-key', 100)
    daemon.append(api, 'Backup', backup, False, 'fl-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary', timeout=300)
    deadline = time.time() + 240
    hb = daemon.wait_history(api, 'Backup')
    while hb['Status'].startswith('DELETED') and time.time() < deadline:
        time.sleep(1)
        hb = daemon.wait_history(api, 'Backup')
    alive = True
    try:
        api.status()
    except Exception:
        alive = False
    live = _grep_log(t, 'Starting live stream repair')
    failed_over = _grep_log(t, 'Failing over Primary')
    return ('failoverlive', alive and failed_over == 1 and hb['Status'].startswith('SUCCESS'),
            'status=%s backup_status=%s live_logs=%d failover_logs=%d daemon_alive=%s'
            % (hp['Status'], hb['Status'], live, failed_over, alive))


def scenario_wholefilerestart(daemon, t):
    """Corner case: nzbget restarts between download and post-processing
    while a whole-file job (no decoded size, no holes yet) waits in the
    queue state. The job must be saved and loaded intact, so the restarted
    daemon still proves the duplicate and recreates the volume."""
    members, donor_members, payloads = _wholefile_fixture(t, 'rs')
    members[1] = members[1][:4] + (set(),)
    api = daemon.wait_ready()
    api.pausepost()
    daemon.append(api, 'DonRS', build_multi_nzb(donor_members), True, 'rs-key', 50)
    daemon.append(api, 'RelRS', build_multi_nzb(members), False, 'rs-key', 100)
    deadline = time.time() + 120
    while time.time() < deadline and _grep_log(t, 'Collection RelRS completely downloaded') == 0:
        time.sleep(0.5)
    queued_job = _grep_log(t, 'no article available')
    try:
        api.shutdown()
    except Exception:
        pass
    time.sleep(4)
    daemon.start()
    time.sleep(3)
    api = daemon.wait_ready()
    api.resumepost()
    h = daemon.wait_history(api, 'RelRS')
    recreated = _grep_log(t, 'Recreating Rel.part03.rar')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=both_dirs) for m in members)
    return ('wholefilerestart', queued_job == 1 and recreated == 1 and integ,
            'status=%s queued_logs=%d recreated_logs=%d integrity=%s'
            % (h['Status'], queued_job, recreated, integ))


def scenario_reloadpostqueue(daemon, t):
    """nzbget reloads (saving settings in the web UI does) while a download
    waits in post-processing, three times: the reloaded post job must run.
    Before, the queue counted as loaded from the start of the reload, so the
    post-processor could sanitise the queue before it was loaded again: the
    reloaded job kept its persisted stage, was never counted as queued, and
    post-processing never started (it showed LOADING_PARS for good). Stream
    repair makes post-processing run for minutes, so a reload meets it more
    often."""
    size, seg = 3_000_000, 500_000
    data = _payload(size, 4260)
    pp = _place_copy(t, 'rpA', data, 'movie.mkv')
    api = daemon.wait_ready()
    api.pausepost()
    # a queue that takes a moment to load: the post-processor starts first
    for i in range(40):
        filler = build_multi_nzb([('rpA/movie.mkv', 'Fill%02d-%02d.bin' % (i, j), size, seg, set())
                                  for j in range(50)])
        daemon.append(api, 'Filler%02d' % i, filler, True, 'rp-fill-%d' % i, 0)
    daemon.append(api, 'RelRP', build_nzb(pp, 'movie.mkv', size, seg, set()), False, 'rp-key', 100)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, 'Collection RelRP completely downloaded') == 0:
        time.sleep(0.2)
    for _ in range(3):
        api.reload()
        time.sleep(1)
        api = daemon.wait_ready(timeout=60)
    api.resumepost()
    try:
        h = daemon.wait_history(api, 'RelRP', timeout=60)
        status = h['Status']
    except RuntimeError:
        status = 'STUCK:%s' % [g['Status'] for g in api.listgroups()]
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return ('reloadpostqueue', status.startswith('SUCCESS') and integ,
            'status=%s integrity=%s' % (status, integ))


PROD_OPTIONS = ['DupeArticleFallback=live', 'DupeStreamDecompress=yes', 'ContinuePartial=yes',
                'ArticleCache=8192', 'FileNaming=auto', 'ReorderFiles=yes', 'PostStrategy=rocket',
                'ParCheck=auto', 'ParRepair=yes', 'ParScan=dupe', 'ParQuick=yes', 'ParRename=yes',
                'RarRename=yes', 'DirectRename=yes', 'HealthCheck=dupe', 'Unpack=yes', 'DirectUnpack=yes',
                'UnrarCmd=/usr/bin/unrar', 'SevenZipCmd=/usr/bin/7z', 'ArticleRetries=0']


def _prod_7z_set(t, tag, movie, volume_kb=1500):
    """A store-mode 7z split of movie.mkv: [(name suffix, bytes)]."""
    import shutil as _sh
    import tempfile as _tf
    work = _tf.mkdtemp(prefix='prod7z-')
    with open(os.path.join(work, 'movie.mkv'), 'wb') as f:
        f.write(movie)
    subprocess.run(['/usr/bin/7z', 'a', '-mx0', '-v%dk' % volume_kb, 'out.7z', 'movie.mkv'], cwd=work,
                   check=True, capture_output=True)
    vols = sorted(n for n in os.listdir(work) if n.startswith('out.7z.'))
    out = [(n[len('out'):], open(os.path.join(work, n), 'rb').read()) for n in vols]
    _sh.rmtree(work)
    return out


def _prod_wholefile(daemon, t, tag, damage):
    """Production options; a 4-volume store 7z of movie.mkv with a par2 index;
    the byte-identical repost under other names in history. damage:
    {volume index: missing parts}. Returns (history, extracted_ok)."""
    seg = 300_000
    movie = _payload(5_500_000, 9990)
    vols = _prod_7z_set(t, tag, movie)
    members, donor_members = [], []
    for i, (suffix, data) in enumerate(vols):
        t.write_file(os.path.join('data', '%sA/v%d' % (tag, i)), data)
        t.write_file(os.path.join('data', '%sB/v%d' % (tag, i)), data)
        members.append(('%sA/v%d' % (tag, i), 'Rel' + suffix, len(data), seg, damage.get(i, set())))
        donor_members.append(('%sB/v%d' % (tag, i), 'Other' + suffix, len(data), 200_000, set()))
    par = generators.par2_index([('Rel' + s, d) for s, d in vols])
    t.write_file(os.path.join('data', '%sA/rel.par2' % tag), par)
    members.append(('%sA/rel.par2' % tag, 'Rel.par2', len(par), seg, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'Don' + tag, build_multi_nzb(donor_members), True, tag + '-key', 50)
    daemon.append(api, 'Rel' + tag, build_multi_nzb(members), False, tag + '-key', 100)
    h = daemon.wait_history(api, 'Rel' + tag, timeout=300)
    ok = _verify_output(t, movie, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return h, ok


def scenario_prodwholefile(daemon, t):
    """Fix 2 under the production option set (live, direct rename/unpack,
    par-rename, quick par-check, unpack, article cache, HealthCheck=dupe):
    one 7z volume missing entirely, the rest intact. The volume is recreated
    from the proven repost, the par-check passes, and unpack extracts a
    byte-identical movie.mkv."""
    h, ok = _prod_wholefile(daemon, t, 'PW', {2: set(range(1, 7))})
    recreated = _grep_log(t, 'Recreating')
    return ('prodwholefile', ok and recreated == 1 and h['Status'].startswith('SUCCESS'),
            'status=%s par=%s unpack=%s recreated_logs=%d extracted_ok=%s'
            % (h['Status'], h['ParStatus'], h['UnpackStatus'], recreated, ok))


def scenario_prodstream(daemon, t):
    """Stream repair under the production option set: two volumes with holes
    (one at its start, one in the middle), the byte-identical repost in
    history; the par-check passes and unpack extracts a byte-identical
    movie.mkv."""
    h, ok = _prod_wholefile(daemon, t, 'PS', {0: {1}, 2: {3, 4}})
    return ('prodstream', ok and h['Status'].startswith('SUCCESS'),
            'status=%s par=%s unpack=%s extracted_ok=%s' % (h['Status'], h['ParStatus'], h['UnpackStatus'], ok))


def _prod_rar(daemon, t, tag, damage):
    """Production options; a 4-volume store-mode rar set (real CRCs, so unrar
    and direct unpack accept it) of movie.mkv with a par2 index; the
    byte-identical repost under other names in history."""
    seg = 300_000
    movie = _payload(5_000_000, 9995)
    vols = generators.rar3_store_volumes_valid('movie.mkv', movie, 1_400_000)
    members, donor_members = [], []
    for i, data in enumerate(vols):
        t.write_file(os.path.join('data', '%sA/v%d' % (tag, i)), data)
        t.write_file(os.path.join('data', '%sB/v%d' % (tag, i)), data)
        members.append(('%sA/v%d' % (tag, i), 'Rel.part%02d.rar' % (i + 1), len(data), seg, damage.get(i, set())))
        donor_members.append(('%sB/v%d' % (tag, i), 'Other.part%02d.rar' % (i + 1), len(data), 200_000, set()))
    par = generators.par2_index([('Rel.part%02d.rar' % (i + 1), d) for i, d in enumerate(vols)])
    t.write_file(os.path.join('data', '%sA/rel.par2' % tag), par)
    members.append(('%sA/rel.par2' % tag, 'Rel.par2', len(par), seg, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'Don' + tag, build_multi_nzb(donor_members), True, tag + '-key', 50)
    daemon.append(api, 'Rel' + tag, build_multi_nzb(members), False, tag + '-key', 100)
    h = daemon.wait_history(api, 'Rel' + tag, timeout=300)
    ok = _verify_output(t, movie, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return h, ok


def scenario_prodrarwhole(daemon, t):
    """Fix 2 on a rar set under the production option set: the third volume is
    missing entirely while direct unpack extracts the others. The volume is
    recreated, par-check passes, and unpack extracts a byte-identical movie."""
    h, ok = _prod_rar(daemon, t, 'RW', {2: set(range(1, 6))})
    recreated = _grep_log(t, 'Recreating')
    return ('prodrarwhole', ok and recreated == 1 and h['Status'].startswith('SUCCESS'),
            'status=%s par=%s unpack=%s recreated_logs=%d extracted_ok=%s'
            % (h['Status'], h['ParStatus'], h['UnpackStatus'], recreated, ok))


def scenario_prodrarstream(daemon, t):
    """Stream repair on a rar set under the production option set: holes in
    the first volume's start (its rar headers) and in the middle of the third
    while direct unpack runs; repaired, par-checked, extracted byte-identical."""
    h, ok = _prod_rar(daemon, t, 'RS', {0: {1}, 2: {3}})
    return ('prodrarstream', ok and h['Status'].startswith('SUCCESS'),
            'status=%s par=%s unpack=%s extracted_ok=%s' % (h['Status'], h['ParStatus'], h['UnpackStatus'], ok))


def scenario_wholefilenfoproof(daemon, t):
    """Corner case: a different packing of the release (same volume names and
    sizes, other bytes) that ships the SAME small .nfo. A byte match on the
    .nfo must not prove the duplicate for recreating a missing volume - only
    a sibling volume of the missing one can."""
    members, donor_members, payloads = _wholefile_fixture(t, 'nfo')
    # one other volume is damaged (one verification miss, below the bail)
    # and the two others are missing as well: the .nfo is the only intact
    # file, so before the set-key rule it was the one the proof used
    members[0] = members[0][:4] + (set(range(1, 4)),)
    members[3] = members[3][:4] + (set(range(1, 4)),)
    for i, dm in enumerate(donor_members):                 # other packing: other bytes
        t.write_file(os.path.join('data', dm[0]), _payload(dm[2], 9800 + i))
    nfo = _payload(3_000, 9890)
    t.write_file(os.path.join('data', 'nfoA/rel.nfo'), nfo)
    t.write_file(os.path.join('data', 'nfoB/rel.nfo'), nfo)
    members = [('nfoA/rel.nfo', 'Rel.nfo', len(nfo), 500_000, set())] + members
    donor_members = [('nfoB/rel.nfo', 'Other.nfo', len(nfo), 500_000, set())] + donor_members
    api = daemon.wait_ready()
    daemon.append(api, 'DonNFO', build_multi_nzb(donor_members), True, 'nfo-key', 50)
    daemon.append(api, 'RelNFO', build_multi_nzb(members), False, 'nfo-key', 100)
    h = daemon.wait_history(api, 'RelNFO')
    proven = _grep_log(t, 'verified on intact file')
    recreated = _grep_log(t, 'Recreating')
    return ('wholefilenfoproof', proven == 0 and recreated == 0,
            'status=%s proven_logs=%d recreated_logs=%d' % (h['Status'], proven, recreated))


def scenario_wholefilesampleproof(daemon, t):
    """Corner case: a different packing (same volume names and sizes, other
    bytes) that ships the SAME sample. The damaged sample is repaired from it
    byte-identically - that is fine - but it must not prove the duplicate
    for recreating a missing archive volume of another set."""
    members, donor_members, payloads = _wholefile_fixture(t, 'smp')
    members[1] = members[1][:4] + (set(),)
    for i, dm in enumerate(donor_members):                 # other packing: other bytes
        t.write_file(os.path.join('data', dm[0]), _payload(dm[2], 9850 + i))
    sample = _payload(900_000, 9899)
    t.write_file(os.path.join('data', 'smpA/sample.mkv'), sample)
    t.write_file(os.path.join('data', 'smpB/sample.mkv'), sample)
    members = members + [('smpA/sample.mkv', 'Rel.sample.mkv', len(sample), 300_000, {2})]
    donor_members = donor_members + [('smpB/sample.mkv', 'Other.sample.mkv', len(sample), 200_000, set())]
    api = daemon.wait_ready()
    daemon.append(api, 'DonSMP', build_multi_nzb(donor_members), True, 'smp-key', 50)
    daemon.append(api, 'RelSMP', build_multi_nzb(members), False, 'smp-key', 100)
    h = daemon.wait_history(api, 'RelSMP')
    sample_fixed = _grep_log(t, 'of Rel.sample.mkv from duplicate')
    recreated = _grep_log(t, 'Recreating')
    return ('wholefilesampleproof', recreated == 0,
            'status=%s sample_repaired_logs=%d recreated_logs=%d' % (h['Status'], sample_fixed, recreated))


def scenario_dupefailoverchain(daemon, t):
    """Corner case: the first backup is dead too. The primary fails over to
    it, it fails over to the second (healthy) backup, which completes; the
    parked primary is never brought back."""
    seg = 100_000
    vol_dead = 2_900_000
    n = vol_dead // seg
    def dead_set(tag, seed):
        files = [('%s/d%d.bin' % (tag, i), '%s%d.bin' % (tag, i), vol_dead, seg, set(range(1, n + 1)))
                 for i in range(6)]
        for m in files:
            t.write_file(os.path.join('data', m[0]), _payload(vol_dead, seed))
        return build_multi_nzb(files)
    data = _payload(3_000_000, 9921)
    bp = _place_copy(t, 'chC', data)
    healthy = build_nzb(bp, 'Healthy.bin', 3_000_000, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', dead_set('chA', 9919), True, 'ch-key', 100)
    daemon.append(api, 'Backup1', dead_set('chB', 9920), False, 'ch-key', 95)
    daemon.append(api, 'Backup2', healthy, False, 'ch-key', 90)
    daemon.wait_history(api, 'Backup2', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    deadline = time.time() + 400
    status = {}
    while time.time() < deadline:
        hist = {h['NZBName']: h['Status'] for h in api.history()}
        queued = {g['NZBName'] for g in api.listgroups()}
        if hist.get('Backup2', '').startswith('SUCCESS') and not queued:
            status = hist
            break
        time.sleep(1)
    status = status or {h['NZBName']: h['Status'] for h in api.history()}
    p_over = _grep_log(t, 'Failing over Primary to duplicate Backup1')
    b1_over = _grep_log(t, 'Failing over Backup1 to duplicate Backup2')
    returned = _grep_log(t, 'Found duplicate Primary')
    return ('dupefailoverchain', p_over == 1 and b1_over == 1 and returned == 0 and
            status.get('Backup2', '').startswith('SUCCESS'),
            'primary=%s backup1=%s backup2=%s primary_failover=%d backup1_failover=%d primary_returned=%d'
            % (status.get('Primary'), status.get('Backup1'), status.get('Backup2'), p_over, b1_over, returned))


def scenario_xpacklatency(daemon, t):
    """Cross-packing against a slow news server (1 s per response): requests
    that end without a server answer (no response line, a connection lost
    mid-article) must be retried instead of counting the article as missing,
    or a repost that carries every byte repairs nothing."""
    size, seg = 20_000_000, 700_000
    data = _payload(size, 4243)
    pp = _place_copy(t, 'xlA', data, 'movie.mkv')
    primary = build_nzb(pp, 'movie.mkv', size, seg, set(range(8, 20)))
    members = []
    for i, vol in enumerate(generators.rar3_store_volumes('movie.mkv', data, 5_000_000), 1):
        rel = 'xlB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        members.append((rel, 'Rel.part%02d.rar' % i, len(vol), seg, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'DonXL', build_multi_nzb(members), True, 'xl-key', 50)
    daemon.append(api, 'RelXL', primary, False, 'xl-key', 100)
    h = daemon.wait_history(api, 'RelXL', timeout=900)
    retried = _grep_log(t, 'no response from') + _grep_log(t, 'lost while fetching')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return ('xpacklatency', integ and h['Status'].startswith('SUCCESS'),
            'status=%s transient_logs=%d integrity=%s' % (h['Status'], retried, integ))


def scenario_xpackendhole_nodirect(daemon, t):
    """DirectWrite=no, holes up to the end of a bare movie: the file joined
    from article files ends with its last downloaded article, and the
    duplicate packs the movie into rar volumes. Cross-packing must map the
    target by its decoded size, not by the shorter file on disk. Before, the
    holes past the end of the file lay "outside the mappable inner stream"
    and nothing was recovered."""
    size, seg = 5_000_000, 333_333
    n = (size + seg - 1) // seg
    data = _payload(size, 4410)
    pp = _place_copy(t, 'xeA', data, 'movie.mkv')
    members = []
    for i, vol in enumerate(generators.rar3_store_volumes('movie.mkv', data, 1_500_000), 1):
        rel = 'xeB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        members.append((rel, 'Rel.part%02d.rar' % i, len(vol), 250_000, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'DonXE', build_multi_nzb(members), True, 'xe-key', 50)
    daemon.append(api, 'RelXE', build_nzb(pp, 'movie.mkv', size, seg, set(range(n - 2, n + 1))), False, 'xe-key', 100)
    h = daemon.wait_history(api, 'RelXE')
    unmappable = _grep_log(t, 'outside the mappable inner stream')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return ('xpackendhole_nodirect', integ and unmappable == 0 and h['Status'].startswith('SUCCESS'),
            'status=%s unmappable_logs=%d integrity=%s' % (h['Status'], unmappable, integ))


def scenario_xpackcorrupt(daemon, t):
    """The news server serves a corrupt copy of every duplicate article (one
    byte changed, crc32 left as declared) and CrcCheck=no: repair fetches
    check the yEnc crc32 anyway, so the corrupt articles are rejected and the
    release ends FAILURE/HEALTH. Before, cross-packing wrote the corrupt
    bytes and the release ended SUCCESS with a damaged movie."""
    size, seg = 3_000_000, 250_000
    data = _payload(size, 4501)
    pp = _place_copy(t, 'xcA', data, 'movie.mkv')
    members = []
    for i, vol in enumerate(generators.rar3_store_volumes('movie.mkv', data, 1_000_000), 1):
        rel = 'xcB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        members.append((rel, 'Rel.part%02d.rar' % i, len(vol), 300_000, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'DonXC', build_multi_nzb(members), True, 'xc-key', 50)
    daemon.append(api, 'RelXC', build_nzb(pp, 'movie.mkv', size, seg, {4, 5, 6}), False, 'xc-key', 100)
    h = daemon.wait_history(api, 'RelXC')
    corrupted = daemon.proxy.corrupted if daemon.proxy else 0
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return ('xpackcorrupt', corrupted >= 1 and h['Status'].startswith('FAILURE') and not integ,
            'status=%s corrupted_articles=%d integrity=%s' % (h['Status'], corrupted, integ))


def scenario_xpackextensionless(daemon, t):
    """An obfuscated bare movie posted without a file extension (par-rename
    can't restore the name while the first article is missing), repaired from
    rar volumes of the same movie. Before, only names with a media extension
    became bare sets, so cross-packing never mapped the target."""
    size, seg = 4_000_000, 250_000
    data = _payload(size, 4960)
    pp = _place_copy(t, 'xxA', data, 'movie.mkv')
    members = []
    for i, vol in enumerate(generators.rar3_store_volumes('movie.mkv', data, 1_500_000), 1):
        rel = 'xxB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        members.append((rel, 'Rel.part%02d.rar' % i, len(vol), 300_000, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'DonXX', build_multi_nzb(members), True, 'xx-key', 50)
    daemon.append(api, 'RelXX', build_nzb(pp, '848eddf3e2133c8a', size, seg, {1, 2, 9, 10}), False, 'xx-key', 100)
    h = daemon.wait_history(api, 'RelXX')
    integ = _verify_output(t, data, '848eddf3e2133c8a', dirs=(('main', 'dst'), ('main', 'inter')))
    return ('xpackextensionless', integ and h['Status'].startswith('SUCCESS'),
            'status=%s integrity=%s' % (h['Status'], integ))


def scenario_xpackextensionlessneg(daemon, t):
    """The negative: extensionless names that are really obfuscated rar
    volumes are tried as bare files too, and the identity probes must reject
    a bare duplicate of the movie inside them - nothing is written."""
    size = 4_000_000
    data = _payload(size, 4950)
    vols = generators.rar3_store_volumes('movie.mkv', data, 1_500_000)
    members, payloads = [], {}
    for i, vol in enumerate(vols, 1):
        rel = 'xnA/v%02d' % i
        t.write_file(os.path.join('data', rel), vol)
        name = '%016x' % (0x5eed0000 + i)
        payloads[name] = vol
        members.append((rel, name, len(vol), 250_000, {3, 4} if i == 2 else set()))
    dp = _place_copy(t, 'xnB', data, 'movie.mkv')
    api = daemon.wait_ready()
    daemon.append(api, 'DonXN', build_nzb(dp, 'movie.mkv', size, 300_000, set()), True, 'xn-key', 50)
    daemon.append(api, 'RelXN', build_multi_nzb(members), False, 'xn-key', 100)
    h = daemon.wait_history(api, 'RelXN')
    recovered = _grep_log(t, 'Recovered')
    both = (('main', 'dst'), ('main', 'inter'))
    intact = [_verify_output(t, v, n, dirs=both) for n, v in payloads.items()]
    # the damaged volume stays damaged, the others untouched
    return ('xpackextensionlessneg', recovered == 0 and intact.count(False) == 1 and
            h['Status'].startswith('FAILURE'),
            'status=%s recovered_logs=%d intact=%s' % (h['Status'], recovered, intact))


def scenario_manualparnopar(daemon, t):
    """ParCheck=manual, no par2 files: a release fully repaired from a
    duplicate is a success. Before, the repair pass asked for a par-check
    anyway and manual mode turned that into "needs manual repair"
    (WARNING/DAMAGED) although every byte was recovered."""
    size, seg = 3_000_000, 250_000
    data = _payload(size, 5100)
    pp = _place_copy(t, 'mpA', data, 'movie.mkv')
    dp = _place_copy(t, 'mpB', data, 'movie.mkv')
    api = daemon.wait_ready()
    daemon.append(api, 'DonMP', build_nzb(dp, 'Other.movie.mkv', size, 300_000, set()), True, 'mp-key', 50)
    daemon.append(api, 'RelMP', build_nzb(pp, 'movie.mkv', size, seg, {3, 4, 9}), False, 'mp-key', 100)
    h = daemon.wait_history(api, 'RelMP')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return ('manualparnopar', integ and h['Status'].startswith('SUCCESS'),
            'status=%s integrity=%s' % (h['Status'], integ))


def scenario_xpackflaky(daemon, t):
    """Cross-packing while the provider drops every connection and turns new
    ones away for 4 s (a per-user connection limit, see FlakyNntpProxy):
    the repeated attempts for a duplicate article are spaced instead of being
    used up within milliseconds, and the four dropped pooled connections
    don't use up attempts of their own. Before, the article counted as
    missing, the duplicate's content map could not be built, and nothing was
    recovered."""
    size, seg = 12_000_000, 300_000
    data = _payload(size, 4244)
    pp = _place_copy(t, 'xfA', data, 'movie.mkv')
    primary = build_nzb(pp, 'movie.mkv', size, seg, set(range(10, 30)))
    members = []
    for i, vol in enumerate(generators.rar3_store_volumes('movie.mkv', data, 3_000_000), 1):
        rel = 'xfB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        members.append((rel, 'Rel.part%02d.rar' % i, len(vol), seg, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'DonXF', build_multi_nzb(members), True, 'xf-key', 50)
    daemon.append(api, 'RelXF', primary, False, 'xf-key', 100)
    h = daemon.wait_history(api, 'RelXF', timeout=300)
    rejected = daemon.proxy.rejected if daemon.proxy else 0
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    return ('xpackflaky', integ and rejected >= 1 and h['Status'].startswith('SUCCESS'),
            'status=%s recovered=%d rejected_connects=%d integrity=%s'
            % (h['Status'], int(h.get('DupeRecoveredArticles', 0)), rejected, integ))


def scenario_xpackdeadserver(daemon, t):
    """Cross-packing while the preferred news server goes down for good at
    the first duplicate request (see SCENARIO_FLAKY_PROXY): once it used up
    its spaced attempts it gets a single attempt per article until it answers
    again, so it delays the repair once, not for every article."""
    size, seg = 12_000_000, 300_000
    data = _payload(size, 4245)
    pp = _place_copy(t, 'xdA', data, 'movie.mkv')
    primary = build_nzb(pp, 'movie.mkv', size, seg, set(range(10, 30)))
    members = []
    for i, vol in enumerate(generators.rar3_store_volumes('movie.mkv', data, 3_000_000), 1):
        rel = 'xdB/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        members.append((rel, 'Rel.part%02d.rar' % i, len(vol), seg, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'DonXD', build_multi_nzb(members), True, 'xd-key', 50)
    daemon.append(api, 'RelXD', primary, False, 'xd-key', 100)
    h = daemon.wait_history(api, 'RelXD', timeout=300)
    log = t.read_file('nzbget.log').decode(errors='replace')
    def when(pattern):
        m = re.search(r'^(\w{3} \w{3} +\d+ [\d:]+ \d{4})\t\w+\t' + pattern, log, re.M)
        return time.mktime(time.strptime(m.group(1), '%a %b %d %H:%M:%S %Y')) if m else None
    start, end = when('Cross-packing repair of'), when(r'Recovered .*\(cross-packing\)')
    seconds = end - start if start is not None and end is not None else None
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    rejected = daemon.proxy.rejected if daemon.proxy else 0
    return ('xpackdeadserver', integ and h['Status'].startswith('SUCCESS') and rejected >= 10 and
            seconds is not None and seconds <= 15,
            'status=%s cross_pack_seconds=%s rejected_connects=%d integrity=%s'
            % (h['Status'], seconds, rejected, integ))


def _wholefile_named(daemon, t, tag, primary_names, donor_names):
    """The whole-file fixture with custom member names on both sides: part03
    missing entirely, everything else intact; byte-identical repost."""
    members, donor_members, payloads = _wholefile_fixture(t, tag)
    members[1] = members[1][:4] + (set(),)
    members = [(m[0], primary_names[i]) + m[2:] for i, m in enumerate(members)]
    donor_members = [(m[0], donor_names[i]) + m[2:] for i, m in enumerate(donor_members)]
    payloads = {primary_names[i]: payloads['Rel.part%02d.rar' % (i + 1)] for i in range(4)}
    api = daemon.wait_ready()
    daemon.append(api, 'Don' + tag, build_multi_nzb(donor_members), True, tag + '-key', 50)
    daemon.append(api, 'Rel' + tag, build_multi_nzb(members), False, tag + '-key', 100)
    h = daemon.wait_history(api, 'Rel' + tag)
    recreated = _grep_log(t, 'Recreating')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    # by name: old-style volumes (x.r00) don't end in .rar
    integ = all(_verify_output(t, payloads[n], n, dirs=both_dirs) for n in primary_names)
    return h, recreated, integ


def scenario_wholefileunicode(daemon, t):
    """Names with spaces and non-ASCII characters on both sides."""
    h, recreated, integ = _wholefile_named(daemon, t, 'UNI',
        ['Rel Fête été.part%02d.rar' % i for i in range(1, 5)],
        ['Autre Ñame ü.part%02d.rar' % i for i in range(1, 5)])
    return ('wholefileunicode', integ and recreated == 1,
            'status=%s recreated_logs=%d integrity=%s' % (h['Status'], recreated, integ))


def scenario_wholefilepadding(daemon, t):
    """The duplicate zero-pads its volume numbers differently (part003 vs
    part03): the volumes are still siblings and the missing one pairs."""
    h, recreated, integ = _wholefile_named(daemon, t, 'PAD',
        ['Rel.part%03d.rar' % i for i in range(1, 5)],
        ['Other.part%02d.rar' % i for i in range(1, 5)])
    return ('wholefilepadding', integ and recreated == 1,
            'status=%s recreated_logs=%d integrity=%s' % (h['Status'], recreated, integ))


def scenario_wholefileoldstyle(daemon, t):
    """Old-style rar volume names (x.rar, x.r00, x.r01, ...) on both sides,
    with different base names: the volumes pair by number and the missing
    third volume (x.r01) is recreated."""
    h, recreated, integ = _wholefile_named(daemon, t, 'OLD',
        ['Rel.rar', 'Rel.r00', 'Rel.r01', 'Rel.r02'],
        ['Other.rar', 'Other.r00', 'Other.r01', 'Other.r02'])
    return ('wholefileoldstyle', integ and recreated == 1,
            'status=%s recreated_logs=%d integrity=%s' % (h['Status'], recreated, integ))


def scenario_wholefilecontinued(daemon, t):
    """An old-style set of more than 101 volumes continues x.r99 with x.s00,
    x.s01, ...: the missing x.s00 belongs to the x.rNN set, is proven on its
    siblings and recreated."""
    h, recreated, integ = _wholefile_named(daemon, t, 'CNT',
        ['Rel.r98', 'Rel.s00', 'Rel.s01', 'Rel.r99'],
        ['Other.r98', 'Other.s00', 'Other.s01', 'Other.r99'])
    return ('wholefilecontinued', integ and recreated == 1,
            'status=%s recreated_logs=%d integrity=%s' % (h['Status'], recreated, integ))


def scenario_wholefiletwosets(daemon, t):
    """Two archive sets in one release (Main.partNN.rar and Extras.partNN.rar),
    the duplicate carrying both under other names. Extras.part03.rar is
    missing entirely: it must be recreated from the duplicate's Extras set,
    never from its Main set's part03."""
    seg, seg_d = 500_000, 300_000
    vol = 1_500_000
    n = (vol + seg - 1) // seg
    members, donor_members, payloads = [], [], {}
    for set_name, other_name, seed in (('Main', 'OMain', 9600), ('Extras', 'OExtras', 9700)):
        for i in range(4):
            data = _payload(vol, seed + i)
            name = '%s.part%02d.rar' % (set_name, i + 1)
            payloads[name] = data
            pa = 'tsA/%s%d' % (set_name, i)
            pb = 'tsB/%s%d' % (set_name, i)
            t.write_file(os.path.join('data', pa), data)
            t.write_file(os.path.join('data', pb), data)
            missing = set(range(1, n + 1)) if (set_name == 'Extras' and i == 2) else set()
            members.append((pa, name, vol, seg, missing))
            donor_members.append((pb, '%s.part%02d.rar' % (other_name, i + 1), vol, seg_d, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'DonTS', build_multi_nzb(donor_members), True, 'ts-key', 50)
    daemon.append(api, 'RelTS', build_multi_nzb(members), False, 'ts-key', 100)
    h = daemon.wait_history(api, 'RelTS')
    recreated = _grep_log(t, 'Recreating Extras.part03.rar')
    wrong = _grep_log(t, 'from file OMain.part03.rar')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[n], n, dirs=both_dirs) for n in payloads)
    return ('wholefiletwosets', integ and recreated == 1 and wrong == 0,
            'status=%s recreated_logs=%d wrong_set_logs=%d integrity=%s' % (h['Status'], recreated, wrong, integ))


def scenario_dupefailovernonzb(daemon, t):
    """Corner case: the best backup's source nzb-file is gone from NzbDir, so
    it can't be downloaded again. The failover must pick the next backup that
    can (and never park the download for one that can't)."""
    seg = 100_000
    vol_dead = 2_900_000
    n = vol_dead // seg
    dead = [('nnA/d%d.bin' % i, 'Dead%d.bin' % i, vol_dead, seg, set(range(1, n + 1))) for i in range(6)]
    for m in dead:
        t.write_file(os.path.join('data', m[0]), _payload(vol_dead, 9930))
    data1, data2 = _payload(3_000_000, 9931), _payload(3_000_000, 9932)
    b1 = build_nzb(_place_copy(t, 'nnB', data1), 'Gone.bin', 3_000_000, seg, set())
    b2 = build_nzb(_place_copy(t, 'nnC', data2), 'Kept.bin', 3_000_000, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(dead), True, 'nn-key', 100)
    daemon.append(api, 'BackupGone', b1, False, 'nn-key', 95)
    daemon.append(api, 'BackupKept', b2, False, 'nn-key', 90)
    daemon.wait_history(api, 'BackupKept', timeout=60)
    nzbdir = t.path('main', 'nzb')
    for name in os.listdir(nzbdir):
        if 'BackupGone' in name:
            os.remove(os.path.join(nzbdir, name))
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary')
    deadline = time.time() + 180
    hk = daemon.wait_history(api, 'BackupKept')
    while hk['Status'].startswith('DELETED') and time.time() < deadline:
        time.sleep(1)
        hk = daemon.wait_history(api, 'BackupKept')
    to_gone = _grep_log(t, 'Failing over Primary to duplicate BackupGone')
    to_kept = _grep_log(t, 'Failing over Primary to duplicate BackupKept')
    return ('dupefailovernonzb', to_gone == 0 and to_kept == 1 and hk['Status'].startswith('SUCCESS'),
            'primary=%s kept=%s failover_to_gone=%d failover_to_kept=%d'
            % (hp['Status'], hk['Status'], to_gone, to_kept))


def scenario_wholefileproofcost(daemon, t):
    """Cost bound: six volumes missing entirely and a duplicate that is a
    different packing (same names and sizes, other bytes). The proof on an
    intact sibling fails - and must not be repeated for every missing volume
    of the same set: the duplicate's articles are fetched a bounded number
    of times, not once per missing volume."""
    seg, seg_d = 500_000, 300_000
    vol = 1_500_000
    n = (vol + seg - 1) // seg
    members, donor_members = [], []
    for i in range(10):
        data = _payload(vol, 9500 + i)
        pa, pb = 'pcA/v%d' % i, 'pcB/v%d' % i
        t.write_file(os.path.join('data', pa), data)
        t.write_file(os.path.join('data', pb), _payload(vol, 9600 + i))     # other packing
        missing = set(range(1, n + 1)) if i in (2, 3, 4, 5, 6, 7) else set()
        members.append((pa, 'Rel.part%02d.rar' % (i + 1), vol, seg, missing))
        donor_members.append((pb, 'Other.part%02d.rar' % (i + 1), vol, seg_d, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'DonPC', build_multi_nzb(donor_members), True, 'pc-key', 50)
    daemon.append(api, 'RelPC', build_multi_nzb(members), False, 'pc-key', 100)
    h = daemon.wait_history(api, 'RelPC')
    try:
        log = t.read_file('nserv.log').decode(errors='replace')
    except Exception:
        log = ''
    donor_bodies = len(re.findall(r'BODY <pcB/', log))
    recreated = _grep_log(t, 'Recreating')
    # one failed proof: 2 intact volumes x their candidates x a few probes
    return ('wholefileproofcost', recreated == 0 and donor_bodies <= 32,
            'status=%s recreated_logs=%d donor_body_requests=%d' % (h['Status'], recreated, donor_bodies))


def scenario_streamretry(daemon, t):
    """Corner case (no whole-file job involved): stream repair fully repairs
    one volume and leaves a hole in another that no duplicate carries, so the
    release ends FAILURE/HEALTH. "Retry failed articles" can't fill that hole
    either - the release must stay a failure, never turn into a SUCCESS with
    a hole in a file."""
    members, donor_members, payloads = _wholefile_fixture(t, 'sr')
    members[2] = members[2][:4] + (set(),)               # part03 complete
    members[3] = members[3][:4] + ({2},)                 # part04 article 2 missing ...
    donor_members[3] = donor_members[3][:4] + (set(range(2, 4)),)  # ... on the duplicate too
    api = daemon.wait_ready()
    daemon.append(api, 'DonSR', build_multi_nzb(donor_members), True, 'sr-key', 50)
    daemon.append(api, 'RelSR', build_multi_nzb(members), False, 'sr-key', 100)
    h = daemon.wait_history(api, 'RelSR')
    nzbid = h['NZBID']
    api.editqueue('HistoryRetryFailed', 0, '', [nzbid])
    time.sleep(3)
    deadline = time.time() + 180
    while time.time() < deadline and any(g['NZBID'] == nzbid for g in api.listgroups()):
        time.sleep(1)
    h2 = [x for x in api.history() if x['NZBID'] == nzbid][0]
    return ('streamretry', h['Status'].startswith('FAILURE') and h2['Status'].startswith('FAILURE'),
            'status=%s health=%d retry_status=%s retry_health=%d'
            % (h['Status'], h['Health'], h2['Status'], h2['Health']))


def scenario_wholefilelive(daemon, t):
    """Fix 2 under DupeArticleFallback=live: the zero-article volume
    completes while the rest still downloads (throttled), the live pass
    proves the donor on the holed volume and recreates the missing one, and
    the release completes SUCCESS byte-identically."""
    members, donor_members, payloads = _wholefile_fixture(t, 'wl')
    big = _payload(12_000_000, 9350)
    t.write_file(os.path.join('data', 'wlA/z.bin'), big)
    members = members + [('wlA/z.bin', 'Rel.zz.bin', len(big), 500_000, set())]
    api = daemon.wait_ready()
    daemon.append(api, 'DonWL', build_multi_nzb(donor_members), True, 'wl-key', 50)
    daemon.append(api, 'RelWL', build_multi_nzb(members), False, 'wl-key', 100)
    h = daemon.wait_history(api, 'RelWL', timeout=300)
    live = _grep_log(t, 'Starting live stream repair')
    recreated = _grep_log(t, 'Recreating Rel.part03.rar')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=both_dirs) for m in members[:4])
    return ('wholefilelive', integ and recreated == 1,
            'status=%s live_logs=%d recreated_logs=%d integrity=%s' % (h['Status'], live, recreated, integ))


def scenario_dupehopelessnodupecheck(daemon, t):
    """Corner case: HealthCheck=dupe with DupeCheck=no. There is no
    duplicate handling, so no failover - but a hopeless download (a dead
    posting) must still be parked, as the option help promises, instead of
    failing every article."""
    seg = 100_000
    vol_dead = 2_900_000
    n = vol_dead // seg
    dead = [('hnA/d%d.bin' % i, 'Dead%d.bin' % i, vol_dead, seg, set(range(1, n + 1))) for i in range(6)]
    for m in dead:
        t.write_file(os.path.join('data', m[0]), _payload(vol_dead, 9800))
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(dead), False, 'hn-key', 100)
    hp = daemon.wait_history(api, 'Primary')
    parked = _grep_log(t, 'Parking Primary: health')
    failed_articles = int(hp.get('FailedArticles', 0))
    return ('dupehopelessnodupecheck', parked == 1 and failed_articles < 140,
            'status=%s parked_logs=%d failed_articles=%d' % (hp['Status'], parked, failed_articles))


def scenario_dupefailovernofallback(daemon, t):
    """Corner case: HealthCheck=dupe with DupeArticleFallback=no (no sample
    gate). A posting that merely misses its first stretch must not be
    abandoned on its first failed article in favour of a lower-scored
    backup: it is still at almost full health then, and warrants more than
    the backup's score."""
    seg = 100_000
    vol = 6_000_000
    data = _payload(vol, 9900)
    pp = _place_copy(t, 'nfA', data)
    primary = build_nzb(pp, 'Main.bin', vol, seg, set(range(1, 4)))
    bp = _place_copy(t, 'nfB', _payload(3_000_000, 9901))
    backup = build_nzb(bp, 'Backup.bin', 3_000_000, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', primary, True, 'nf-key', 100)
    daemon.append(api, 'Backup', backup, False, 'nf-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary')
    failed_over = _grep_log(t, 'Failing over Primary')
    parked = _grep_log(t, 'Parking Primary')
    return ('dupefailovernofallback', failed_over == 0 and parked == 0,
            'status=%s failover_logs=%d parked_logs=%d' % (hp['Status'], failed_over, parked))


def scenario_dupefailover(daemon, t):
    """HealthCheck=dupe: the primary is a dead posting (every article of
    every file missing) and no duplicate carries its files, while a healthy
    lower-scored backup of the same title waits in history (deleted as
    duplicate at intake). The download must be abandoned early - long before
    all of its articles have failed - and the backup fetched and completed
    in its place. The backup's score (90) is above what the primary (100)
    warrants at the health it has when the sample completes, so the
    failover itself fires (not the hopeless-park, see dupehopeless)."""
    seg = 100_000
    # the dead posting is packaged differently from the backup (other
    # volume size and article count), so no article can be borrowed from it
    vol_dead, vol_backup = 2_900_000, 3_000_000
    n = vol_dead // seg
    dead = [('foA/d%d.bin' % i, 'Dead%d.bin' % i, vol_dead, seg, set(range(1, n + 1)))
            for i in range(6)]
    for m in dead:
        t.write_file(os.path.join('data', m[0]), _payload(vol_dead, 9500))
    data = _payload(vol_backup, 9501)
    bp = _place_copy(t, 'foB', data)
    backup = build_nzb(bp, 'Backup.bin', vol_backup, seg, set())
    api = daemon.wait_ready()
    # the primary is queued paused first, so the lower-scored backup is
    # deleted as duplicate into history instead of downloading
    daemon.append(api, 'Primary', build_multi_nzb(dead), True, 'fo-key', 100)
    daemon.append(api, 'Backup', backup, False, 'fo-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups()
                                         if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary')
    # the backup leaves history while it downloads and returns finished
    deadline = time.time() + 180
    hb = daemon.wait_history(api, 'Backup')
    while hb['Status'].startswith('DELETED') and time.time() < deadline:
        time.sleep(0.5)
        hb = daemon.wait_history(api, 'Backup')
    failed_over = _grep_log(t, 'Failing over Primary to duplicate Backup')
    failed_articles = int(hp.get('FailedArticles', 0))
    integ = _verify_output(t, data)
    # the primary has 174 articles; the failover fires once 32 were tried
    # against the (unusable) duplicate and the health is below critical
    early = failed_articles < 140
    return ('dupefailover', failed_over == 1 and early and integ and
            hb['Status'].startswith('SUCCESS'),
            'status=%s backup_status=%s failover_logs=%d failed_articles=%d integrity=%s'
            % (hp['Status'], hb['Status'], failed_over, failed_articles, integ))


DS_TITLE = 'Show.S01E01.1080p.WEB.H264-GRP'
DS_KEY = 'tvdbid=1-S01-E01|show-s01e01'
DS_PICK = 23859118


def _ds_nzb(t, tag, size=300_000):
    data = _payload(size, 9950 + len(tag))
    path = _place_copy(t, 'ds' + tag, data)
    return build_nzb(path, 'a.bin', size, 100_000, set())


def _ds_append(api, name, nzb, key, score, paused=True, params=()):
    content = base64.standard_b64encode(nzb.encode()).decode()
    return api.append(name, content, 'test', 0, False, paused, key, score, 'score', list(params))


def _ds_group(api, name):
    return next((g for g in api.listgroups() if g['NZBName'] == name), None)


def scenario_dupesearchtrigger(daemon, t):
    """DupeSearch decides which downloads it searches: the pick of a client
    that also submits two backups of its own (scored one and two below) is
    searched once, after DupeSearchDelay, and the backups - filed in history as
    duplicates by the duplicate check - never are."""
    api = daemon.wait_ready()
    _ds_append(api, DS_TITLE, _ds_nzb(t, 'p'), DS_KEY, DS_PICK)
    _ds_append(api, DS_TITLE + '.b1', _ds_nzb(t, 'b1'), DS_KEY, DS_PICK - 1)
    _ds_append(api, DS_TITLE + '.b2', _ds_nzb(t, 'b2'), DS_KEY, DS_PICK - 2)
    time.sleep(6)
    searched = _grep_log(t, 'DupeSearch: searching duplicates of %s ' % DS_TITLE)
    backups = _grep_log(t, 'DupeSearch: searching duplicates of %s.b' % DS_TITLE)
    pick = _ds_group(api, DS_TITLE)
    score = int(pick['DupeScore']) if pick else -1
    return ('dupesearchtrigger', searched == 1 and backups == 0 and score == DS_PICK,
            'searched_logs=%d backup_searched_logs=%d pick_score=%d' % (searched, backups, score))


def scenario_dupesearchkey(daemon, t):
    """A pick without a duplicate key and with a score of 0 gets "dupes:" + its
    normalized title as key and the managed score of 1,000,000, which leaves
    room below it for duplicates."""
    api = daemon.wait_ready()
    _ds_append(api, DS_TITLE, _ds_nzb(t, 'k'), '', 0)
    time.sleep(6)
    pick = _ds_group(api, DS_TITLE)
    key = pick['DupeKey'] if pick else None
    score = int(pick['DupeScore']) if pick else -1
    searched = _grep_log(t, 'DupeSearch: searching duplicates of %s ' % DS_TITLE)
    return ('dupesearchkey', searched == 1 and key == 'dupes:show.s01e01.1080p.web.h264.grp' and score == 1_000_000,
            'searched_logs=%d key=%s score=%d' % (searched, key, score))


def scenario_dupesearchsearch(daemon, t):
    """The search of a pick: the full title query reads three pages (250
    results), the short query one page, the short query with the group gets an
    indexer error; the failing query doesn't fail the search. Results are
    merged by link and filtered to the same release, and the api key reaches
    the indexer but never the log."""
    full, short, grouped = 'show s01e01 1080p web h264 grp', 'show s01e01 1080p', 'show s01e01 1080p grp'
    api_key = 'SECRETKEY123'

    def item(i, same):
        title = 'Show S01E01 1080p WEB H264-GRP' if same else 'Show.S01E01.1080p.WEB.H264-OTHER%d' % i
        return {'title': title, 'link': 'http://127.0.0.1:1/getnzb/%d?apikey=%s' % (i, api_key),
                'size': 1_000_000 + i, 'grabs': i % 7, 'indexer': 'Idx%d' % (i % 3)}

    def respond(params, path):
        q, offset, limit = params.get('q'), int(params.get('offset', 0)), int(params.get('limit', 0))
        if params.get('apikey') != api_key:
            return 200, newznab_xml([], error=(100, 'Wrong api key'))
        if q == full:
            return 200, newznab_xml([item(i, i % 5 == 0) for i in range(offset, min(offset + limit, 250))])
        if q == short:      # two results of the full query again, and a new one of the same release
            return 200, newznab_xml([item(0, True), item(1, False), item(900, True)])
        if q == grouped:
            return 200, newznab_xml([], error=(300, 'That search could not be run'))
        return 200, newznab_xml([])

    daemon.newznab.respond = respond
    api = daemon.wait_ready()
    _ds_append(api, DS_TITLE, _ds_nzb(t, 's'), DS_KEY, DS_PICK)
    deadline = time.time() + 20
    while time.time() < deadline and _grep_log(t, 'DupeSearch: %s: ' % DS_TITLE) == 0:
        time.sleep(0.5)
    reqs = list(daemon.newznab.requests)
    log = t.read_file('nzbget.log').decode(errors='replace')
    offsets = sorted(int(r['offset']) for r in reqs if r.get('q') == full)
    short_pages = sum(1 for r in reqs if r.get('q') == short)
    grouped_pages = sum(1 for r in reqs if r.get('q') == grouped)
    keys_sent = all(r.get('apikey') == api_key for r in reqs)
    summary = _grep_log(t, 'DupeSearch: %s: 251 result(s) from 3 search(es) (1 failed, 5 page(s)), 51 of them the same release' % DS_TITLE)
    error_logged = 'error 300' in log
    leaked = api_key in log
    return ('dupesearchsearch', offsets == [0, 100, 200] and short_pages == 1 and grouped_pages == 1 and
            len(reqs) == 5 and keys_sent and summary == 1 and error_logged and not leaked,
            'full_offsets=%s short_pages=%d grouped_pages=%d requests=%d summary_logs=%d error_logged=%s api_key_in_log=%s'
            % (offsets, short_pages, grouped_pages, len(reqs), summary, error_logged, leaked))


def _fake_nzb(prefix, total_bytes, name='a.mkv'):
    """An nzb-file of one file of ``total_bytes`` whose articles are named after ``prefix``."""
    return ('<?xml version="1.0" encoding="UTF-8"?>\n<nzb xmlns="http://www.newzbin.com/DTD/2003/nzb">'
            '<file poster="p@x" date="1" subject="[1/1] &quot;%s&quot; yEnc (1/1)"><groups><group>alt.binaries.test</group>'
            '</groups><segments><segment bytes="%d" number="1">%s-1@x</segment></segments></file></nzb>'
            % (name, total_bytes, prefix)).encode()


def scenario_dupesearchfetch(daemon, t):
    """Fetching the postings the search found: a posting that three indexers
    list is fetched once (from its most grabbed listing); an indexer that
    answers 403 is asked twice (one retry), then not asked at all (its other
    listing is refused without a request, and the refusal is kept on disk);
    a listing that serves an nzb-file of another size is skipped for the
    posting's next listing, and only used when no listing serves the posting."""
    same = 'Show S01E01 1080p WEB H264-GRP'

    def listing(link, size, grabs, indexer, date):
        return {'title': same, 'link': 'http://127.0.0.1:%d/getnzb/%s' % (daemon.newznab.port, link),
                'size': size, 'grabs': grabs, 'indexer': indexer, 'date': date}

    results = [
        listing('p1a', 310_000, 9, 'IdxA', 'Tue, 10 Jun 2025 01:10:05 +0000'),
        listing('p1b', 310_000, 5, 'IdxB', 'Tue, 10 Jun 2025 01:10:20 +0000'),
        listing('p1c', 310_000, 1, 'IdxC', 'Tue, 10 Jun 2025 01:10:40 +0000'),
        listing('p2', 320_000, 1, 'Capped', 'Wed, 11 Jun 2025 01:10:05 +0000'),
        listing('p3', 330_000, 1, 'Capped', 'Thu, 12 Jun 2025 01:10:05 +0000'),
        listing('p4m', 340_000, 9, 'IdxM', 'Fri, 13 Jun 2025 01:10:05 +0000'),
        listing('p4n', 340_000, 3, 'IdxN', 'Fri, 13 Jun 2025 01:10:30 +0000'),
        listing('p5', 350_000, 1, 'IdxM', 'Sat, 14 Jun 2025 01:10:05 +0000'),
    ]
    served = {'p1a': 310_000, 'p1b': 310_000, 'p1c': 310_000, 'p4m': 380_000, 'p4n': 340_000, 'p5': 400_000}

    def respond(params, path):
        if path.startswith('/getnzb/'):
            name = path.rsplit('/', 1)[-1]
            if name in ('p2', 'p3'):
                return 403, b''
            return 200, _fake_nzb(name, served[name])
        if params.get('q') == 'show s01e01 1080p web h264 grp':
            return 200, newznab_xml(results)
        return 200, newznab_xml([])

    daemon.newznab.respond = respond
    api = daemon.wait_ready()
    _ds_append(api, DS_TITLE, _ds_nzb(t, 'f'), DS_KEY, DS_PICK)
    deadline = time.time() + 40
    while time.time() < deadline and _grep_log(t, 'verified=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    grabs = {}
    for r in daemon.newznab.requests:
        if r['_path'].startswith('/getnzb/'):
            name = r['_path'].rsplit('/', 1)[-1]
            grabs[name] = grabs.get(name, 0) + 1
    capped = grabs.get('p2', 0) + grabs.get('p3', 0)
    summary = _grep_log(t, 'DupeSearch: %s: results=8 candidates=8 postings=5 verified=3 added=3 '
                           'rejected={fetch: 1, listing-mismatch: 1, refused: 1, relisted: 2}' % DS_TITLE)
    state = t.read_file(os.path.join('main', 'queue', 'dupesearch-indexers')).decode(errors='replace') \
        if t.exists(os.path.join('main', 'queue', 'dupesearch-indexers')) else ''
    kept = state.startswith('Capped\t')
    exact = {k: v for k, v in grabs.items() if k not in ('p2', 'p3')}
    return ('dupesearchfetch', exact == {'p1a': 1, 'p4m': 1, 'p4n': 1, 'p5': 1} and capped == 2 and
            summary == 1 and kept,
            'grabs=%s capped_requests=%d summary_logs=%d cooldown_kept=%s' % (exact, capped, summary, kept))


def scenario_dupesearchfetcherror(daemon, t):
    """An error answer served with HTTP 200 (code 429, "request limit
    reached") is no nzb-file: the download is retried once and counted as not
    an nzb-file, but it starts no cooldown - only HTTP 403 and 429 do."""
    listing = {'title': 'Show S01E01 1080p WEB H264-GRP', 'size': 310_000, 'grabs': 1, 'indexer': 'IdxE',
               'link': 'http://127.0.0.1:%d/getnzb/pe' % daemon.newznab.port}

    def respond(params, path):
        if path.startswith('/getnzb/'):
            return 200, b'<?xml version="1.0" encoding="UTF-8"?>\n<error code="429" description="Request limit reached"/>'
        if params.get('q') == 'show s01e01 1080p web h264 grp':
            return 200, newznab_xml([listing])
        return 200, newznab_xml([])

    daemon.newznab.respond = respond
    api = daemon.wait_ready()
    _ds_append(api, DS_TITLE, _ds_nzb(t, 'e'), DS_KEY, DS_PICK)
    deadline = time.time() + 40
    while time.time() < deadline and _grep_log(t, 'verified=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    grabs = sum(1 for r in daemon.newznab.requests if r['_path'] == '/getnzb/pe')
    summary = _grep_log(t, 'DupeSearch: %s: results=1 candidates=1 postings=1 verified=0 added=0 rejected={parse: 1}' % DS_TITLE)
    cooldown = t.exists(os.path.join('main', 'queue', 'dupesearch-indexers')) and \
        len(t.read_file(os.path.join('main', 'queue', 'dupesearch-indexers')).strip()) > 0
    return ('dupesearchfetcherror', grabs == 2 and summary == 1 and not cooldown,
            'grabs=%d summary_logs=%d cooldown_started=%s' % (grabs, summary, cooldown))


def _fake_nzb_ids(ids, total_bytes, name='a.mkv'):
    """An nzb-file of one file whose articles are exactly ``ids``."""
    seg = max(1, total_bytes // len(ids))
    segs = ''.join('<segment bytes="%d" number="%d">%s</segment>' % (seg, i + 1, m) for i, m in enumerate(ids))
    return ('<?xml version="1.0" encoding="UTF-8"?>\n<nzb xmlns="http://www.newzbin.com/DTD/2003/nzb">'
            '<file poster="p@x" date="1" subject="[1/1] &quot;%s&quot; yEnc (1/1)"><groups><group>alt.binaries.test</group>'
            '</groups><segments>%s</segments></file></nzb>' % (name, segs)).encode()


def scenario_dupesearchfilters(daemon, t):
    """What the search keeps among the postings it fetched, in the order of
    the filters: another release (named by the largest data file, only when
    the name is readable: an obfuscated one never rejects), a posting nzbget
    already holds (a backup in history, read from its copy in NzbDir), a
    posting found dead before (kept on disk), a posting that shares articles
    with the pick or an accepted duplicate. Two fresh postings are kept."""
    import zlib
    same = 'Show S01E01 1080p WEB H264-GRP'
    ids = lambda p, n=40: ['%s-%d@x' % (p, i) for i in range(n)]
    held_nzb = _ds_nzb(t, 'held')                       # a backup the user keeps in history

    postings = {
        # (ids, main file name) served per link; sizes are listed equal to the served total
        'other':   (ids('other'), 'Other.Show.S09E09.1080p.WEB.H264-XYZ.mkv'),
        'obf':     (ids('obf'),   'e630b420289687dc3c66cc3274207902.mkv'),             # obfuscated: not rejected
        'dead':    (ids('dead'),  'a.mkv'),
        'fresh':   (ids('fresh'), 'Show.S01E01.1080p.WEB.H264-GRP.mkv'),
        'resend':  (ids('fresh', 40)[:39] + ['reup@x'], 'a.mkv'),                      # the same posting, one article re-uploaded
    }
    # the posting nzbget holds is served again by an indexer: its articles are in the held nzb-file
    held_ids = re.findall(r'<segment[^>]*>([^<]+)</segment>', held_nzb)
    postings['held'] = (held_ids, 'a.mkv')

    dead_sketch = sorted({zlib.crc32(i.encode()) for i in postings['dead'][0]})[:64]

    sizes = {name: 100_000 + 10_000 * k for k, name in enumerate(['fresh', 'resend', 'obf', 'other', 'dead', 'held'])}
    results = [{'title': same, 'link': 'http://127.0.0.1:%d/getnzb/%s' % (daemon.newznab.port, name),
                'size': sizes[name], 'grabs': 1, 'indexer': 'Idx%s' % name,
                'date': 'Tue, 1%d Jun 2025 01:10:05 +0000' % k} for k, name in enumerate(sizes)]

    def respond(params, path):
        if path.startswith('/getnzb/'):
            name = path.rsplit('/', 1)[-1]
            return 200, _fake_nzb_ids(postings[name][0], sizes[name], postings[name][1])
        if params.get('q') == 'show s01e01 1080p web h264 grp':
            return 200, newznab_xml(results)
        return 200, newznab_xml([])

    # the dead posting is remembered from an earlier search: restart with the record in place
    api = daemon.wait_ready()
    try:
        api.shutdown()
    except Exception:
        pass
    time.sleep(3)
    t.write_file(os.path.join('main', 'queue', 'dupesearch-dead'),
                 ('%d\t%s\t\n' % (int(time.time()), ','.join(str(h) for h in dead_sketch))).encode())
    daemon.start()
    api = daemon.wait_ready()
    daemon.newznab.respond = respond

    # the held posting sits in history as a backup (lower score, same key), filed by the duplicate check
    _ds_append(api, DS_TITLE, _ds_nzb(t, 'pick'), DS_KEY, DS_PICK)
    _ds_append(api, DS_TITLE + '.held', held_nzb, DS_KEY, DS_PICK - 5)
    deadline = time.time() + 40
    while time.time() < deadline and _grep_log(t, 'verified=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    summary = _grep_log(t, 'DupeSearch: %s: results=6 candidates=6 postings=6 verified=2 added=0 '
                           'rejected={dead: 2, in-nzbget: 1, known-dead: 1, other-release: 1, same-posting: 1}' % DS_TITLE)
    return ('dupesearchfilters', summary == 1, 'summary_logs=%d' % summary)


def scenario_dupesearchdonors(daemon, t):
    """Donors are scored by wholeness: a pick scored like a client's (23859118)
    with two backups of the client's own, five postings on the indexer: a
    byte-identical twin 100% alive, another packaging 100%, one 95%, a twin
    90%, and one that no server holds. The dead one is dropped; the others are
    added below the backups (the first two at once, after the probe, the rest as
    their full samples land) and end up scored base + 90, 89, 85, 82 (base is
    the pick's score minus 1000)."""
    n = 40
    ids = lambda p, alive=n: ['%s-%d@x' % (p, i) for i in range(n)]
    postings = {
        'twin100': (ids('tw'), 400_000, 1, 11),
        'other100': (ids('ot'), 410_000, 1, 12),
        'other95': (ids('o5'), 420_000, 40, 13),
        'twin90': (ids('t9'), 400_000, 50, 14),
        'gone': (ids('gn'), 430_000, 5, 15),
    }
    alive = set(ids('pk'))
    alive |= set(postings['twin100'][0]) | set(postings['other100'][0])
    alive |= set(postings['other95'][0][:38]) | set(postings['twin90'][0][:36])
    daemon.fake_nntp.alive = alive
    results = [{'title': DS_TITLE, 'link': 'http://127.0.0.1:%d/getnzb/%s' % (daemon.newznab.port, name),
                'size': size, 'grabs': grabs, 'indexer': 'Idx' + name,
                'date': 'Tue, %d Jun 2025 01:10:05 +0000' % day}
               for name, (_, size, grabs, day) in postings.items()]

    def respond(params, path):
        if path.startswith('/getnzb/'):
            name = path.rsplit('/', 1)[-1]
            return 200, _fake_nzb_ids(postings[name][0], postings[name][1])
        if params.get('q') == 'show s01e01 1080p web h264 grp':
            return 200, newznab_xml(results)
        return 200, newznab_xml([])

    api = daemon.wait_ready()
    daemon.newznab.respond = respond
    _ds_append(api, DS_TITLE, _fake_nzb_ids(ids('pk'), 400_000).decode(), DS_KEY, DS_PICK)
    _ds_append(api, DS_TITLE + '.b1', _ds_nzb(t, 'b1'), DS_KEY, DS_PICK - 1)
    _ds_append(api, DS_TITLE + '.b2', _ds_nzb(t, 'b2'), DS_KEY, DS_PICK - 2)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    base = DS_PICK - 1000
    got = sorted((h.get('DupeScore') for h in api.history() if h.get('NZBName') == DS_TITLE), reverse=True)
    summary = _grep_log(t, 'verified=5 added=4 rejected={dead: 1}')
    fast = _grep_log(t, '(fast)')
    checked = _grep_log(t, '(checked)')
    ok = got == [base + 90, base + 89, base + 85, base + 82] and summary == 1 and fast == 2 and checked == 2
    return ('dupesearchdonors', ok, 'scores=%s summary=%d fast=%d checked=%d' % ([g - base for g in got], summary, fast, checked))


def _ds_donor_env(daemon, t, postings, alive):
    """The indexer and the news server of a donors scenario: postings maps a name to
    (ids, size, grabs, day); returns the api once the pick and its backups are queued."""
    daemon.fake_nntp.alive = set(alive)
    results = [{'title': DS_TITLE, 'link': 'http://127.0.0.1:%d/getnzb/%s' % (daemon.newznab.port, name),
                'size': size, 'grabs': grabs, 'indexer': 'Idx' + name,
                'date': 'Tue, %d Jun 2025 01:10:05 +0000' % day}
               for name, (_, size, grabs, day) in postings.items()]

    def respond(params, path):
        if path.startswith('/getnzb/'):
            name = path.rsplit('/', 1)[-1]
            return 200, _fake_nzb_ids(postings[name][0], postings[name][1])
        if params.get('q') == 'show s01e01 1080p web h264 grp':
            return 200, newznab_xml(results)
        return 200, newznab_xml([])

    api = daemon.wait_ready()
    daemon.newznab.respond = respond
    _ds_append(api, DS_TITLE, _fake_nzb_ids(['pk-%d@x' % i for i in range(40)], 400_000).decode(), DS_KEY, DS_PICK)
    daemon.fake_nntp.alive |= {'pk-%d@x' % i for i in range(40)}
    return api


def scenario_dupesearchgroup(daemon, t):
    """The whole duplicate key is ranked, not only the search's own donors: a
    client sends the pick and three backups of its own, scored just below the
    pick (so nzbget would try them first): one 50% alive, one dead, one whole.
    The indexer has one more posting, 95% alive. After the search every
    DELETED/DUPE member carries its measured DupeAlive and they are scored by
    wholeness: whole backup base + 89, donor base + 85, half-dead backup
    base + 49, dead backup base + 1. The pick keeps its score."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    postings = {'idx95': (ids('ix'), 420_000, 3, 11)}
    alive = set(ids('ix')[:38]) | set(ids('half')[:20]) | set(ids('whole'))
    api = _ds_donor_env(daemon, t, postings, alive)
    backups = (('half', 1, 430_000), ('gone', 2, 440_000), ('whole', 3, 410_000))
    for tag, below, size in backups:
        _ds_append(api, DS_TITLE + '.' + tag, _fake_nzb_ids(ids(tag), size).decode(), DS_KEY, DS_PICK - below)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(2)
    base = DS_PICK - 1000
    got = {}
    for h in api.history():
        name = h.get('NZBName')
        tag = name.rsplit('.', 1)[-1] if name != DS_TITLE else 'idx95'
        params = {p['Name']: p['Value'] for p in h.get('Parameters', [])}
        got[tag] = (h.get('DupeScore') - base, params.get('DupeAlive'))
    pick = _ds_group(api, DS_TITLE)
    want = {'whole': (89, '100'), 'idx95': (85, '95'), 'half': (49, '50'), 'gone': (1, '0')}
    ok = got == want and pick is not None and pick.get('DupeScore') == DS_PICK
    return ('dupesearchgroup', ok, 'got=%s pick_score=%s' % (sorted(got.items()), pick and pick.get('DupeScore')))


def scenario_dupesearchresume(daemon, t):
    """A search that a crash cuts off resumes after the restart without searching
    or fetching again (grabs are scarce): the daemon is killed right after the
    two fast donors are added, while the full samples of the other two postings
    are still running. After the restart the saved nzb-files of those two are
    checked and added, the fast donors (in history now) are measured with the
    key's other duplicates, and the final scores are those of an uninterrupted
    run (dupesearchdonors without the dead posting): 90, 89, 85, 82."""
    n = 40
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(n)]
    postings = {
        'twin100': (ids('tw'), 400_000, 1, 11),
        'other100': (ids('ot'), 410_000, 1, 12),
        'other95': (ids('o5'), 420_000, 40, 13),
        'twin90': (ids('t9'), 400_000, 50, 14),
    }
    alive = set(ids('tw')) | set(ids('ot')) | set(ids('o5')[:38]) | set(ids('t9')[:36])
    daemon.fake_nntp.delays.update({'ot-': 0.3, 'o5-': 0.3})
    api = _ds_donor_env(daemon, t, postings, alive)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, '(fast)') < 2:
        time.sleep(0.2)
    time.sleep(0.5)
    t.procs[-1].kill()
    t.procs[-1].wait()
    killed_before_summary = _grep_log(t, ' added=') == 0
    grabs_before = sum(1 for r in daemon.newznab.requests if r['_path'].startswith('/getnzb/'))
    searches_before = sum(1 for r in daemon.newznab.requests if r.get('t') in ('search', 'tvsearch', 'movie'))
    daemon.fake_nntp.delays.clear()
    daemon.start()
    api = daemon.wait_ready()
    deadline = time.time() + 90
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    base = DS_PICK - 1000
    got = sorted((h.get('DupeScore') - base for h in api.history() if h.get('NZBName') == DS_TITLE), reverse=True)
    grabs_after = sum(1 for r in daemon.newznab.requests if r['_path'].startswith('/getnzb/')) - grabs_before
    searches_after = sum(1 for r in daemon.newznab.requests if r.get('t') in ('search', 'tvsearch', 'movie')) - searches_before
    resumed = _grep_log(t, 'resuming the search of')
    pending_left = t.exists(os.path.join('main', 'queue', 'dupesearch-pending', '1'))
    ok = (killed_before_summary and got == [90, 89, 85, 82] and grabs_after == 0 and searches_after == 0 and
          resumed == 1 and not pending_left)
    return ('dupesearchresume', ok, 'killed_before_summary=%s scores=%s grabs_after=%d searches_after=%d resumed=%d '
            'pending_left=%s' % (killed_before_summary, got, grabs_after, searches_after, resumed, pending_left))


def scenario_dupesearchfastdead(daemon, t):
    """A posting the quick probe finds alive but its full sample finds mostly
    gone (30% alive, below DupeMinAlive) was already queued as a fast donor: it
    is demoted to base + 1 and carries DupeAlive 30, the whole one stays at
    base + 89, and the dead one is remembered."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    postings = {'whole': (ids('wh'), 410_000, 1, 11), 'mostlygone': (ids('mg'), 420_000, 5, 12)}
    api = _ds_donor_env(daemon, t, postings, set(ids('wh')) | set(ids('mg')[:12]))
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    base = DS_PICK - 1000
    rows = sorted(((h.get('DupeScore'), {p['Name']: p['Value'] for p in h.get('Parameters', [])})
                   for h in api.history() if h.get('NZBName') == DS_TITLE), reverse=True)
    scores = [r[0] - base for r in rows]
    alive = [r[1].get('DupeAlive') for r in rows]
    dead_kept = False
    try:
        dead_kept = len(t.read_file(os.path.join('main', 'queue', 'dupesearch-dead')).strip()) > 0
    except Exception:
        pass
    summary = _grep_log(t, 'verified=2 added=2 rejected={dead: 1}')
    ok = scores == [89, 1] and alive == ['100', '30'] and dead_kept and summary == 1
    return ('dupesearchfastdead', ok, 'scores=%s alive=%s dead_kept=%s summary=%d' % (scores, alive, dead_kept, summary))


def scenario_dupesearchrescorefail(daemon, t):
    """A fast donor that left history before its full sample landed (the user
    deleted it) can't be rescored: the search says so (rescore: 1 in its
    summary, a warning) instead of claiming the new score."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    postings = {'slow': (ids('sl'), 410_000, 1, 11)}
    api = _ds_donor_env(daemon, t, postings, set(ids('sl')))
    daemon.fake_nntp.delays['sl-'] = 0.4
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, '(fast)') == 0:
        time.sleep(0.2)
    donors = [h for h in api.history() if h.get('NZBName') == DS_TITLE]
    for h in donors:
        api.editqueue('HistoryFinalDelete', '', [h['ID']])
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    summary = _grep_log(t, 'verified=1 added=1 rejected={rescore: 1}')
    warned = _grep_log(t, 'could not rescore')
    ok = len(donors) == 1 and summary == 1 and warned == 1
    return ('dupesearchrescorefail', ok, 'donors=%d summary=%d warned=%d' % (len(donors), summary, warned))


def scenario_dupesearchdryrun(daemon, t):
    """DupeSearchDryRun: the search runs and logs what it would do (four donors,
    ranks 90/89/85/82) but queues nothing and leaves the pick's key and score alone."""
    n = 40
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(n)]
    postings = {'twin100': (ids('tw'), 400_000, 1, 11), 'other100': (ids('ot'), 410_000, 1, 12),
                'other95': (ids('o5'), 420_000, 40, 13), 'twin90': (ids('t9'), 400_000, 50, 14)}
    alive = set(postings['twin100'][0]) | set(postings['other100'][0]) | set(postings['other95'][0][:38]) \
        | set(postings['twin90'][0][:36])
    api = _ds_donor_env(daemon, t, postings, alive)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    queued = [h for h in api.history() if h.get('NZBName') == DS_TITLE]
    groups = [g for g in api.listgroups() if g['NZBName'] == DS_TITLE]
    would = _grep_log(t, 'dry run, would add')
    summary = _grep_log(t, 'verified=4 added=4')
    pick_ok = len(groups) == 1 and groups[0]['MaxPostTime'] is not None and groups[0].get('DupeScore') == DS_PICK
    ok = not queued and would == 4 and summary == 1 and pick_ok
    return ('dupesearchdryrun', ok, 'queued=%d would=%d summary=%d pick_ok=%s' % (len(queued), would, summary, pick_ok))


def scenario_dupesearchdonor(daemon, t):
    """Items carrying the DupeAlive parameter (the nzbget-dupe-proxy marks its
    duplicates with it) or the DupeSearch parameter (what this search marks
    its own duplicates with) are never searched."""
    api = daemon.wait_ready()
    _ds_append(api, 'Alive.Show.S01E01.1080p.WEB.H264-GRP', _ds_nzb(t, 'a'), 'ds-key-a', 5_000_000,
               params=[{'Name': 'DupeAlive', 'Value': '80'}])
    _ds_append(api, 'Donor.Show.S01E01.1080p.WEB.H264-GRP', _ds_nzb(t, 'd'), 'ds-key-d', 5_000_000,
               params=[{'Name': 'DupeSearch', 'Value': 'donor'}])
    _ds_append(api, 'Plain.Show.S01E01.1080p.WEB.H264-GRP', _ds_nzb(t, 'n'), 'ds-key-n', 5_000_000)
    time.sleep(6)
    skipped = _grep_log(t, 'DupeSearch: searching duplicates of Alive.') + \
        _grep_log(t, 'DupeSearch: searching duplicates of Donor.')
    plain = _grep_log(t, 'DupeSearch: searching duplicates of Plain.')
    return ('dupesearchdonor', skipped == 0 and plain == 1,
            'marked_searched_logs=%d plain_searched_logs=%d' % (skipped, plain))


def scenario_dupesearchrestart(daemon, t):
    """A restart before the delay is over doesn't lose the pick (no add event
    fires for it after the restart: the queue is scanned once), and a restart
    after the search doesn't repeat it (the searched key is kept on disk)."""
    api = daemon.wait_ready()
    _ds_append(api, DS_TITLE, _ds_nzb(t, 'r'), DS_KEY, DS_PICK)
    try:
        api.shutdown()
    except Exception:
        pass
    time.sleep(3)
    daemon.start()
    api = daemon.wait_ready()
    time.sleep(8)
    first = _grep_log(t, 'DupeSearch: searching duplicates of %s ' % DS_TITLE)
    try:
        api.shutdown()
    except Exception:
        pass
    time.sleep(3)
    daemon.start()
    api = daemon.wait_ready()
    time.sleep(8)
    total = _grep_log(t, 'DupeSearch: searching duplicates of %s ' % DS_TITLE)
    return ('dupesearchrestart', first == 1 and total == 1,
            'searched_after_first_restart=%d searched_in_total=%d' % (first, total))


def _probe_fixture(t, tag, alive_every=0):
    """A six-file, 180-article dead posting (every article missing, or all but
    every ``alive_every``-th of each file) and a healthy lower-scored backup of
    the same title in another packing."""
    seg = 100_000
    vol = 3_000_000
    n = vol // seg
    dead_set = set(range(1, n + 1))
    if alive_every:
        dead_set = {i for i in dead_set if (i - 1) % alive_every != 3 % alive_every}
    primary = [('%sA/d%d.bin' % (tag, i), 'Dead%d.bin' % i, vol, seg, set(dead_set)) for i in range(6)]
    for m in primary:
        t.write_file(os.path.join('data', m[0]), _payload(vol, 9700))
    data = _payload(2_900_000, 9701)
    bp = _place_copy(t, '%sB' % tag, data)
    backup = build_nzb(bp, 'Backup.bin', 2_900_000, seg, set())
    return primary, backup, data


def scenario_deadpickprobe(daemon, t):
    """HealthCheck=dupe, a posting none of whose articles exists on the server
    and a healthy backup in history: the probe (STAT of 10 articles spread
    over the posting) finds nothing and abandons the download for the backup
    at once, not through the failed-article path (the news server answers
    after 500 ms, so the probe finishes before 32 articles failed)."""
    primary, backup, data = _probe_fixture(t, 'dp')
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, 'dp-key', 100)
    daemon.append(api, 'Backup', backup, False, 'dp-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups()
                                         if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary')
    deadline = time.time() + 240
    hb = daemon.wait_history(api, 'Backup')
    while hb['Status'].startswith('DELETED') and time.time() < deadline:
        time.sleep(0.5)
        hb = daemon.wait_history(api, 'Backup')
    probed = _grep_log(t, 'none of 10 sampled articles exists on any server')
    health_failover = _grep_log(t, 'Failing over Primary to duplicate Backup: health')
    integ = _verify_output(t, data)
    failed_articles = int(hp.get('FailedArticles', 0))
    return ('deadpickprobe', probed == 1 and health_failover == 0 and failed_articles < 32 and
            integ and hb['Status'].startswith('SUCCESS'),
            'status=%s backup_status=%s probe_logs=%d health_failover_logs=%d failed_articles=%d integrity=%s'
            % (hp['Status'], hb['Status'], probed, health_failover, failed_articles, integ))


def _deadpick_servers(daemon, t, tag):
    """A large dead posting (1800 articles, 15 files) with a healthy backup,
    several news servers answering after 200 ms (SCENARIO_EXTRA_SERVERS); the
    probe runs through the servers one after the other and finishes before the
    downloads have failed the 15% of the posting that make the regular health
    check abandon it."""
    seg, vol = 20_000, 2_400_000
    n = vol // seg
    payload = _payload(vol, 9800)
    primary = []
    for i in range(15):
        rel = '%sA/d%d.bin' % (tag, i)
        t.write_file(os.path.join('data', rel), payload)
        primary.append((rel, 'Dead%d.bin' % i, vol, seg, set(range(1, n + 1))))
    data = _payload(2_900_000, 9801)
    backup = build_nzb(_place_copy(t, '%sB' % tag, data), 'Backup.bin', 2_900_000, 100_000, set())
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, tag + '-key', 100)
    daemon.append(api, 'Backup', backup, False, tag + '-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups()
                                         if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary', timeout=300)
    deadline = time.time() + 300
    hb = daemon.wait_history(api, 'Backup', timeout=300)
    while hb['Status'].startswith('DELETED') and time.time() < deadline:
        time.sleep(0.5)
        hb = daemon.wait_history(api, 'Backup', timeout=300)
    return hp, hb, _verify_output(t, data)


def scenario_deadpickservers(daemon, t):
    """Six news servers, one of them unreachable (an optional server nothing
    listens on): five servers answer definitively, which is the minimum for a
    verdict, and the unreachable one is ignored, not counted as missing."""
    hp, hb, integ = _deadpick_servers(daemon, t, 'ds')
    probed = _grep_log(t, 'none of 10 sampled articles exists on any server')
    return ('deadpickservers', probed == 1 and integ and hb['Status'].startswith('SUCCESS'),
            'status=%s backup_status=%s probe_logs=%d failed_articles=%s integrity=%s'
            % (hp['Status'], hb['Status'], probed, hp.get('FailedArticles'), integ))


def scenario_deadpickfewservers(daemon, t):
    """Six news servers, two of them unreachable: only four answer
    definitively, below the minimum of five, so the probe gives no verdict and
    the regular health check handles the dead posting later."""
    hp, hb, integ = _deadpick_servers(daemon, t, 'df')
    probed = _grep_log(t, 'none of 10 sampled articles exists on any server')
    no_verdict = _grep_log(t, 'no verdict (4 of 6 servers answered definitively)')
    return ('deadpickfewservers', probed == 0 and no_verdict == 1 and integ and
            hb['Status'].startswith('SUCCESS'),
            'status=%s backup_status=%s probe_logs=%d no_verdict_logs=%d integrity=%s'
            % (hp['Status'], hb['Status'], probed, no_verdict, integ))


def scenario_deadpickpartial(daemon, t):
    """The probe never abandons a partly alive posting: 85% of the articles
    are missing but some exist, among them one the probe samples, and the
    backup's score is too low to be warranted by the real health. The download
    is not failed over."""
    primary, backup, data = _probe_fixture(t, 'dq', alive_every=7)
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, 'dq-key', 100)
    daemon.append(api, 'Backup', backup, False, 'dq-key', 1)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups()
                                         if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary')
    failed_over = _grep_log(t, 'Failing over Primary')
    alive_logs = _grep_log(t, 'an article exists, not abandoning it')
    return ('deadpickpartial', failed_over == 0 and alive_logs == 1,
            'status=%s failover_logs=%d alive_probe_logs=%d' % (hp['Status'], failed_over, alive_logs))


def scenario_notfound451(daemon, t):
    """A news server that answers 451 for a missing article (as some
    providers do) is treated like 430: the article is asked for once on that
    server. Before, 451 was an unknown error and the same server was asked
    again ArticleRetries times, for every missing article of every server."""
    size, seg = 2_500_000, 500_000
    data = _payload(size, 5300)
    pp = _place_copy(t, 'nfA', data, 'movie.mkv')
    api = daemon.wait_ready()
    daemon.append(api, 'RelNF', build_nzb(pp, 'movie.mkv', size, seg, {2, 3}), False, 'nf-key', 100)
    h = daemon.wait_history(api, 'RelNF', timeout=120)
    rewritten = daemon.proxy.rewritten if daemon.proxy else 0
    return ('notfound451', rewritten == 2 and int(h.get('FailedArticles', 0)) == 2,
            'status=%s rewritten_replies=%d failed_articles=%s (one request per missing article)'
            % (h['Status'], rewritten, h.get('FailedArticles')))


def scenario_dupehopeless(daemon, t):
    """HealthCheck=dupe without a backup: a dead posting (every article of
    every file missing, no duplicate able to supply anything) must be parked
    once the duplicates were asked for a sample and almost nothing of it
    exists, instead of failing all of its articles first. The one donor in
    history is an unrelated packaging that is itself dead, so nothing can
    be borrowed and no failover target qualifies either."""
    seg = 100_000
    vol_dead, vol_donor = 2_900_000, 3_000_000
    n = vol_dead // seg
    dead = [('hoA/d%d.bin' % i, 'Dead%d.bin' % i, vol_dead, seg, set(range(1, n + 1)))
            for i in range(6)]
    for m in dead:
        t.write_file(os.path.join('data', m[0]), _payload(vol_dead, 9600))
    dp = _place_copy(t, 'hoB', _payload(vol_donor, 9601))
    donor = build_nzb(dp, 'Donor.bin', vol_donor, seg, set(range(1, vol_donor // seg + 1)))
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(dead), True, 'ho-key', 100)
    daemon.append(api, 'Donor', donor, False, 'ho-key', 50)
    daemon.wait_history(api, 'Donor', timeout=60)
    # the donor is a dead posting too: mark it bad so it is no failover target
    api.editqueue('HistoryMarkBad', 0, '', [h['NZBID'] for h in api.history()
                                            if h['NZBName'] == 'Donor'])
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups()
                                         if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary')
    parked = _grep_log(t, 'Parking Primary: health')
    failed_over = _grep_log(t, 'Failing over Primary')
    failed_articles = int(hp.get('FailedArticles', 0))
    # 174 articles in total; the park fires once 32 were tried (more than a
    # tenth of the download, none of them alive)
    early = failed_articles < 140
    return ('dupehopeless', parked == 1 and failed_over == 0 and early,
            'status=%s parked_logs=%d failover_logs=%d failed_articles=%d'
            % (hp['Status'], parked, failed_over, failed_articles))


def scenario_dupehopelessretry(daemon, t):
    """"Retry failed articles" on a download parked as hopeless: the retry
    is checked like a new download and parked again as early as the first
    attempt while the posting is still dead. Before, the failure count of the
    first attempt was kept as the point of the last check and the retry's own
    count started below it, so the retry was checked only once it had failed
    that many articles again plus a sample (60 here, thousands after a late
    first check)."""
    seg = 100_000
    vol_dead = 2_900_000
    n = vol_dead // seg
    # one article exists: a download none of whose articles arrived has no
    # destination directory and can't be retried at all (upstream behavior)
    dead = [('hrA/d%d.bin' % i, 'Dead%d.bin' % i, vol_dead, seg,
             set(range(2 if i == 0 else 1, n + 1))) for i in range(6)]
    for m in dead:
        t.write_file(os.path.join('data', m[0]), _payload(vol_dead, 9610))
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(dead), False, 'hr-key', 100)
    hp = daemon.wait_history(api, 'Primary')
    first_failed = int(hp.get('FailedArticles', 0))
    api.editqueue('HistoryRetryFailed', 0, '', [hp['NZBID']])
    deadline = time.time() + 60
    while time.time() < deadline and any(h['NZBID'] == hp['NZBID'] for h in api.history()):
        time.sleep(0.2)
    hp = daemon.wait_history(api, 'Primary')
    parked = _grep_log(t, 'Parking Primary: health')
    retry_failed = int(hp.get('FailedArticles', 0))
    # 174 articles in total; each attempt is parked once a tenth was tried
    # and its health is below critical
    return ('dupehopelessretry', parked == 2 and first_failed < 140 and
            retry_failed <= first_failed + 8,
            'status=%s parked_logs=%d first_failed_articles=%d retry_failed_articles=%d'
            % (hp['Status'], parked, first_failed, retry_failed))


def scenario_dupedeadstart(daemon, t):
    """The hopeless-park must not fire on a posting that merely BEGINS with
    a dead stretch: the first 40 of 300 articles are missing, the rest
    exist, there is no par2 and no duplicate carries anything. Health is
    below critical with nothing downloaded yet, but by the time a tenth of
    the download was tried most tried articles exist, so the download runs
    to the end (FAILURE/HEALTH with exactly the 40 missing articles, every
    other article downloaded) and nothing is parked early."""
    seg = 100_000
    vol = 10_000_000
    data = _payload(vol, 9700)
    pp = _place_copy(t, 'dsA', data)
    primary = build_multi_nzb([(pp, 'Start.bin', vol, seg, set(range(1, 41))),
                               ('dsA/file.bin', 'Start2.bin', vol, seg, set(range(1, 41))),
                               ('dsA/file.bin', 'Start3.bin', vol, seg, set(range(1, 41)))])
    dp = _place_copy(t, 'dsB', _payload(3_000_000, 9701))
    donor = build_nzb(dp, 'Donor.bin', 3_000_000, seg, set(range(1, 31)))
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', primary, True, 'ds-key', 100)
    daemon.append(api, 'Donor', donor, False, 'ds-key', 50)
    daemon.wait_history(api, 'Donor', timeout=60)
    api.editqueue('HistoryMarkBad', 0, '', [h['NZBID'] for h in api.history()
                                            if h['NZBName'] == 'Donor'])
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups()
                                         if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary', timeout=300)
    parked = _grep_log(t, 'Parking Primary: health')
    failed_articles = int(hp.get('FailedArticles', 0))
    success_articles = int(hp.get('SuccessArticles', 0))
    return ('dupedeadstart', parked == 0 and failed_articles == 120 and success_articles == 180,
            'status=%s parked_logs=%d failed_articles=%d success_articles=%d'
            % (hp['Status'], parked, failed_articles, success_articles))


def _verify_output(t, expected, ext='.bin', dirs=(('main', 'dst'),)):
    """On SUCCESS the completed file lands at main/dst/<category>/<nzb>/
    <name><ext>, whose exact path depends on category and FileNaming. When
    the history status stays non-SUCCESS (e.g. a PARTIALLY-repaired release
    still has failed-byte statistics, so cleanup/move-to-dst is skipped and
    nzbget "parks" the item instead - see HistoryCoordinator's
    cleanupParkedFiles logic), the same file instead sits under
    main/inter/<nzb>.#<id>/<name><ext>. Walk the directories specified in
    'dirs' (by default, main/dst only; scenarios whose item ends parked,
    like xpackrar's header-holed volume, pass both main/dst and main/inter)
    and byte-compare every produced file with the given extension against
    the source payload."""
    for base in dirs:
        for rel in t.find_files(*base):
            if rel.endswith(ext):
                try:
                    if t.read_file(rel) == expected:
                        return True
                except Exception:
                    pass
    return False


def _grep_log(t, needle):
    try:
        return t.read_file('nzbget.log').decode(errors='replace').count(needle)
    except Exception:
        return 0


def _log_before(t, first, second):
    """True when the FIRST occurrence of ``first`` precedes the FIRST
    occurrence of ``second`` in the daemon log (both must occur). The log is
    strictly append-ordered, so this proves event ordering."""
    try:
        log = t.read_file('nzbget.log').decode(errors='replace')
    except Exception:
        return False
    first_at = log.find(first)
    second_at = log.find(second)
    return 0 <= first_at < second_at


SCENARIOS = {
    'complementary': scenario_complementary,
    'cutover': scenario_cutover,
    'leadswitch': scenario_leadswitch,
    'cutovertruth': scenario_cutovertruth,
    'manydonors': scenario_manydonors,
    'stream': scenario_stream,
    'streamtimeout': scenario_streamtimeout,
    'streamdeaddonor': scenario_streamdeaddonor,
    'streamslowprogress': scenario_streamslowprogress,
    'streamtooslow': scenario_streamtooslow,
    'liveoverlap': scenario_liveoverlap,
    'livegate': scenario_livegate,
    'livelastfile': scenario_livelastfile,
    'repost': scenario_repost,
    'repostrenamed': scenario_repostrenamed,
    'repostobfuscated': scenario_repostobfuscated,
    'repostdonorgaps': scenario_repostdonorgaps,
    'xpackbare': scenario_xpackbare,
    'xpackrar': scenario_xpackrar,
    'xpackrar2rar': scenario_xpackrar2rar,
    'xpack2sets': scenario_xpack2sets,
    'xpackzip': scenario_xpackzip,
    'xpack7z': scenario_xpack7z,
    'xpacksplit': scenario_xpacksplit,
    'xpackcompressed': scenario_xpackcompressed,
    'xpackneg': scenario_xpackneg,
    'xcrypt_encplain': scenario_xcrypt_encplain,
    'xcrypt_plainenc': scenario_xcrypt_plainenc,
    'xcrypt_diffpass': scenario_xcrypt_diffpass,
    'xcrypt_wrongpass': scenario_xcrypt_wrongpass,
    'xdecomp_zip': scenario_xdecomp_zip,
    'xdecomp_7z': scenario_xdecomp_7z,
    'xdecomp_storetarget': scenario_xdecomp_storetarget,
    'xdecomp_enc7z': scenario_xdecomp_enc7z,
    'xdecomp_enctarget': scenario_xdecomp_enctarget,
    'xdecomp_neg': scenario_xdecomp_neg,
    'xdecomp_symlink': scenario_xdecomp_symlink,
    'xdecomp_off': scenario_xdecomp_off,
    'wholefile': scenario_wholefile,
    'dupefailover': scenario_dupefailover,
    'dupehopeless': scenario_dupehopeless,
    'dupehopelessretry': scenario_dupehopelessretry,
    'dupedeadstart': scenario_dupedeadstart,
    'wholefileretry': scenario_wholefileretry,
    'dupehopelessnodupecheck': scenario_dupehopelessnodupecheck,
    'dupefailovernofallback': scenario_dupefailovernofallback,
    'wholefilepar': scenario_wholefilepar,
    'wholefilefailretry': scenario_wholefilefailretry,
    'wholefilelive': scenario_wholefilelive,
    'streamretry': scenario_streamretry,
    'wholefilerestart': scenario_wholefilerestart,
    'wholefilenfoproof': scenario_wholefilenfoproof,
    'wholefilesampleproof': scenario_wholefilesampleproof,
    'dupefailoverchain': scenario_dupefailoverchain,
    'xpacklatency': scenario_xpacklatency,
    'xpackflaky': scenario_xpackflaky,
    'deadpickprobe': scenario_deadpickprobe,
    'deadpickpartial': scenario_deadpickpartial,
    'dupesearchtrigger': scenario_dupesearchtrigger,
    'dupesearchkey': scenario_dupesearchkey,
    'dupesearchsearch': scenario_dupesearchsearch,
    'dupesearchfetch': scenario_dupesearchfetch,
    'dupesearchfilters': scenario_dupesearchfilters,
    'dupesearchdonors': scenario_dupesearchdonors,
    'dupesearchfastdead': scenario_dupesearchfastdead,
    'dupesearchgroup': scenario_dupesearchgroup,
    'dupesearchresume': scenario_dupesearchresume,
    'dupesearchdryrun': scenario_dupesearchdryrun,
    'dupesearchrescorefail': scenario_dupesearchrescorefail,
    'dupesearchfetcherror': scenario_dupesearchfetcherror,
    'dupesearchdonor': scenario_dupesearchdonor,
    'dupesearchrestart': scenario_dupesearchrestart,
    'deadpickservers': scenario_deadpickservers,
    'deadpickfewservers': scenario_deadpickfewservers,
    'notfound451': scenario_notfound451,
    'manualparnopar': scenario_manualparnopar,
    'xpackextensionless': scenario_xpackextensionless,
    'xpackextensionlessneg': scenario_xpackextensionlessneg,
    'xpackcorrupt': scenario_xpackcorrupt,
    'xpackendhole_nodirect': scenario_xpackendhole_nodirect,
    'xpackdeadserver': scenario_xpackdeadserver,
    'wholefileunicode': scenario_wholefileunicode,
    'wholefilepadding': scenario_wholefilepadding,
    'wholefileoldstyle': scenario_wholefileoldstyle,
    'wholefilecontinued': scenario_wholefilecontinued,
    'reloadpostqueue': scenario_reloadpostqueue,
    'wholefiletwosets': scenario_wholefiletwosets,
    'dupefailovernonzb': scenario_dupefailovernonzb,
    'wholefileproofcost': scenario_wholefileproofcost,
    'wholefile_nodirect': scenario_wholefile,
    'wholefileonly_nodirect': scenario_wholefileonly,
    'stream_nodirect': scenario_stream,
    'xpackbare_nodirect': scenario_xpackbare,
    'streamretry_nodirect': scenario_streamretry,
    'prodwholefile': scenario_prodwholefile,
    'prodstream': scenario_prodstream,
    'prodrarwhole': scenario_prodrarwhole,
    'prodrarstream': scenario_prodrarstream,
    'wholefileonly': scenario_wholefileonly,
    'wholefilewrongdonor': scenario_wholefilewrongdonor,
    'wholefilenofirst': scenario_wholefilenofirst,
    'wholefilepartial': scenario_wholefilepartial,
    'wholefiletwo': scenario_wholefiletwo,
    'failoverlive': scenario_failoverlive,
}

EXPECTED_HISTORY_STATUS = {
    'complementary': 'SUCCESS/HEALTH', 'cutover': 'SUCCESS/HEALTH',
    'leadswitch': 'SUCCESS/HEALTH', 'cutovertruth': 'SUCCESS/HEALTH',
    'manydonors': 'SUCCESS/HEALTH',
    'stream': 'SUCCESS/HEALTH', 'repost': 'FAILURE/PAR',
    'repostrenamed': 'SUCCESS/HEALTH', 'repostobfuscated': 'SUCCESS/HEALTH',
    'repostdonorgaps': 'SUCCESS/HEALTH',
    'xpackbare': 'SUCCESS/HEALTH',
    'xpackrar': 'FAILURE/HEALTH', 'xpackrar2rar': 'SUCCESS/HEALTH',
    'xpack2sets': 'SUCCESS/HEALTH',
    'xpackzip': 'SUCCESS/HEALTH', 'xpack7z': 'SUCCESS/HEALTH',
    'xpacksplit': 'SUCCESS/HEALTH', 'xpackcompressed': 'SUCCESS/HEALTH',
    'xpackneg': 'FAILURE/HEALTH', 'xcrypt_encplain': 'SUCCESS/HEALTH',
    'xcrypt_plainenc': 'SUCCESS/HEALTH', 'xcrypt_diffpass': 'SUCCESS/HEALTH',
    'xcrypt_wrongpass': 'FAILURE/HEALTH', 'xdecomp_zip': 'SUCCESS/HEALTH',
    'xdecomp_7z': 'SUCCESS/HEALTH', 'xdecomp_storetarget': 'SUCCESS/HEALTH',
    'xdecomp_enc7z': 'SUCCESS/HEALTH', 'xdecomp_neg': 'FAILURE/HEALTH',
    'xdecomp_symlink': 'FAILURE/HEALTH', 'xdecomp_off': 'FAILURE/HEALTH',
    'wholefile': 'SUCCESS/HEALTH',
    'dupefailover': 'FAILURE/HEALTH', 'dupehopeless': 'FAILURE/HEALTH',
    'dupedeadstart': 'FAILURE/HEALTH',
}

# per-scenario daemon options; the article-level scenarios keep the legacy
# "yes" spelling on purpose - it must still parse as "article". The stream
# scenario overrides the base config's ParCheck=manual: under "manual" the
# RequestParCheck the repair stage issues would only flag the item instead
# of running the par stage, leaving that handoff untested (with no par2
# files present, "auto" ends in a harmless "Nothing to par-check").
# the xdecomp_* daemon config pins SevenZipCmd at whatever real 7z binary
# generators._find_7z() discovered on PATH, so Unpack::MakeExtractor has a
# tool to shell out to; empty when none was found (the scenarios themselves
# SKIP in that case, see generators.HAVE_7Z).
_SEVENZIP_OPTION = ['SevenZipCmd=%s' % generators.SEVENZIP_PATH] if generators.HAVE_7Z else []

SCENARIO_OPTIONS = {
    'stream': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'streamdeaddonor': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'streamslowprogress': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DupeStreamTimeout=5'],
    'streamtooslow': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DupeStreamTimeout=5'],
    'streamtimeout': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DupeStreamTimeout=5'],
    # liveoverlap: the DownloadRate throttle (KB/s) keeps the big FileB
    # downloading long enough that FileA's live repair provably overlaps it
    'liveoverlap': ['DupeArticleFallback=live', 'ParCheck=auto', 'DownloadRate=8000'],
    'livegate': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'livelastfile': ['DupeArticleFallback=live', 'ParCheck=auto'],
    # repost: ParCheck=auto runs par-check against a random-bytes stand-in
    # par2; duplicate fallback repairs only archive data and leaves parity
    # untouched - FAILURE/PAR is the EXPECTED final status
    'repost': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # repostrenamed: no par2 members; "auto" ends in a harmless
    # "Nothing to par-check" after the repair handoff
    'repostrenamed': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'repostobfuscated': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'repostdonorgaps': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # xpackbare: no par2; "auto" ends in "Nothing to par-check" post-repair
    'xpackbare': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # xpack*: no real par2 anywhere; ParCheck=auto ends in "Nothing to par-check"
    'xpackrar': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackrar2rar': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpack2sets': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackzip': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpack7z': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpacksplit': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackcompressed': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackneg': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # xcrypt_*: M3 password-assisted cross-encryption; no real par2 anywhere,
    # ParCheck=auto ends in "Nothing to par-check" post-repair. Skipped
    # outright (no daemon options matter) when the cryptography module is
    # not installed - see generators.HAVE_CRYPTO.
    'xcrypt_encplain': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xcrypt_plainenc': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xcrypt_diffpass': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xcrypt_wrongpass': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # xdecomp_*: M4 decompression-assisted donor extraction. SevenZipCmd is
    # pinned at the locally-discovered 7z binary (generators.SEVENZIP_PATH,
    # via _SEVENZIP_OPTION below) so Unpack::MakeExtractor has a real tool to
    # shell out to; every xdecomp_* scenario SKIPs outright when 7z is not
    # installed (see generators.HAVE_7Z) before even reading this dict.
    # xdecomp_off is the deliberate exception: it OMITS
    # DupeStreamDecompress=yes to prove the opt-in gate, everything else
    # about its fixture matches xdecomp_7z.
    'xdecomp_zip': ['DupeArticleFallback=stream', 'DupeStreamDecompress=yes',
                    'ParCheck=auto'] + _SEVENZIP_OPTION,
    'xdecomp_7z': ['DupeArticleFallback=stream', 'DupeStreamDecompress=yes',
                   'ParCheck=auto'] + _SEVENZIP_OPTION,
    'xdecomp_storetarget': ['DupeArticleFallback=stream', 'DupeStreamDecompress=yes',
                            'ParCheck=auto'] + _SEVENZIP_OPTION,
    'xdecomp_enc7z': ['DupeArticleFallback=stream', 'DupeStreamDecompress=yes',
                      'ParCheck=auto'] + _SEVENZIP_OPTION,
    'xdecomp_enctarget': ['DupeArticleFallback=stream', 'DupeStreamDecompress=yes',
                          'ParCheck=auto'] + _SEVENZIP_OPTION,
    'xdecomp_neg': ['DupeArticleFallback=stream', 'DupeStreamDecompress=yes',
                    'ParCheck=auto'] + _SEVENZIP_OPTION,
    'xdecomp_symlink': ['DupeArticleFallback=stream', 'DupeStreamDecompress=yes',
                        'ParCheck=auto'] + _SEVENZIP_OPTION,
    'xdecomp_off': ['DupeArticleFallback=stream', 'ParCheck=auto'] + _SEVENZIP_OPTION,
    'wholefile': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # dupefailover: the health action under test replaces the base config's
    # HealthCheck=none; article recovery stays on so the failover's
    # "duplicates were asked first" sample gate is exercised too
    'dupefailover': ['DupeArticleFallback=article', 'HealthCheck=dupe'],
    'dupehopeless': ['DupeArticleFallback=article', 'HealthCheck=dupe'],
    'dupehopelessretry': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'dupedeadstart': ['DupeArticleFallback=article', 'HealthCheck=dupe'],
    'wholefileretry': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'dupehopelessnodupecheck': ['DupeArticleFallback=article', 'HealthCheck=dupe', 'DupeCheck=no'],
    'dupefailovernofallback': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'wholefilepar': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilefailretry': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilelive': ['DupeArticleFallback=live', 'ParCheck=auto', 'DownloadRate=4000'],
    'streamretry': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilerestart': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilenfoproof': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilesampleproof': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'dupefailoverchain': ['DupeArticleFallback=article', 'HealthCheck=dupe'],
    'xpacklatency': ['DupeArticleFallback=stream', 'ParCheck=auto', 'Server1.Connections=8'],
    'xpackflaky': ['DupeArticleFallback=stream', 'ParCheck=auto', 'Server1.Connections=4'],
    'deadpickprobe': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'deadpickpartial': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'dupesearchtrigger': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchUrl=http://127.0.0.1:9/api', 'DupeSearchDelay=2'],
    'dupesearchkey': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchUrl=http://127.0.0.1:9/api', 'DupeSearchDelay=2'],
    'dupesearchdonor': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchUrl=http://127.0.0.1:9/api', 'DupeSearchDelay=2'],
    'dupesearchsearch': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=1', 'DupeSearchApiKey=SECRETKEY123'],
    'dupesearchfetch': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=1', 'DupeSearchApiKey=k'],
    'dupesearchfetcherror': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=1', 'DupeSearchApiKey=k'],
    'dupesearchfilters': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k'],
    'dupesearchdonors': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchresume': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchgroup': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchfastdead': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchdryrun': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDryRun=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchrescorefail': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchrestart': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchUrl=http://127.0.0.1:9/api', 'DupeSearchDelay=2'],
    'deadpickservers': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'deadpickfewservers': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'notfound451': ['DupeArticleFallback=no', 'ArticleRetries=3', 'ArticleInterval=3'],
    'manualparnopar': ['DupeArticleFallback=stream', 'ParCheck=manual'],
    'xpackextensionless': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackextensionlessneg': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackcorrupt': ['DupeArticleFallback=stream', 'ParCheck=auto', 'CrcCheck=no'],
    'xpackendhole_nodirect': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DirectWrite=no'],
    # Server2 (preferred level 0, behind a FlakyNntpProxy) serves the download
    # and goes down for good at the first duplicate request; Server1 is the
    # level-1 backup every repair fetch then falls back to
    'xpackdeadserver': ['DupeArticleFallback=stream', 'ParCheck=auto', 'Server1.Connections=2',
                        'Server1.Level=1', 'Server2.Host=127.0.0.1', 'Server2.Connections=2',
                        'Server2.Level=0', 'Server2.Encryption=no'],
    'wholefileunicode': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilepadding': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefileoldstyle': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilecontinued': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'reloadpostqueue': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefiletwosets': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'dupefailovernonzb': ['DupeArticleFallback=article', 'HealthCheck=dupe'],
    'wholefileproofcost': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # the same paths when nzbget assembles files from temporary article files
    'wholefile_nodirect': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DirectWrite=no'],
    'wholefileonly_nodirect': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DirectWrite=no'],
    'stream_nodirect': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DirectWrite=no'],
    'xpackbare_nodirect': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DirectWrite=no'],
    'streamretry_nodirect': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DirectWrite=no'],
    'prodwholefile': PROD_OPTIONS,
    'prodstream': PROD_OPTIONS,
    'prodrarwhole': PROD_OPTIONS,
    'prodrarstream': PROD_OPTIONS,
    'wholefileonly': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilewrongdonor': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilenofirst': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilepartial': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefiletwo': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'failoverlive': ['DupeArticleFallback=live', 'HealthCheck=dupe', 'ParCheck=auto'],
}
DEFAULT_OPTIONS = ['DupeArticleFallback=yes']
# scenarios that read nserv's request log (nserv.log)
CAPTURE_REQUESTS = {'repost', 'wholefileproofcost'}
# extra nserv arguments per scenario (-w: response latency in ms)
SCENARIO_NSERV_ARGS = {'xpacklatency': ['-w', '1000'], 'deadpickprobe': ['-w', '500'],
                       'deadpickpartial': ['-w', '100'], 'deadpickservers': ['-w', '200'],
                       'deadpickfewservers': ['-w', '200']}

# extra news servers behind the same nserv: (servers answering, unreachable
# optional servers); Server1 counts among the answering ones
SCENARIO_EXTRA_SERVERS = {'deadpickservers': (5, 1), 'deadpickfewservers': (4, 2)}

# scenarios with a FlakyNntpProxy in front of a news server:
# (server number, message-id trigger, window in s or None for good)
SCENARIO_FLAKY_PROXY = {'xpackflaky': (1, 'xfB/', 4.0), 'xpackdeadserver': (2, 'xdB/', None)}

# scenarios with a CorruptingNntpProxy in front of Server1: message-id marker
SCENARIO_CORRUPT_PROXY = {'xpackcorrupt': 'xcB/'}

# scenarios with a RewritingNntpProxy in front of Server1: (old, new) reply bytes
SCENARIO_REWRITE_PROXY = {'notfound451': (b'430 ', b'451 ')}

# scenarios with a DelayingNntpProxy in front of Server1: [(message-id marker, delay in s)]
SCENARIO_DELAY_PROXY = {'streamtimeout': [(b'slowB/', 8.0)],
                        'streamslowprogress': [(b'slowA/', 2.0), (b'slowB/', 0.5)],
                        'streamtooslow': [(b'slowB/', 1.0)]}
# scenarios with a DelayingNntpProxy in front of Server1: (message-id marker, delay in s)
# scenarios with a FakeNewznab indexer (DupeSearchUrl points to it)
SCENARIO_NEWZNAB = {'dupesearchsearch', 'dupesearchfetch', 'dupesearchfetcherror', 'dupesearchfilters', 'dupesearchdonors',
                   'dupesearchfastdead', 'dupesearchrescorefail', 'dupesearchdryrun',
                   'dupesearchgroup', 'dupesearchresume'}

# scenarios with a FakeNntp news server in place of nserv
SCENARIO_FAKE_NNTP = {'dupesearchresume', 'dupesearchgroup', 'dupesearchdonors', 'dupesearchfastdead', 'dupesearchrescorefail', 'dupesearchdryrun'}


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #

def main():
    ap = argparse.ArgumentParser(description='DupeArticleFallback functional harness')
    ap.add_argument('--nzbget', required=True,
                    help='path to nzbget binary (local) or ON-DEVICE path (adb)')
    ap.add_argument('--target', choices=['local', 'adb'], default='local')
    ap.add_argument('--scenario', default='all',
                    choices=['all'] + list(SCENARIOS))
    ap.add_argument('--serial', help='adb device serial (adb target)')
    ap.add_argument('--keep', action='store_true', help='keep the workdir')
    args = ap.parse_args()

    scenarios = list(SCENARIOS) if args.scenario == 'all' else [args.scenario]
    results = []

    for name in scenarios:
        if args.target == 'adb' and (name in SCENARIO_FLAKY_PROXY or name in SCENARIO_CORRUPT_PROXY or
                                           name in SCENARIO_REWRITE_PROXY or name in SCENARIO_DELAY_PROXY):
            results.append((name, None, 'SKIP: the flaky news-server proxy runs on the host'))
            print('[SKIP] %s  (the flaky news-server proxy runs on the host)' % name)
            continue
        if args.target == 'adb' and name.startswith('xdecomp_'):
            results.append((name, None, 'SKIP: target-side archive extractor is not provisioned on Android'))
            print('[SKIP] %s  (target-side archive extractor is not provisioned on Android)' % name)
            continue
        nntp = free_port()
        rpc = free_port()
        while rpc == nntp:  # the two must not coincide
            rpc = free_port()
        stage = tempfile.mkdtemp(prefix='dupefallback-%s-' % name)
        if args.target == 'local':
            target = LocalTarget(args.nzbget, stage)
        else:
            target = AdbTarget(args.nzbget, stage, serial=args.serial)
        daemon = Daemon(target, nntp, rpc)
        try:
            flaky = SCENARIO_FLAKY_PROXY.get(name)
            corrupt = SCENARIO_CORRUPT_PROXY.get(name)
            rewrite = SCENARIO_REWRITE_PROXY.get(name)
            delay = SCENARIO_DELAY_PROXY.get(name)
            options = list(SCENARIO_OPTIONS.get(name, DEFAULT_OPTIONS))
            nserv_port = nntp
            proxy_port = None
            if flaky or corrupt or rewrite or delay:
                # Server1 always is nntp: a proxy for it listens there and
                # nserv moves to a port of its own; a proxy for Server2 gets a
                # port of its own
                server = flaky[0] if flaky else 1
                proxy_port = nntp if server == 1 else free_port()
                if server == 1:
                    nserv_port = free_port()
                else:
                    options.append('Server%d.Port=%d' % (server, proxy_port))
            if name in SCENARIO_EXTRA_SERVERS:
                live, dead = SCENARIO_EXTRA_SERVERS[name]
                number = 1
                for _ in range(live - 1):
                    number += 1
                    options += ['Server%d.Host=127.0.0.1' % number, 'Server%d.Port=%d' % (number, nserv_port),
                                'Server%d.Connections=2' % number, 'Server%d.Level=0' % number,
                                'Server%d.Encryption=no' % number]
                for _ in range(dead):
                    number += 1
                    options += ['Server%d.Host=127.0.0.1' % number, 'Server%d.Port=1' % number,
                                'Server%d.Connections=2' % number, 'Server%d.Level=0' % number,
                                'Server%d.Encryption=no' % number, 'Server%d.Optional=yes' % number]
            if name in SCENARIO_NEWZNAB:
                daemon.newznab = FakeNewznab()
                options.append('DupeSearchUrl=http://127.0.0.1:%d/api' % daemon.newznab.port)
            daemon.write_config(options)
            if name in SCENARIO_FAKE_NNTP:
                daemon.fake_nntp = FakeNntp(nserv_port)
            else:
                daemon.start_nserv(capture_requests=(name in CAPTURE_REQUESTS),
                                   extra_args=SCENARIO_NSERV_ARGS.get(name, ()), port=nserv_port)
            if flaky:
                daemon.proxy = FlakyNntpProxy(proxy_port, nserv_port, *flaky[1:])
            elif corrupt:
                daemon.proxy = CorruptingNntpProxy(proxy_port, nserv_port, corrupt)
            elif rewrite:
                daemon.proxy = RewritingNntpProxy(proxy_port, nserv_port, *rewrite)
            elif delay:
                daemon.proxy = DelayingNntpProxy(proxy_port, nserv_port, delay)
            time.sleep(1)
            daemon.start()
            if args.target == 'adb':
                target.forward_rpc(rpc)
            time.sleep(2)
            sc_name, passed, detail = SCENARIOS[name](daemon, target)
            expected_status = EXPECTED_HISTORY_STATUS.get(name)
            if passed is not None and expected_status and \
                    ('status=%s' % expected_status) not in detail:
                passed = False
                detail += ' expected_status=%s' % expected_status
            results.append((sc_name, passed, detail))
            label = 'SKIP' if passed is None else ('PASS' if passed else 'FAIL')
            print('[%s] %s  (%s)' % (label, sc_name, detail))
        except Exception as e:
            results.append((name, False, 'ERROR: %s' % e))
            print('[FAIL] %s  (ERROR: %s)' % (name, e))
        finally:
            if daemon.proxy:
                daemon.proxy.close()
            if daemon.newznab:
                daemon.newznab.close()
            if daemon.fake_nntp:
                daemon.fake_nntp.close()
            target.teardown(args.keep)

    # tri-state result: True/False are real pass/fail, None is a graceful SKIP
    # (e.g. an xcrypt_* scenario when the cryptography module is not
    # installed) - it counts toward neither the numerator nor the "attempted"
    # denominator, and never fails the run on its own.
    skipped = sum(1 for _, p, _ in results if p is None)
    attempted = len(results) - skipped
    passed_count = sum(1 for _, p, _ in results if p)
    failed = any(p is False for _, p, _ in results)
    suffix = ' (+%d skipped)' % skipped if skipped else ''
    print('\n=== %s / %s scenarios passed%s on target=%s ===' % (
        passed_count, attempted, suffix, args.target))
    if failed:
        return 1
    # A requested scenario that cannot run is an explicit, machine-readable
    # infrastructure result. Never turn missing crypto/7z into a green run.
    return 77 if skipped else 0


if __name__ == '__main__':
    sys.exit(main())
