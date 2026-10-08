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
import uuid
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


def build_nzb_with_par2(t, served_path, subject_name, data, seg_size, missing_parts):
    """build_nzb plus a par2 index of the file (no recovery slices) in the same
    collection: article borrowing needs par2 to check borrowed bytes against."""
    par = generators.par2_index([(subject_name, data)])
    par_path = served_path.rsplit('/', 1)[0] + '/rel.par2'
    t.write_file(os.path.join('data', par_path), par)
    return build_multi_nzb([(served_path, subject_name, len(data), seg_size, missing_parts),
                            (par_path, subject_name.rsplit('.', 1)[0] + '.par2', len(par), 500_000, set())])


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
        # (a scenario that sets ControlPassword sets creds "user:password" too)
        creds = getattr(self, 'creds', None)
        return ServerProxy('http://%s%s:%d/xmlrpc' % (creds + '@' if creds else '', host, self.rpc_port))

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
        self.accepted = 0
        self.closed = 0
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
            self.accepted += 1
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
            if not rewrite:
                self.closed += 1
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
        self.bare_ids = 0
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
                        arg = arg.strip()
                        mid = arg.strip('<>')
                        if (cmd == 'STAT' or cmd == 'BODY') and not (arg.startswith('<') and arg.endswith('>')):
                            # a real server takes a message-id only in angle brackets (RFC 3977 3.6)
                            with outer.lock:
                                outer.bare_ids += 1
                            self.wfile.write(b'501 message-id must be in angle brackets\r\n')
                        elif cmd == 'STAT' or cmd == 'BODY':
                            for prefix, delay in list(outer.delays.items()):
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


class RejectingNntp:
    """A news server that refuses every login for good (502), as a server with a
    broken or expired account does."""

    def __init__(self):
        self.logins = 0
        self.srv = socket.socket()
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind(('127.0.0.1', 0))
        self.srv.listen(64)
        self.port = self.srv.getsockname()[1]
        threading.Thread(target=self._serve, daemon=True).start()

    def _serve(self):
        while True:
            try:
                conn, _ = self.srv.accept()
            except OSError:
                return
            threading.Thread(target=self._client, args=(conn,), daemon=True).start()

    def _client(self, conn):
        try:
            conn.sendall(b'200 reject server ready\r\n')
            data = b''
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                data += chunk
                while b'\r\n' in data:
                    line, data = data.split(b'\r\n', 1)
                    if line.upper().startswith(b'AUTHINFO USER'):
                        conn.sendall(b'381 password required\r\n')
                    elif line.upper().startswith(b'AUTHINFO PASS'):
                        self.logins += 1
                        conn.sendall(b'502 Authentication Failed\r\n')
                    else:
                        conn.sendall(b'480 authentication required\r\n')
        except OSError:
            pass
        finally:
            conn.close()

    def close(self):
        self.srv.close()


class FakeNewznab:
    """A Newznab indexer for the duplicate search: an HTTP server whose
    ``respond(params, path)`` returns (status, body bytes); every request is
    recorded in ``requests`` (the query parameters, apikey included)."""

    def __init__(self):
        outer = self
        self.requests = []
        self.lock = threading.Lock()
        self.respond = lambda params, path: (200, newznab_xml([]))
        # answer as NZBHydra2 does when asked for gzip: compressed, no length, the
        # end marked by closing the connection
        self.gzip = False

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                parts = urllib.parse.urlsplit(self.path)
                params = {k: v[0] for k, v in urllib.parse.parse_qs(parts.query).items()}
                with outer.lock:
                    outer.requests.append(dict(params, _path=parts.path, _time=time.time()))
                status, body = outer.respond(params, parts.path)
                if status is None:
                    # the connection drops without an answer
                    self.close_connection = True
                    return
                self.send_response(status)
                self.send_header('Content-Type', 'application/xml')
                if outer.gzip and 'gzip' in self.headers.get('Accept-Encoding', ''):
                    import gzip as _gzip
                    body = _gzip.compress(body)
                    self.send_header('Content-Encoding', 'gzip')
                    self.close_connection = True
                else:
                    self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *args):
                pass

        # nzbget asks its queries at once: the default accept queue (5) dropped one
        # under load ("connection reset by peer") and its results with it
        class Server(ThreadingHTTPServer):
            request_queue_size = 64
            daemon_threads = True
        self.server = Server(('127.0.0.1', 0), Handler)
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


class FirstFailNntpProxy(DelayingNntpProxy):
    """A news server in front of nserv that answers the FIRST body request for
    each message-id naming one of ``markers`` itself, with a transient error
    ("503"), and passes every other request: articles a single failed request
    loses (ArticleRetries=0) though they exist on the server."""

    def __init__(self, listen_port, upstream_port, markers):
        self.markers = markers
        self.seen = set()
        self.failed = 0
        super().__init__(listen_port, upstream_port, [])

    def _pipe(self, src, dst, rewrite):
        try:
            while True:
                data = src.recv(65536)
                if not data:
                    break
                if not rewrite:
                    found = re.findall(rb'(?:BODY|ARTICLE) <([^>]+)>', data)
                    first = [m for m in found if any(k in m for k in self.markers) and m not in self.seen]
                    if first and len(found) == 1:
                        self.seen.add(first[0])
                        self.failed += 1
                        src.sendall(b'503 temporarily unavailable\r\n')
                        continue
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
    primary = build_nzb_with_par2(t, pp, 'ReleaseA.bin', data, seg, {3, 5, 7})
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


def scenario_rejectnextserver(daemon, t):
    """B6: Server1 serves every article with a malformed yEnc part header (a
    proxy turns "=ypart begin=" into "=ypart begxn="), Server2 serves them
    intact. A rejected article is the server's fault, not the article's: the
    next server is asked, and the download completes byte-identically without
    a duplicate."""
    size, seg = 1_000_000, 100_000
    data = _payload(size, 9900)
    pp = _place_copy(t, 'rejA', data, 'file.bin')
    api = daemon.wait_ready()
    daemon.append(api, 'RelRej', build_nzb(pp, 'Rej.bin', size, seg, set()), False, 'rej-key', 100)
    h = daemon.wait_history(api, 'RelRej')
    integ = _verify_output(t, data)
    rejected = _grep_log(t, 'Malformed article') + _grep_log(t, 'could not be decoded')
    ok = 'SUCCESS' in h['Status'] and integ and daemon.proxy.rewritten > 0
    return ('rejectnextserver', ok, 'status=%s integrity=%s rewritten=%d rejected_logs=%d'
            % (h['Status'], integ, daemon.proxy.rewritten, rejected))


def scenario_nzbgapborrow(daemon, t):
    """B30 and B3: the release's nzb-file skips part 4 (an indexer that didn't
    capture it) and part 5 is missing on the server; a twin duplicate lists
    every part. The twin pairs by part number despite the gap (B30), and part 5
    is borrowed: its expected place isn't taken from part 3, its list neighbour
    but not the previous part (B3). Part 4, in no nzb-file of the release, stays
    missing."""
    size, seg = 1_000_000, 100_000
    data = _payload(size, 9800)
    pp = _place_copy(t, 'gapA', data, 'file.bin')
    dp = _place_copy(t, 'gapB', data, 'file.bin')
    primary = build_nzb_with_par2(t, pp, 'Gap.bin', data, seg, {5})
    primary = re.sub(r'<segment[^>]*number="4"[^>]*>[^<]*</segment>', '', primary)
    donor = build_nzb(dp, 'Gap.bin', size, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonGap', donor, True, 'gap-key', 50)
    daemon.append(api, 'RelGap', primary, False, 'gap-key', 100)
    h = daemon.wait_history(api, 'RelGap')
    recovered = int(h.get('DupeRecoveredArticles', 0))
    unmatched = _grep_log(t, 'none with a matching file')
    mismatch = _grep_log(t, 'offset mismatch') + _grep_log(t, 'end mismatch')
    ok = recovered >= 1 and unmatched == 0 and mismatch == 0
    return ('nzbgapborrow', ok, 'status=%s recovered=%d unmatched_logs=%d mismatch_logs=%d'
            % (h['Status'], recovered, unmatched, mismatch))


def scenario_declaredcountnopair(daemon, t):
    """B39: a release file of 29 parts (its subject says 1/29) and a duplicate's
    file of 30 parts with the same article size, other content, both missing
    parts. The release lists every part it declares, so the 30-part file is
    another one: nothing is borrowed from it (it used to be paired - one
    trailing extra part was allowed for an nzb-file that lost its last one -
    and its bytes written into the release)."""
    seg = 100_000
    rel = _payload(2_900_000, 9810)
    don = _payload(3_000_000, 9811)
    rp = _place_copy(t, 'dcA', rel, 'rel.bin')
    dp = _place_copy(t, 'dcB', don, 'don.bin')
    primary = build_nzb(rp, 'Rel.bin', len(rel), seg, {5, 6, 20})
    donor = build_nzb(dp, 'Don.bin', len(don), seg, {9})
    api = daemon.wait_ready()
    daemon.append(api, 'DonCount', donor, True, 'dc-key', 50)
    daemon.append(api, 'RelCount', primary, False, 'dc-key', 100)
    h = daemon.wait_history(api, 'RelCount')
    recovered = int(h.get('DupeRecoveredArticles', 0))
    borrowed = _grep_log(t, 'from duplicate collections')
    ok = recovered == 0 and borrowed == 0 and int(h.get('FailedArticles', 0)) == 3
    return ('declaredcountnopair', ok, 'status=%s recovered=%d recovered_logs=%d failed_articles=%s'
            % (h['Status'], recovered, borrowed, h.get('FailedArticles')))


def _final_delete_midway(daemon, t, name, restart_at_once):
    """A client cancels a download that is half done (nzbdavkodi's
    cancel_jobs: GroupFinalDelete, then GroupFinalDelete and HistoryFinalDelete
    again a moment later) while a lower-scored duplicate waits in history and
    articles are being borrowed from it. The download must be gone for good:
    not left in the queue without files (stuck QUEUED, never post-processed),
    not in history, and not back after a restart."""
    seg = 250_000
    size = 3_000_000
    members, backup = [], []
    for i in range(6):
        data = _payload(size, 9830 + i)
        pp = _place_copy(t, 'fdA%d' % i, data, 'f%d.bin' % i)
        bp = _place_copy(t, 'fdB%d' % i, data, 'f%d.bin' % i)
        members.append((pp, 'Movie.part%02d.rar' % (i + 1), size, seg, {3, 7}))
        backup.append((bp, 'Movie.part%02d.rar' % (i + 1), size, seg, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(members), True, 'fd-key', 100)
    daemon.append(api, 'Backup', build_multi_nzb(backup), False, 'fd-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    pid = [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'][0]
    api.editqueue('GroupResume', 0, '', [pid])
    deadline = time.time() + 120
    while time.time() < deadline:
        g = [x for x in api.listgroups() if x['NZBID'] == pid]
        if not g or g[0]['RemainingFileCount'] <= 4:
            break
        time.sleep(0.1)
    cancelled_with = g[0]['RemainingFileCount'] if g else -1
    api.editqueue('GroupFinalDelete', 0, '', [pid])
    if not restart_at_once:
        time.sleep(0.3)
        api.editqueue('GroupFinalDelete', 0, '', [pid])
        api.editqueue('HistoryFinalDelete', 0, '', [pid])
        time.sleep(8)

    def present():
        queued = [x for x in api.listgroups() if x['NZBID'] == pid]
        hist = [h for h in api.history(True) if h.get('NZBID') == pid or h.get('ID') == pid]
        return queued, hist
    queued, hist = present() if not restart_at_once else ([], [])
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    time.sleep(3)
    queued2, hist2 = present()
    ok = cancelled_with > 0 and not queued and not hist and not queued2 and not hist2
    return (name, ok, 'remaining_files_at_cancel=%s queued=%s history=%d after_restart_queued=%s history=%d'
            % (cancelled_with, [(x['Status'], x['RemainingFileCount']) for x in queued], len(hist),
               [(x['Status'], x['RemainingFileCount']) for x in queued2], len(hist2)))


def scenario_finaldeletemidway(daemon, t):
    """B42: a client cancels a half-done download (nzbdavkodi's cancel_jobs:
    GroupFinalDelete, then again with HistoryFinalDelete a moment later) while
    articles are borrowed from a duplicate in history: it is gone for good,
    before and after a restart."""
    return _final_delete_midway(daemon, t, 'finaldeletemidway', False)


def scenario_finaldeleterestart(daemon, t):
    """B42: nzbget shuts down right after a half-done download was final-deleted,
    before the files still downloading let the delete finish. The download
    must not come back after the restart as a QUEUED item without files
    (production 2425: never post-processed, never removed)."""
    return _final_delete_midway(daemon, t, 'finaldeleterestart', True)


def scenario_restartmidway(daemon, t):
    """B42: nzbget shuts down while a download is half done, articles are
    being borrowed from a duplicate in history (HealthCheck=dupe,
    DupeArticleFallback=live). After the restart the download keeps the files
    it hadn't finished and completes: it isn't left QUEUED without files
    (then nothing ever starts its post-processing)."""
    seg = 100_000
    size = 6_000_000
    members, backup, payloads = [], [], []
    for i in range(6):
        data = _payload(size, 9840 + i)
        payloads.append(data)
        pp = _place_copy(t, 'rmA%d' % i, data, 'f%d.bin' % i)
        bp = _place_copy(t, 'rmB%d' % i, data, 'f%d.bin' % i)
        members.append((pp, 'Movie.part%02d.rar' % (i + 1), size, seg, {3, 7}))
        backup.append((bp, 'Movie.part%02d.rar' % (i + 1), size, seg, set()))
    # a par2 index: articles are borrowed only for a collection par2 can check
    par = generators.par2_index([('Movie.part%02d.rar' % (i + 1), payloads[i]) for i in range(6)])
    t.write_file(os.path.join('data', 'rmA0/rel.par2'), par)
    members.append(('rmA0/rel.par2', 'Movie.par2', len(par), 500_000, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(members), True, 'rm-key', 100)
    daemon.append(api, 'Backup', build_multi_nzb(backup), False, 'rm-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    pid = [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'][0]
    api.editqueue('GroupResume', 0, '', [pid])
    deadline = time.time() + 120
    g = []
    while time.time() < deadline:
        g = [x for x in api.listgroups() if x['NZBID'] == pid]
        if not g or g[0]['RemainingFileCount'] <= 4:
            break
        time.sleep(0.1)
    before = g[0]['RemainingFileCount'] if g else -1
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    time.sleep(1)
    g = [x for x in api.listgroups() if x['NZBID'] == pid]
    after = g[0]['RemainingFileCount'] if g else -1
    files_after = len(api.listfiles(0, 0, pid)) if g else -1
    h = daemon.wait_history(api, 'Primary', timeout=180)
    # completed: whole by borrowing, or (its par2 index has no recovery data, so the
    # articles that failed before the par-first wait was lifted stay missing) by
    # failing over to the whole backup
    deadline = time.time() + 120
    backup = {}
    while time.time() < deadline and not h['Status'].startswith('SUCCESS'):
        backup = next((x for x in api.history() if x['NZBName'] == 'Backup'), {})
        if backup.get('Status', '').startswith('SUCCESS'):
            break
        time.sleep(0.5)
    completed = h['Status'].startswith('SUCCESS') or backup.get('Status', '').startswith('SUCCESS')
    # (each file takes seconds: at most one finishes before the count is taken)
    ok = before > 1 and after >= before - 1 and files_after == after and completed
    return ('restartmidway', ok, 'remaining_before=%d remaining_after=%d listfiles_after=%d status=%s backup=%s'
            % (before, after, files_after, h['Status'], backup.get('Status')))


def scenario_sidefilenorepair(daemon, t):
    """B53: the data file is whole and only the .nfo's article is missing, from
    the duplicate too. No repair from the duplicate is queued for
    a side file (it held up Industry S04E06 4155, whose par2 had verified the
    video): the download completes without one."""
    data = _payload(2_000_000, 9860)
    nfo = _payload(3_000, 9861)
    dp = _place_copy(t, 'sfA', data, 'movie.mkv')
    np_ = _place_copy(t, 'sfN', nfo, 'movie.nfo')
    db = _place_copy(t, 'sfB', data, 'movie.mkv')
    nb = _place_copy(t, 'sfM', nfo, 'movie.nfo')
    primary = build_multi_nzb([(dp, 'Movie.mkv', len(data), 100_000, set()), (np_, 'Movie.nfo', len(nfo), 3_000, {1})])
    # the duplicates lack the .nfo too (as in production): only a repair could bring it
    donor = build_multi_nzb([(db, 'Movie.mkv', len(data), 100_000, set()), (nb, 'Movie.nfo', len(nfo), 3_000, {1})])
    api = daemon.wait_ready()
    daemon.append(api, 'Donor', donor, True, 'sf-key', 50)
    daemon.append(api, 'Release', primary, False, 'sf-key', 100)
    h = daemon.wait_history(api, 'Release')
    queued = _grep_log(t, 'Queueing stream repair of Movie.nfo')
    skipped = _grep_log(t, 'Not repairing side file Movie.nfo')
    # (no par2 here: the missing .nfo leaves the download FAILURE/HEALTH, as without the fix)
    ok = queued == 0 and skipped == 1
    return ('sidefilenorepair', ok, 'status=%s nfo_repairs_queued=%d skipped_logs=%d' % (h['Status'], queued, skipped))


def scenario_recheckfailed(daemon, t):
    """Lesson 18: with ArticleRetries=0, three articles are lost to a single
    failed request each (the server answers their first request with a
    transient error) though they exist on the server. The release fails; its failed articles are
    then asked of the server (STAT), all three exist, and they are retried once:
    the release completes byte-identically."""
    size, seg = 2_000_000, 100_000
    data = _payload(size, 9950)
    pp = _place_copy(t, 'rcA', data, 'file.bin')
    nzb = build_nzb(pp, 'Rc.bin', size, seg, set())
    # the proxy's markers pick articles 5, 10 and 15
    api = daemon.wait_ready()
    daemon.append(api, 'RelRc', nzb, False, 'rc-key', 100)
    first = daemon.wait_history(api, 'RelRc')
    deadline = time.time() + 120
    final = first
    while time.time() < deadline:
        h = [x for x in api.history() if x['NZBName'] == 'RelRc']
        if h and 'SUCCESS' in h[0]['Status']:
            final = h[0]
            break
        time.sleep(1)
    rechecked = _grep_log(t, '3 of 3 failed articles exist on the servers; retrying them')
    integ = _verify_output(t, data)
    # (the retry can be over before the first look at the history: the log tells)
    ok = rechecked == 1 and daemon.proxy.failed == 3 and 'SUCCESS' in final['Status'] and integ
    return ('recheckfailed', ok, 'first=%s rechecked_logs=%d final=%s integrity=%s failed_requests=%d'
            % (first['Status'], rechecked, final['Status'], integ, daemon.proxy.failed))


def scenario_cutover(daemon, t):
    """Primary missing 10 of 20 articles => file cuts over to the duplicate."""
    size, seg = 10_000_000, 500_000
    data = _payload(size, 1206)
    pp = _place_copy(t, 'cutA', data)
    dp = _place_copy(t, 'cutB', data)
    primary = build_nzb_with_par2(t, pp, 'CutA.bin', data, seg, set(range(2, 12)))
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
    # the summary's "of N missing" counts only articles that failed: the ones the
    # duplicate served first after the cutover were never missing (it read
    # "of 19 missing" for a file missing 10)
    with open(t.path('nzbget.log'), errors='replace') as f:
        found = re.findall(r'Recovered (\d+) of (\d+) missing article\(s\) of CutA', f.read())
    attempted = int(found[-1][1]) if found else -1
    return ('cutover', ok and integ and 3 <= recov <= 10 and cut == 1 and 0 < attempted <= 10,
            'status=%s recovered=%d attempted=%d cutover_logs=%d integrity=%s'
            % (h['Status'], recov, attempted, cut, integ))


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
    primary = build_nzb_with_par2(t, pp, 'LeadA.bin', data, seg, holes)
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
    primary = build_nzb_with_par2(t, pp, 'CtrA.bin', data, seg, set(range(2, 13)))       # 11 missing
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
    primary = build_nzb_with_par2(t, pp, 'ManyPrim.bin', data, seg, {2, 3, 4})
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


def scenario_articledecoy(daemon, t):
    """Article borrowing from a duplicate of the same size and article layout but
    other bytes (a different encode), no par2 to check against: the download must
    not end SUCCESS with the decoy's bytes in its file."""
    size, seg = 5_000_000, 500_000
    data = _payload(size, 6100)
    decoy = _payload(size, 6101)
    pp = _place_copy(t, 'adA', data)
    dp = _place_copy(t, 'adB', decoy)
    primary = build_nzb(pp, 'Decoy.bin', size, seg, {3, 7})
    donor = build_nzb(dp, 'Decoy.bin', size, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonAD', donor, True, 'ad-key', 50)
    daemon.append(api, 'RelAD', primary, False, 'ad-key', 100)
    h = daemon.wait_history(api, 'RelAD')
    integ = _verify_output(t, data)
    wrong_success = h['Status'].startswith('SUCCESS') and not integ
    return ('articledecoy', not wrong_success, 'status=%s integrity=%s recovered=%s' % (
        h['Status'], integ, h.get('DupeRecoveredArticles')))


def scenario_articledecoypar(daemon, t):
    """The decoy of articledecoy (same size and article layout, other bytes), the
    collection with a par2 index this time: articles are borrowed, but their
    bytes fail the par2 checksums, so the download doesn't end SUCCESS with them."""
    size, seg = 5_000_000, 500_000
    data = _payload(size, 6200)
    decoy = _payload(size, 6201)
    pp = _place_copy(t, 'apA', data)
    dp = _place_copy(t, 'apB', decoy)
    primary = build_nzb_with_par2(t, pp, 'DecoyPar.bin', data, seg, {3, 7})
    donor = build_nzb(dp, 'DecoyPar.bin', size, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonAP', donor, True, 'ap-key', 50)
    daemon.append(api, 'RelAP', primary, False, 'ap-key', 100)
    h = daemon.wait_history(api, 'RelAP')
    integ = _verify_output(t, data)
    wrong_success = h['Status'].startswith('SUCCESS') and not integ
    return ('articledecoypar', not wrong_success, 'status=%s integrity=%s recovered=%s rejected_logs=%d' % (
        h['Status'], integ, h.get('DupeRecoveredArticles'), _grep_log(t, "don't match its par2 checksums")))


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


def scenario_streamrestartcredit(daemon, t):
    """A stream repair interrupted by a shutdown after it fully repaired one file
    (the other's duplicate is slow): after the restart the other file is repaired
    too, and the release ends SUCCESS. The fully repaired file's failed size was
    never credited back to health (its job was dropped as done), so the release
    stayed short of full health though its files were complete."""
    size, seg_primary, seg_donor = 2_000_000, 500_000, 300_000
    d1, d2 = _payload(size, 7676), _payload(size, 7677)
    p1 = _place_copy(t, 'srA', d1, 'f1.mkv')
    p2 = _place_copy(t, 'srC', d2, 'f2.mkv')
    b1 = _place_copy(t, 'srB', d1, 'f1.mkv')
    b2 = _place_copy(t, 'srSlow', d2, 'f2.mkv')
    primary = build_multi_nzb([(p1, 'Sr1.mkv', size, seg_primary, {2}), (p2, 'Sr2.mkv', size, seg_primary, {3})])
    donor = build_multi_nzb([(b1, 'obf-sr1.mkv', size, seg_donor, set()), (b2, 'obf-sr2.mkv', size, seg_donor, set())])
    api = daemon.wait_ready()
    daemon.append(api, 'DonSR', donor, True, 'sr2-key', 50)
    daemon.append(api, 'RelSR2', primary, False, 'sr2-key', 100)
    deadline = time.time() + 120
    while time.time() < deadline and _grep_log(t, 'of Sr1.mkv from duplicate') == 0:
        time.sleep(0.3)
    first_repaired = _grep_log(t, 'of Sr1.mkv from duplicate')
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    h = daemon.wait_history(api, 'RelSR2', timeout=240)
    integ = _verify_output(t, d1, '.mkv', dirs=(('main', 'dst'), ('main', 'inter'))) and \
        _verify_output(t, d2, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    interrupted = _grep_log(t, 'Stream repair interrupted')
    ok = first_repaired >= 1 and interrupted >= 1 and h['Status'].startswith('SUCCESS') and h['Health'] == 1000 and integ
    return ('streamrestartcredit', ok, 'first_repaired=%d interrupted=%d status=%s health=%s integrity=%s' % (
        first_repaired, interrupted, h['Status'], h['Health'], integ))


def scenario_truncatedstate(daemon, t):
    """A file's partial state (<id>s, written in place) is cut short, as a power
    loss leaves it: the download still completes, the file downloaded again. The
    load left blank articles behind (they downloaded nothing, the file failed)
    and stopped reading the remaining states."""
    size, seg = 3_000_000, 100_000
    data = _payload(size, 7979)
    pp = _place_copy(t, 'tsA', data, 'f.bin')
    api = daemon.wait_ready()
    daemon.append(api, 'RelTS', build_nzb(pp, 'ts.bin', size, seg, set()), False, 'ts-key', 100)
    queue_dir = t.path('main', 'queue')
    deadline = time.time() + 60
    state = None
    while time.time() < deadline and not state:
        state = next((f for f in os.listdir(queue_dir) if re.fullmatch(r'\d+s', f)), None)
        time.sleep(0.2)
    t.procs[-1].kill()
    t.procs[-1].wait(timeout=30)
    if state:
        path = os.path.join(queue_dir, state)
        content = open(path, 'rb').read()
        open(path, 'wb').write(content[:len(content) // 2])
    daemon.start()
    api = daemon.wait_ready()
    h = daemon.wait_history(api, 'RelTS', timeout=180)
    integ = _verify_output(t, data)
    ok = bool(state) and h['Status'].startswith('SUCCESS') and integ
    return ('truncatedstate', ok, 'state_file=%s status=%s integrity=%s discarded_logs=%d' % (
        state, h['Status'], integ, _grep_log(t, 'Discarding damaged download state')))


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
    nzbget an average speed to compare with; it is slowed to take a few
    seconds, since nzbget measures download time in whole seconds and a
    download under one second leaves the speed unknown.)"""
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


def scenario_streammaxrun(daemon, t):
    """Whatever its progress, a stream repair stops once it ran 5 times
    DupeStreamTimeout (Industry S04E02: a repair that never ended). The
    duplicate delivers every 1 s, under the 8 s without progress, and the
    download is slow too (2 s a request), so the repair isn't 3 times slower
    than downloading either: only the 40 s cap stops it, with 62 MB to
    recover. The download then ends as a failure (no par2 here), as any
    download whose repair failed, instead of sitting in stream repair."""
    size, seg_primary, seg_donor = 64_000_000, 500_000, 250_000
    data = _payload(size, 4343)
    pp = _place_copy(t, 'capA', data, 'file.mkv')
    dp = _place_copy(t, 'capB', data, 'file.mkv')
    primary = build_nzb(pp, 'CapA.mkv', size, seg_primary, set(range(2, 126)))
    donor = build_nzb(dp, 'obf-cap.mkv', size, seg_donor, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonCap', donor, True, 'cap-key', 50)
    daemon.append(api, 'CapA', primary, False, 'cap-key', 100)
    start = time.time()
    h = daemon.wait_history(api, 'CapA', timeout=240)
    took = time.time() - start
    capped = _grep_log(t, 'still running after 40 seconds, 5 times the timeout (option DupeStreamTimeout)')
    other = _grep_log(t, 'nothing recovered for') + _grep_log(t, 'times a download at')
    ok = capped == 1 and other == 0 and 'SUCCESS' not in h['Status'] and took < 120
    return ('streammaxrun', ok, 'status=%s took=%.0fs capped_logs=%d other_stop_logs=%d delayed=%d'
            % (h['Status'], took, capped, other, daemon.proxy.delayed))


def scenario_streamstarved(daemon, t):
    """B76 (Las Azules S02E06 4440): the stream repair of a damaged download
    competes with the next download for the server's 2 connections, which a
    slow download holds (2 s a request) for about 40 s. Waiting for a
    connection isn't a lack of progress: the repair waits, then repairs the
    file from the whole duplicate. Before, each fetch gave up after waiting,
    and the repair stopped with nothing recovered after DupeStreamTimeout."""
    size, seg_primary, seg_donor = 6_000_000, 500_000, 250_000
    data = _payload(size, 7676)
    primary = build_nzb(_place_copy(t, 'stA', data, 'file.mkv'), 'StarvA.mkv', size, seg_primary, set(range(2, 6)))
    donor = build_nzb(_place_copy(t, 'stB', data, 'file.mkv'), 'obf-starv.mkv', size, seg_donor, set())
    busy_size = 4_000_000
    busy = build_nzb(_place_copy(t, 'busy', _payload(busy_size, 7677)), 'Busy.bin', busy_size, 100_000, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonStarv', donor, True, 'starv-key', 50)
    daemon.append(api, 'StarvA', primary, False, 'starv-key', 100)
    daemon.append(api, 'Busy', busy, False, 'busy-key', 0)
    h = daemon.wait_history(api, 'StarvA', timeout=240)
    stopped = _grep_log(t, 'nothing recovered for')
    integ = _verify_output(t, data, '.mkv', dirs=(('main', 'dst'), ('main', 'inter')))
    ok = stopped == 0 and integ and h['Status'].startswith('SUCCESS')
    return ('streamstarved', ok, 'status=%s stopped_logs=%d integrity=%s delayed=%d'
            % (h['Status'], stopped, integ, daemon.proxy.delayed))


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


def scenario_xpackgapdonor(daemon, t):
    """Cross-packing repair from a donor missing one article inside the hole: the
    rest of the hole is still recovered from it. A read fails as a whole when any
    donor article in it is missing, and that gave up the entire hole."""
    size = 6_000_000
    data = _payload(size, 6150)
    volumes = generators.rar3_store_volumes('movie.mkv', data, 2_000_000)
    payloads, members = {}, []
    missing = [set(), {2, 3, 4}, set()]        # vol2: a 1.5 MB data hole
    for i, (vol, miss) in enumerate(zip(volumes, missing), 1):
        rel = 'xgdA/rel.part%02d.rar' % i
        name = 'Rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        payloads[name] = vol
        members.append((rel, name, len(vol), 500_000, miss))
    dp = _place_copy(t, 'xgdB', data, 'movie.mkv')
    # the donor lacks one 300 KB article inside the hole (inner offset about 2.7 MB)
    donor_members = [(dp, 'movie.mkv', size, 300_000, {10})]
    h, integ, c = _xpack_run(daemon, t, 'Xgd', members, donor_members, payloads)
    with open(t.path('nzbget.log'), errors='replace') as f:
        found = re.findall(r'Recovered ([\d.]+) MB .*?of movie\.mkv from duplicate', f.read())
    recovered_mb = max((float(x) for x in found), default=0.0)
    ok = recovered_mb >= 1.0 and integ['Rel.part01.rar'] and integ['Rel.part03.rar']
    return ('xpackgapdonor', ok, 'status=%s recovered_mb=%.1f repaired=%d' % (
        h['Status'], recovered_mb, c['repaired']))


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
    # marked dead by a dupe tool: no failover fetches it, so stream repair is the
    # last option left (B78) - the repair itself is what these scenarios test
    _ds_append(api, 'Don' + tag, build_multi_nzb(donor_members), tag + '-key', 50,
               params=[{'Name': 'DupeAlive', 'Value': '0'}])
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
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    api.resumepost()
    h = daemon.wait_history(api, 'RelRS')
    recreated = _grep_log(t, 'Recreating Rel.part03.rar')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=both_dirs) for m in members)
    return ('wholefilerestart', queued_job == 1 and recreated == 1 and integ,
            'status=%s queued_logs=%d recreated_logs=%d integrity=%s'
            % (h['Status'], queued_job, recreated, integ))


def scenario_wholefilestale(daemon, t):
    """A recreation of a missing volume was cut short by a shutdown: the volume is
    on disk at its full size, mostly zeros, and its job wasn't written back. After
    the restart it is recreated again and the release completes. Every later pass
    refused it ("file already exists"), and the zeros stayed."""
    members, donor_members, payloads = _wholefile_fixture(t, 'ws')
    api = daemon.wait_ready()
    api.pausepost()
    daemon.append(api, 'DonWS', build_multi_nzb(donor_members), True, 'ws-key', 50)
    daemon.append(api, 'RelWS', build_multi_nzb(members), False, 'ws-key', 100)
    deadline = time.time() + 120
    while time.time() < deadline and _grep_log(t, 'Collection RelWS completely downloaded') == 0:
        time.sleep(0.5)
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    part01 = next((rel for rel in t.find_files('main') if rel.endswith('Rel.part01.rar')), None)
    if part01:
        stale = os.path.join(os.path.dirname(t.path(part01)), 'Rel.part03.rar')
        with open(stale, 'wb') as f:
            f.truncate(len(payloads['Rel.part03.rar']))
    daemon.start()
    api = daemon.wait_ready()
    api.resumepost()
    h = daemon.wait_history(api, 'RelWS')
    both_dirs = (('main', 'dst'), ('main', 'inter'))
    integ = all(_verify_output(t, payloads[m[1]], '.rar', dirs=both_dirs) for m in members)
    refused = _grep_log(t, 'file already exists')
    ok = bool(part01) and integ and refused == 0
    return ('wholefilestale', ok, 'status=%s integrity=%s refused_logs=%d' % (h['Status'], integ, refused))


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
    # marked dead by a dupe tool: no failover fetches it, so stream repair is the
    # last option left (B78) - the repair itself is what these scenarios test
    _ds_append(api, 'Don' + tag, build_multi_nzb(donor_members), tag + '-key', 50,
               params=[{'Name': 'DupeAlive', 'Value': '0'}])
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
    # marked dead by a dupe tool: no failover fetches it, so stream repair is the
    # last option left (B78) - the repair itself is what these scenarios test
    _ds_append(api, 'Don' + tag, build_multi_nzb(donor_members), tag + '-key', 50,
               params=[{'Name': 'DupeAlive', 'Value': '0'}])
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
    # marked dead by a dupe tool: no failover fetches it, so stream repair is the
    # last option left (B78) - the repair itself is what these scenarios test
    _ds_append(api, 'Don' + tag, build_multi_nzb(donor_members), tag + '-key', 50,
               params=[{'Name': 'DupeAlive', 'Value': '0'}])
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


def _retry_parked_release(daemon, t):
    """The release of the B22 and B29 scenarios: parked for health (no par2,
    HealthCheck=park: a whole volume is gone, health < 85%) after one finished
    volume kept a hole, then "Download remaining files" (HistoryReturn). Returns
    the parked status, the final history entry and a byte check of a volume."""
    seg_primary, seg_donor = 500_000, 300_000
    small, big = 1_500_000, 6_000_000
    members = [
        ('rkA/x.part01.rar', 'Rel.part01.rar', small, seg_primary, {3}),     # its last article is missing
        # (the Recovered line names the duplicate; Queueing doesn't)
        ('rkA/x.part02.rar', 'Rel.part02.rar', big, seg_primary, set(range(1, 13))),   # gone: health < 85%
        ('rkA/x.part03.rar', 'Rel.part03.rar', big, seg_primary, set()),
    ]
    payloads = {}
    for i, m in enumerate(members):
        payloads[m[1]] = _payload(m[2], 9700 + i)
        t.write_file(os.path.join('data', m[0]), payloads[m[1]])
    donor = [(m[0].replace('rkA', 'rkB'), m[1].replace('Rel.', 'Other.'), m[2], seg_donor, set()) for m in members]
    for dm, m in zip(donor, members):
        t.write_file(os.path.join('data', dm[0]), payloads[m[1]])
    api = daemon.wait_ready()
    daemon.append(api, 'DonRK', build_multi_nzb(donor), True, 'rk-key', 50)
    daemon.append(api, 'RelRK', build_multi_nzb(members), False, 'rk-key', 100)
    h = daemon.wait_history(api, 'RelRK')
    nzbid = h['NZBID']
    parked = h['Status']
    api.editqueue('HistoryReturn', 0, '', [nzbid])
    time.sleep(3)
    deadline = time.time() + 180
    while time.time() < deadline and any(g['NZBID'] == nzbid for g in api.listgroups()):
        time.sleep(1)
    h2 = [x for x in api.history() if x['NZBID'] == nzbid][0]
    def intact(name):
        dst = [rel for rel in t.find_files('main') if rel.endswith(name)]
        return bool(dst) and t.read_file(dst[0]) == payloads[name]
    return parked, h2, intact


def scenario_retrykeepsjobs(daemon, t):
    """B22: a release is parked for health (no par2, HealthCheck=park: a whole
    volume is gone, health < 85%) after one finished volume kept a hole;
    "Download remaining files" (HistoryReturn) downloads the rest, and
    post-processing must still repair the finished volume's hole from the
    duplicate. Retrying used to drop every saved repair job, so the finished
    volume (not downloaded again) was never repaired."""
    parked, h2, intact = _retry_parked_release(daemon, t)
    repaired = _grep_log(t, 'of Rel.part01.rar from duplicate DonRK')
    ok = parked.startswith('FAILURE') and repaired >= 1 and intact('Rel.part01.rar')
    return ('retrykeepsjobs', ok, 'parked=%s status=%s repaired_logs=%d intact=%s'
            % (parked, h2['Status'], repaired, intact('Rel.part01.rar')))


def scenario_retryparkedname(daemon, t):
    """B29: the volume that was parked while it downloaded is recorded by its
    temporary output name (<id>.out.tmp); after "Download remaining files" it
    must download and be repaired under its own name (Rel.part02.rar), so a
    whole-file recreation from the duplicate can pair it. Both volumes end
    byte-identical and the release succeeds."""
    parked, h2, intact = _retry_parked_release(daemon, t)
    temp_named = _grep_log(t, '.out.tmp (no article available)') + _grep_log(t, 'Stream repair of 5.out.tmp')
    ok = (parked.startswith('FAILURE') and 'SUCCESS' in h2['Status'] and intact('Rel.part01.rar') and
          intact('Rel.part02.rar') and temp_named == 0)
    return ('retryparkedname', ok, 'parked=%s status=%s part01=%s part02=%s temp_named_logs=%d'
            % (parked, h2['Status'], intact('Rel.part01.rar'), intact('Rel.part02.rar'), temp_named))


def scenario_healthlastarticle(daemon, t):
    """The failed article that drops health below critical is a file's last
    one: the health cancel deletes the file inside that article's completion.
    The file is still whole but for its hole, and must be finished like any
    other (its hole queued for stream repair), not dropped unassembled with no
    repair job (it was parked with its hole, and "Download remaining files"
    never repaired it). One connection makes the order deterministic."""
    seg_primary, seg_donor = 100_000, 60_000
    payload = _payload(1_000_000, 9800)
    t.write_file('data/hlA/x.part01.rar', payload)
    t.write_file('data/hlB/x.part01.rar', payload)
    daemon_api = daemon.wait_ready()
    daemon.append(daemon_api, 'DonHL', build_multi_nzb(
        [('hlB/x.part01.rar', 'Other.part01.rar', len(payload), seg_donor, set())]), True, 'hl-key', 50)
    daemon.append(daemon_api, 'RelHL', build_multi_nzb(
        [('hlA/x.part01.rar', 'Rel.part01.rar', len(payload), seg_primary, {9, 10})]), False, 'hl-key', 100)
    h = daemon.wait_history(daemon_api, 'RelHL')
    queued = _grep_log(t, 'Queueing stream repair of Rel.part01.rar')
    return ('healthlastarticle', h['Status'].startswith('FAILURE') and queued >= 1,
            'status=%s health=%d queued_logs=%d' % (h['Status'], h['Health'], queued))


def scenario_streamgrouped(daemon, t):
    """B2: two news servers of one group (the same account, Server1.Group =
    Server2.Group = 1) and a duplicate that lacks an article stream repair asks
    for. Once the first server says 430 the pool won't hand out the second
    (same group and level), so it isn't worth waiting for: the repair moves on
    at once instead of waiting ArticleTimeout (20 s) per missing article."""
    members, donor_members, payloads = _wholefile_fixture(t, 'sg')
    members[2] = members[2][:4] + (set(),)               # part03 complete
    members[3] = members[3][:4] + ({2},)                 # part04 article 2 missing ...
    donor_members[3] = donor_members[3][:4] + (set(range(2, 4)),)  # ... on the duplicate too
    api = daemon.wait_ready()
    daemon.append(api, 'DonSG', build_multi_nzb(donor_members), True, 'sg-key', 50)
    daemon.append(api, 'RelSG', build_multi_nzb(members), False, 'sg-key', 100)
    start = time.time()
    h = daemon.wait_history(api, 'RelSG', timeout=300)
    took = time.time() - start
    repaired = _grep_log(t, 'from duplicate DonSG')
    ok = took < 18 and repaired >= 1
    return ('streamgrouped', ok, 'status=%s took=%.0fs repaired_logs=%d' % (h['Status'], took, repaired))


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


def scenario_dupefailoverraw(daemon, t):
    """B25: HealthCheck=dupe with RawArticle=yes, where article borrowing never
    applies (the articles stay encoded): the attempt count stays at 0, and the
    gate that gives borrowing its 32-article sample must not block the failover
    for good. The dead primary is abandoned early and the backup fetched."""
    seg = 100_000
    vol_dead, vol_backup = 2_900_000, 3_000_000
    n = vol_dead // seg
    # every 5th article exists: the dead-pick probe finds the posting alive and
    # leaves it to the health check, and it isn't hopeless (20% alive)
    dead = [('frA/d%d.bin' % i, 'Dead%d.bin' % i, vol_dead, seg,
             set(k for k in range(1, n + 1) if k % 5)) for i in range(6)]
    for m in dead:
        t.write_file(os.path.join('data', m[0]), _payload(vol_dead, 9600))
    bp = _place_copy(t, 'frB', _payload(vol_backup, 9601))
    backup = build_nzb(bp, 'Backup.bin', vol_backup, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(dead), True, 'fr-key', 100)
    daemon.append(api, 'Backup', backup, False, 'fr-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary')
    failed_over = _grep_log(t, 'Failing over Primary to duplicate Backup')
    probed = _grep_log(t, 'Dupe probe of download') + _grep_log(t, 'probe articles on no server')
    failed_articles = int(hp.get('FailedArticles', 0))
    ok = failed_over == 1 and failed_articles < 6 * n * 4 // 5
    return ('dupefailoverraw', ok, 'status=%s failover_logs=%d probe_logs=%d failed_articles=%d of %d'
            % (hp['Status'], failed_over, probed, failed_articles, 6 * n * 4 // 5))


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
    # the record is written once nzbget has exited: on its way out it saves its own
    t.procs[-1].wait(timeout=60)
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
    got = re.findall(r'results=\d+ [^\n]*', t.read_file('nzbget.log').decode(errors='replace'))
    return ('dupesearchfilters', summary == 1, 'summary_logs=%d got=%s' % (summary, got))


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
    client sends the pick and three backups of its own (other spellings of the
    same release), scored just below the pick (so nzbget would try them first):
    one 50% alive, one dead, one whole. The indexer has one more posting, 95%
    alive. After the search the members carry their measured health (DupeHealth;
    DupeAlive stays the mark of a duplicate a dupe tool added) and everything is
    scored by wholeness: whole backup base + 89, donor base + 85, half-dead
    backup base + 49, dead backup base + 1. The pick keeps its score. A backup
    of another release under the same key (B20: 720p) is left alone."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    postings = {'idx95': (ids('ix'), 420_000, 3, 11)}
    alive = set(ids('ix')[:38]) | set(ids('half')[:20]) | set(ids('whole')) | set(ids('other'))
    api = _ds_donor_env(daemon, t, postings, alive)
    backups = (('half', 'Show S01E01 1080p WEB H264-GRP', 1, 430_000),
               ('gone', 'Show_S01E01_1080p_WEB_H264-GRP', 2, 440_000),
               ('whole', 'show.s01e01.1080p.web.h264-grp', 3, 410_000),
               ('other', 'Show.S01E01.720p.WEB.H264-GRP', 4, 300_000))
    tags = {DS_TITLE: 'idx95'}
    for tag, name, below, size, *same in backups:
        # a backup may be the same posting as another (same message ids)
        _ds_append(api, name, _fake_nzb_ids(ids(same[0] if same else tag), size).decode(), DS_KEY, DS_PICK - below)
        tags[name] = tag
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(2)
    base = DS_PICK - 1000
    got = {}
    for h in api.history():
        tag = tags.get(h.get('NZBName'))
        if not tag:
            continue
        params = {p['Name']: p['Value'] for p in h.get('Parameters', [])}
        got[tag] = (h.get('DupeScore') - base, params.get('DupeAlive'), params.get('DupeHealth'))
    pick = _ds_group(api, DS_TITLE)
    want = {'whole': (89, None, '100'), 'idx95': (85, '95', None), 'half': (49, None, '50'),
            'gone': (1, '0', '0'), 'other': (1000 - 4, None, None)}
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


def _ds_interrupt_pick(daemon, t, action):
    """A search whose health checks are slow (each article 0.3 s); once it is
    checking, the pick is deleted or gets another DupeKey. Returns the api, the
    pick's id and the duplicates in history afterwards."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    postings = {'one': (ids('on'), 410_000, 1, 11), 'two': (ids('tt'), 420_000, 1, 12)}
    daemon.fake_nntp.delays.update({'on-': 0.3, 'tt-': 0.3})
    api = _ds_donor_env(daemon, t, postings, set(ids('on')) | set(ids('tt')))
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, 'result(s) from') == 0:
        time.sleep(0.2)
    time.sleep(1)
    pick = _ds_group(api, DS_TITLE)
    if action == 'delete':
        api.editqueue('GroupDelete', '', [pick['NZBID']])
    elif action == 'key':
        api.editqueue('GroupSetDupeKey', 'another-key', [pick['NZBID']])
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    donors = [h for h in api.history() if h.get('NZBName') == DS_TITLE and h['NZBID'] != pick['NZBID']]
    queued = [g for g in api.listgroups() if g['NZBName'] == DS_TITLE and g['NZBID'] != pick['NZBID']]
    return api, pick, donors + queued


def scenario_dupesearchpickdeleted(daemon, t):
    """B16: the user deletes the pick while its search checks the postings: no
    donor is added (with the pick deleted, the first donor would download)."""
    api, pick, donors = _ds_interrupt_pick(daemon, t, 'delete')
    stopped = _grep_log(t, 'the pick was deleted')
    ok = not donors and stopped >= 1
    return ('dupesearchpickdeleted', ok, 'donors=%d stopped_logs=%d' % (len(donors), stopped))


def scenario_dupesearchpickgoneadd(daemon, t):
    """B33: the pick is deleted while its first donor is being added (a scan
    extension, which runs inside the add, deletes it): the donor just added is
    removed again, and no other donor is added."""
    api, pick, donors = _ds_interrupt_pick(daemon, t, None)
    deleted = _grep_log(t, 'deletepick: deleted')
    removed = _grep_log(t, 'just added: the pick was deleted')
    pick_left = [g for g in api.listgroups() if g['NZBID'] == pick['NZBID']]
    ok = deleted == 1 and removed == 1 and not donors and not pick_left
    return ('dupesearchpickgoneadd', ok, 'deleted_by_extension=%d removed_logs=%d donors=%d pick_queued=%d'
            % (deleted, removed, len(donors), len(pick_left)))


def scenario_dupesearchquickstop(daemon, t):
    """B37: nzbget shuts down during a donor health check (each article takes
    2 s, DupeHealthBudget=120): the check ends with the shutdown instead of
    waiting out its budget, and the posting it cut short isn't added (the add
    waited for the stopped scanner for good), so nzbget exits within seconds;
    the search resumes after the restart."""
    ids = ['qs-%d@x' % i for i in range(40)]
    daemon.fake_nntp.delays.update({'qs-': 2.0})
    api = _ds_donor_env(daemon, t, {'slow': (ids, 410_000, 1, 11)}, set(ids))
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, 'result(s) from') == 0:
        time.sleep(0.2)
    time.sleep(3)
    checking = _grep_log(t, ' added=') == 0
    started = time.time()
    try:
        api.shutdown()
    except Exception:
        pass
    try:
        t.procs[-1].wait(timeout=90)
    except Exception:
        pass
    took = time.time() - started
    # nothing is added from a check cut short: the search resumes after the restart
    resumes = _grep_log(t, 'the search resumes after the restart')
    added = _grep_log(t, ': added ')
    ok = checking and took < 15 and resumes == 1 and added == 0
    return ('dupesearchquickstop', ok, 'checking_at_shutdown=%s exit_took=%.1fs (want < 15 s) resume_logs=%d added_logs=%d'
            % (checking, took, resumes, added))


# a scan extension that deletes the pick while the first donor is being added
DELETE_PICK_EXTENSION = '''#!/usr/bin/env python3
##############################################################################
### NZBGET SCAN SCRIPT                                                     ###
# Deletes the pick while a duplicate search adds its first donor.
### NZBGET SCAN SCRIPT                                                     ###
##############################################################################
import json, os, sys, urllib.request

if int(os.environ.get('NZBNP_DUPESCORE', '0')) == %d:
    sys.exit(0)
marker = os.path.join(os.environ['NZBOP_TEMPDIR'], 'deletepick.done')
if os.path.exists(marker):
    sys.exit(0)
open(marker, 'w').close()
url = 'http://127.0.0.1:%%s/jsonrpc' %% os.environ['NZBOP_CONTROLPORT']

def call(method, *params):
    body = json.dumps({'method': method, 'params': list(params)}).encode()
    return json.loads(urllib.request.urlopen(url, body, timeout=10).read())['result']

for group in call('listgroups', 0):
    if group['DupeScore'] == %d:
        call('editqueue', 'GroupDelete', '', [group['NZBID']])
        print('[INFO] deletepick: deleted %%d' %% group['NZBID'])
sys.exit(0)
''' % (DS_PICK, DS_PICK)


def scenario_dupesearchresubmit(daemon, t):
    """B41: a pick and its duplicates are deleted for good (final delete, as
    nzbdavkodi cancels), then the same nzb-file is sent again as a new pick
    with a higher score (a client re-grabbing it).
    The new pick is searched, and the duplicates the first search added - all
    gone now - are added again: what a search sent earlier for the key isn't
    taken for something nzbget still holds."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    postings = {'twin100': (ids('tw'), 400_000, 1, 11), 'other100': (ids('ot'), 410_000, 1, 12)}
    api = _ds_donor_env(daemon, t, postings, set(ids('tw')) | set(ids('ot')))
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    first = [h['NZBID'] for h in api.history() if h.get('NZBName') == DS_TITLE]
    pick = _ds_group(api, DS_TITLE)
    # as nzbdavkodi cancels: final deletes, nothing left in history
    api.editqueue('GroupFinalDelete', '', [pick['NZBID']])
    time.sleep(1)
    api.editqueue('HistoryFinalDelete', '', first)
    time.sleep(1)
    _ds_append(api, DS_TITLE, _fake_nzb_ids(['pk-%d@x' % i for i in range(40)], 400_000).decode(),
               DS_KEY, DS_PICK + 100)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') < 2:
        time.sleep(0.5)
    time.sleep(1)
    searched = _grep_log(t, 'DupeSearch: searching duplicates of %s ' % DS_TITLE)
    again = _grep_log(t, 'added=2 ')
    donors = [h for h in api.history() if h.get('NZBName') == DS_TITLE and h['NZBID'] not in first
              and h.get('DupeScore', 0) < DS_PICK]
    ok = len(first) == 2 and searched == 2 and again == 2 and len(donors) == 2
    return ('dupesearchresubmit', ok, 'first_donors=%d searches=%d summaries_added2=%d donors_after=%d'
            % (len(first), searched, again, len(donors)))


def _ds_members(daemon, t, backups, alive):
    """A pick and the client's own backups (tag, name, below the pick, size), no
    indexer results; returns {tag: (score, DupeAlive, DupeHealth)} after the
    search, and the backups' scores before it."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    api = _ds_donor_env(daemon, t, {}, set(alive(ids)))
    tags = {}
    before = {}
    for tag, name, below, size, *same in backups:
        # a backup may be the same posting as another (same message ids)
        _ds_append(api, name, _fake_nzb_ids(ids(same[0] if same else tag), size).decode(), DS_KEY, DS_PICK - below)
        tags[name] = tag
        before[tag] = DS_PICK - below
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(2)
    got = {}
    for h in api.history():
        tag = tags.get(h.get('NZBName'))
        if tag:
            params = {p['Name']: p['Value'] for p in h.get('Parameters', [])}
            got[tag] = (h.get('DupeScore'), params.get('DupeAlive'), params.get('DupeHealth'))
    return got, before


def scenario_dupesearchrerank(daemon, t):
    """The client's own backups are ranked by wholeness, a twin of the pick
    first among whole ones: the dead one - which the client scored highest -
    sinks to the bottom and is marked DupeAlive=0; a dead one scored far below
    keeps its score (B52: never raised)."""
    backups = (('dead', 'Show S01E01 1080p WEB H264-GRP', 1, 430_000),
               ('half', 'Show_S01E01_1080p_WEB_H264-GRP', 2, 440_000),
               ('whole', 'show.s01e01.1080p.web.h264-grp', 3, 410_000),
               ('twin', 'SHOW.S01E01.1080P.WEB.H264-GRP', 4, 400_000),
               # B52: a dead backup scored far below the search's range keeps its score
               ('deep', 'Show.S01E01.1080p.Web.H264-GRP', 2500, 450_000))
    got, before = _ds_members(daemon, t, backups,
                              lambda ids: ids('half')[:20] + ids('whole') + ids('twin'))
    score = lambda tag: got.get(tag, (0,))[0]
    ok = (score('twin') > score('whole') > score('half') > score('dead') and
          score('dead') < before['dead'] and got.get('dead', (0, None))[1] == '0' and
          got.get('half', (0, None))[1] is None and
          score('deep') == before['deep'] and got.get('deep', (0, None))[1] == '0')
    return ('dupesearchrerank', ok, 'got=%s before=%s' % (sorted(got.items()), sorted(before.items())))


def scenario_dupesearchalldead(daemon, t):
    """Every backup of the pick is dead: nothing to order, so their scores stay;
    each is marked dead (DupeAlive=0) so no failover fetches it."""
    backups = (('d1', 'Show S01E01 1080p WEB H264-GRP', 1, 430_000),
               ('d2', 'Show_S01E01_1080p_WEB_H264-GRP', 2, 440_000))
    got, before = _ds_members(daemon, t, backups, lambda ids: [])
    ok = all(got.get(tag, (None,))[0] == before[tag] and got[tag][1] == '0' for tag in before)
    return ('dupesearchalldead', ok, 'got=%s before=%s' % (sorted(got.items()), sorted(before.items())))


def scenario_dupesearchtwinmember(daemon, t):
    """Two of the client's backups are the same posting (Fury 2014: a twin the
    check skipped kept its old score and outranked every checked backup). The
    posting is checked once, and its twin is ranked with it: scored as half
    alive, it never stays above the whole backup."""
    backups = (('stale', 'Show S01E01 1080p WEB H264-GRP', 1, 440_000, 'half'),
               ('half', 'Show_S01E01_1080p_WEB_H264-GRP', 3, 440_000),
               ('whole', 'show.s01e01.1080p.web.h264-grp', 2, 410_000))
    got, before = _ds_members(daemon, t, backups, lambda ids: ids('half')[:20] + ids('whole'))
    score = lambda tag: got.get(tag, (0,))[0]
    ok = score('whole') > score('stale') and score('whole') > score('half') and score('stale') < before['stale']
    return ('dupesearchtwinmember', ok, 'got=%s before=%s' % (sorted(got.items()), sorted(before.items())))


def scenario_dupesearchfailedfirst(daemon, t):
    """B56 (Industry S04E07 4162): the pick is dead and fails over to the client's
    one backup before its search is due (DupeSearchDelay=15). The search still
    runs, from the failed pick in history, and adds the indexer's posting under
    the key. Before, the search found the pick gone from the queue and gave up."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    daemon.fake_nntp.alive = set(ids('bk')) | set(ids('ix'))
    results = [{'title': DS_TITLE, 'link': 'http://127.0.0.1:%d/getnzb/ix' % daemon.newznab.port,
                'size': 420_000, 'grabs': 3, 'indexer': 'Idxix', 'date': 'Tue, 11 Jun 2025 01:10:05 +0000'}]

    def respond(params, path):
        if path.startswith('/getnzb/'):
            return 200, _fake_nzb_ids(ids('ix'), 420_000)
        if params.get('q') == 'show s01e01 1080p web h264 grp':
            return 200, newznab_xml(results)
        return 200, newznab_xml([])

    api = daemon.wait_ready()
    daemon.newznab.respond = respond
    # the pick waits until the duplicate check filed the backup in history
    pick_id = _ds_append(api, DS_TITLE, _fake_nzb_ids(ids('pk'), 400_000).decode(), DS_KEY, DS_PICK)
    _ds_append(api, 'Show S01E01 1080p WEB H264-GRP', _fake_nzb_ids(ids('bk'), 430_000).decode(), DS_KEY, DS_PICK - 1)
    deadline = time.time() + 10
    while time.time() < deadline and not any(h['NZBName'] == 'Show S01E01 1080p WEB H264-GRP' for h in api.history()):
        time.sleep(0.2)
    api.editqueue('GroupResume', 0, '', [pick_id])
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    failed_first = _log_before(t, 'Found duplicate Show S01E01', 'DupeSearch: searching duplicates of')
    pick = [h for h in api.history() if h.get('NZBName') == DS_TITLE and h.get('DupeScore') == DS_PICK]
    donors = [x for x in api.history() + api.listgroups()
              if any(p['Name'] == 'DupeSearch' for p in x.get('Parameters', []))]
    ok = failed_first and bool(pick) and 'SUCCESS' not in pick[0]['Status'] and len(donors) == 1 and \
        _grep_log(t, 'it failed before its search was due') == 1
    return ('dupesearchfailedfirst', ok, 'failed_first=%s pick=%s donors=%d searched_from_history=%d'
            % (failed_first, pick and pick[0]['Status'], len(donors), _grep_log(t, 'it failed before its search was due')))


def scenario_dupesearchfailedrestart(daemon, t):
    """B64: the pick is dead and fails over to the client's backup, then nzbget
    restarts before the pick's search is due (DupeSearchDelay=15). After the
    restart the pick is in history, not in the queue: it is still searched,
    from history, and the indexer's posting is added. Before, the start-up scan
    looked at the queue only, and the search was lost."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    daemon.fake_nntp.alive = set(ids('bk')) | set(ids('ix'))
    results = [{'title': DS_TITLE, 'link': 'http://127.0.0.1:%d/getnzb/ix' % daemon.newznab.port,
                'size': 420_000, 'grabs': 3, 'indexer': 'Idxix', 'date': 'Tue, 11 Jun 2025 01:10:05 +0000'}]

    def respond(params, path):
        if path.startswith('/getnzb/'):
            return 200, _fake_nzb_ids(ids('ix'), 420_000)
        if params.get('q') == 'show s01e01 1080p web h264 grp':
            return 200, newznab_xml(results)
        return 200, newznab_xml([])

    api = daemon.wait_ready()
    daemon.newznab.respond = respond
    pick_id = _ds_append(api, DS_TITLE, _fake_nzb_ids(ids('pk'), 400_000).decode(), DS_KEY, DS_PICK)
    _ds_append(api, 'Show S01E01 1080p WEB H264-GRP', _fake_nzb_ids(ids('bk'), 430_000).decode(), DS_KEY, DS_PICK - 1)
    deadline = time.time() + 10
    while time.time() < deadline and not any(h['NZBName'] == 'Show S01E01 1080p WEB H264-GRP' for h in api.history()):
        time.sleep(0.2)
    api.editqueue('GroupResume', 0, '', [pick_id])
    deadline = time.time() + 20
    while time.time() < deadline and _grep_log(t, 'Found duplicate Show S01E01') == 0:
        time.sleep(0.2)
    before = _grep_log(t, 'DupeSearch: searching duplicates of')
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    searched = _grep_log(t, 'DupeSearch: searching duplicates of %s ' % DS_TITLE)
    donors = [x for x in api.history() + api.listgroups()
              if any(p['Name'] == 'DupeSearch' for p in x.get('Parameters', []))]
    ok = before == 0 and searched == 1 and len(donors) == 1
    return ('dupesearchfailedrestart', ok, 'searched_before_restart=%d searched=%d donors=%d'
            % (before, searched, len(donors)))


def scenario_dupesearchtwopicks(daemon, t):
    """Two picks of one release arrive at once under the same key, with equal
    scores (a client that sent its request twice): the second is filed in
    history as a backup, only one downloads, and the key is searched once."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    api = _ds_donor_env(daemon, t, {}, set(ids('two')))
    _ds_append(api, 'Show S01E01 1080p WEB H264-GRP', _fake_nzb_ids(ids('two'), 400_000).decode(), DS_KEY, DS_PICK)
    deadline = time.time() + 30
    while time.time() < deadline and _grep_log(t, 'DupeSearch: searching duplicates of') == 0:
        time.sleep(0.2)
    time.sleep(3)
    queued = [g['NZBName'] for g in api.listgroups() if g.get('DupeKey') == DS_KEY]
    second = [h['Status'] for h in api.history() if h['NZBName'] == 'Show S01E01 1080p WEB H264-GRP']
    searched = _grep_log(t, 'DupeSearch: searching duplicates of')
    ok = queued == [DS_TITLE] and second == ['DELETED/DUPE'] and searched == 1
    return ('dupesearchtwopicks', ok, 'queued=%s second=%s searches=%d' % (queued, second, searched))


def scenario_dupesearchindexerdown(daemon, t):
    """Review item 3: every query of a search fails (the indexer is down). The
    key isn't held for six hours: after a restart the pick is searched again,
    and with the indexer back the posting is added. Before, the key stayed
    marked searched (kept on disk) and no search followed."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    daemon.fake_nntp.alive = set(ids('ix')) | set(ids('pk'))
    results = [{'title': DS_TITLE, 'link': 'http://127.0.0.1:%d/getnzb/ix' % daemon.newznab.port,
                'size': 420_000, 'grabs': 3, 'indexer': 'Idxix', 'date': 'Tue, 11 Jun 2025 01:10:05 +0000'}]
    state = {'down': True}

    def respond(params, path):
        if state['down']:
            return 503, b'down'
        if path.startswith('/getnzb/'):
            return 200, _fake_nzb_ids(ids('ix'), 420_000)
        if params.get('q') == 'show s01e01 1080p web h264 grp':
            return 200, newznab_xml(results)
        return 200, newznab_xml([])

    api = daemon.wait_ready()
    daemon.newznab.respond = respond
    _ds_append(api, DS_TITLE, _fake_nzb_ids(ids('pk'), 400_000).decode(), DS_KEY, DS_PICK)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, 'result(s) from') == 0:
        time.sleep(0.5)
    time.sleep(2)
    released = _grep_log(t, 'no indexer answered, the key may be searched again')
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    state['down'] = False
    summaries = _grep_log(t, ' added=')
    daemon.start()
    api = daemon.wait_ready()
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == summaries:
        time.sleep(0.5)
    searches = _grep_log(t, 'DupeSearch: searching duplicates of %s ' % DS_TITLE)
    donors = [x for x in api.history() + api.listgroups()
              if any(p['Name'] == 'DupeSearch' for p in x.get('Parameters', []))]
    ok = released == 1 and searches == 2 and len(donors) == 1
    return ('dupesearchindexerdown', ok, 'released_logs=%d searches=%d donors=%d' % (released, searches, len(donors)))


def scenario_dupesearchquerydrop(daemon, t):
    """The indexer drops the connection of the search that has the results,
    once, without an answer (as an indexer under load does): the search asks it
    once more and still finds the posting. Before, that query's results were
    lost and nothing was added."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    daemon.fake_nntp.alive = set(ids('qd')) | set(ids('pk'))
    results = [{'title': DS_TITLE, 'link': 'http://127.0.0.1:%d/getnzb/qd' % daemon.newznab.port,
                'size': 420_000, 'grabs': 3, 'indexer': 'Idxqd', 'date': 'Tue, 11 Jun 2025 01:10:05 +0000'}]
    dropped = {'n': 0}

    def respond(params, path):
        if path.startswith('/getnzb/'):
            return 200, _fake_nzb_ids(ids('qd'), 420_000)
        if params.get('q') == 'show s01e01 1080p web h264 grp':
            if not dropped['n']:
                dropped['n'] += 1
                return None, b''
            return 200, newznab_xml(results)
        return 200, newznab_xml([])

    api = daemon.wait_ready()
    daemon.newznab.respond = respond
    _ds_append(api, DS_TITLE, _fake_nzb_ids(ids('pk'), 400_000).decode(), DS_KEY, DS_PICK)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    added = _grep_log(t, 'added=1 ')
    ok = dropped['n'] == 1 and added == 1
    return ('dupesearchquerydrop', ok, 'dropped=%d added_logs=%d' % (dropped['n'], added))


def _fleet(daemon, members, key='fleet-key', timeout=30, http_timeout=None):
    """Calls appendfleet (JSON-RPC) with members [(name, nzb text)]; returns its result."""
    import json as _json
    import urllib.request as _req
    params = [key, 'test', 0, timeout]
    for name, nzb in members:
        params += [name, 'http://127.0.0.1:9/none.nzb' if nzb is None else base64.standard_b64encode(nzb.encode()).decode()]
    body = _json.dumps({'method': 'appendfleet', 'params': params}).encode()
    request = _req.Request('http://127.0.0.1:%d/jsonrpc' % daemon.rpc_port, body, {'Content-Type': 'application/json'})
    with _req.urlopen(request, timeout=http_timeout or timeout + 30) as reply:
        return _json.loads(reply.read().decode())['result']


def _fleet_ids(prefix, n=40):
    return ['%s-%d@x' % (prefix, i) for i in range(n)]


def scenario_fleetdeadfirst(daemon, t):
    """appendfleet: the client sends a dead copy first, then a half one, then a
    whole one. The whole one is queued; the others wait in history ranked
    (half before dead, the dead one marked DupeAlive=0); the dead copy never
    downloads an article."""
    daemon.fake_nntp.alive = set(_fleet_ids('fw')) | set(_fleet_ids('fh')[:20])
    api = daemon.wait_ready()
    reply = _fleet(daemon, [('Show.S01E01.dead', _fake_nzb_ids(_fleet_ids('fd'), 400_000).decode()),
                            ('Show.S01E01.half', _fake_nzb_ids(_fleet_ids('fh'), 400_000).decode()),
                            ('Show.S01E01.whole', _fake_nzb_ids(_fleet_ids('fw'), 400_000).decode())])
    order = [(m['Name'], m['Status'], m['Alive']) for m in reply['Members']]
    hw = daemon.wait_history(api, 'Show.S01E01.whole', timeout=120)
    deadline = time.time() + 30
    while time.time() < deadline and not hw['Status'].startswith(('SUCCESS', 'FAILURE')):
        time.sleep(0.5)
        hw = daemon.wait_history(api, 'Show.S01E01.whole', timeout=60)
    dead = next((h for h in api.history() if h['NZBName'] == 'Show.S01E01.dead'), {})
    tried = int(dead.get('SuccessArticles', 0)) + int(dead.get('FailedArticles', 0))
    ok = ([o[0] for o in order] == ['Show.S01E01.whole', 'Show.S01E01.half', 'Show.S01E01.dead'] and
          order[0][1] == 'QUEUED' and order[2][1] == 'DEAD' and reply['Chosen'] == reply['Members'][0]['NZBID'] and
          hw['Status'].startswith('SUCCESS') and tried == 0)
    return ('fleetdeadfirst', ok, 'order=%s whole=%s dead_tried=%d complete=%s' % (order, hw['Status'], tried, reply['Complete']))


def scenario_fleettwins(daemon, t):
    """appendfleet: two members are the same posting: it is checked once, added
    once; the copy is reported SAME_POSTING with the id of the one added."""
    daemon.fake_nntp.alive = set(_fleet_ids('tw')) | set(_fleet_ids('to'))
    daemon.wait_ready()
    reply = _fleet(daemon, [('Show.S01E01.a', _fake_nzb_ids(_fleet_ids('tw'), 400_000).decode()),
                            ('Show.S01E01.b', _fake_nzb_ids(_fleet_ids('tw'), 400_000).decode()),
                            ('Show.S01E01.c', _fake_nzb_ids(_fleet_ids('to'), 410_000).decode())])
    byname = {m['Name']: m for m in reply['Members']}
    # every item the fleet added carries its rank (DupeFleet), for queue extensions
    api = daemon.wait_ready()
    fleet_params = {x['NZBName']: {p['Name']: p['Value'] for p in x.get('Parameters', [])}.get('DupeFleet')
                    for x in api.listgroups() + api.history()}
    ranks_ok = (fleet_params.get('Show.S01E01.a') == str(byname['Show.S01E01.a']['Rank']) and
                fleet_params.get('Show.S01E01.c') == str(byname['Show.S01E01.c']['Rank']))
    ok = ranks_ok and (byname['Show.S01E01.b']['Status'] == 'SAME_POSTING' and byname['Show.S01E01.b']['NZBID'] == 0 and
          byname['Show.S01E01.b']['SameAs'] == byname['Show.S01E01.a']['NZBID'] and
          byname['Show.S01E01.a']['NZBID'] > 0 and reply['Chosen'] > 0)
    return ('fleettwins', ok, 'fleet_params=%s members=%s' % (fleet_params, [(m['Name'], m['Status'], m['NZBID'], m['SameAs']) for m in reply['Members']]))


def scenario_fleettimeout(daemon, t):
    """appendfleet: six postings, the news server answers slowly (2 s a request),
    and the fleet allows 8 s: the call returns by then (F1-b: the postings were
    checked one after another, each with its own budget), queues the best it
    knows, and says the check was not complete."""
    tags = ['t%d' % i for i in range(6)]
    daemon.fake_nntp.alive = set().union(*(set(_fleet_ids(tag)) for tag in tags))
    daemon.fake_nntp.delays.update({tag + '-': 2.0 for tag in tags})
    daemon.wait_ready()
    start = time.time()
    reply = _fleet(daemon, [('Show.S01E01.%s' % tag, _fake_nzb_ids(_fleet_ids(tag), 400_000 + i).decode())
                            for i, tag in enumerate(tags)], timeout=8)
    took = time.time() - start
    ok = reply['Chosen'] > 0 and not reply['Complete'] and took < 11
    return ('fleettimeout', ok, 'chosen=%s complete=%s took=%.1fs' % (reply['Chosen'], reply['Complete'], took))


def scenario_fleetallerror(daemon, t):
    """appendfleet (F1-a): every member is unreadable (junk content, an
    unreachable url): the reason is NO_USABLE_MEMBERS, not ALL_DEAD, and the url
    reads "connection failed"."""
    api = daemon.wait_ready()
    reply = _fleet(daemon, [('Show.S01E01.junk', 'not an nzb-file'), ('Show.S01E01.url', None)], timeout=10)
    reasons = [m['Reason'] for m in reply['Members']]
    ok = reply['Chosen'] == 0 and reply['Reason'] == 'NO_USABLE_MEMBERS' and \
        any('connection failed' in r for r in reasons) and not api.listgroups()
    return ('fleetallerror', ok, 'reply=%s' % reply)


def scenario_fleetotherkey(daemon, t):
    """appendfleet (F1-c): the release was downloaded under another client's key
    and is on disk; a fleet for it under a new key downloads nothing
    (ALREADY_DOWNLOADED, members SKIPPED)."""
    daemon.fake_nntp.alive = set(_fleet_ids('ok1')) | set(_fleet_ids('ok2'))
    api = daemon.wait_ready()
    _ds_append(api, DS_TITLE, _fake_nzb_ids(_fleet_ids('ok1'), 400_000).decode(), 'batch:key-a', 100, paused=False)
    first = daemon.wait_history(api, DS_TITLE, timeout=120)
    reply = _fleet(daemon, [('Show S01E01 1080p WEB H264-GRP', _fake_nzb_ids(_fleet_ids('ok2'), 410_000).decode())],
                   key='fleet:key-b')
    time.sleep(1)
    ok = first['Status'].startswith('SUCCESS') and reply['Chosen'] == 0 and \
        reply['Reason'] == 'ALREADY_DOWNLOADED' and reply['Members'][0]['Status'] == 'SKIPPED' and not api.listgroups() and \
        reply['Members'][0]['Alive'] == -1
    # (never health-checked: it was measured first, and read as dead with Alive 0)
    return ('fleetotherkey', ok, 'first=%s reply=%s' % (first['Status'], reply))


def scenario_fleetnokey(daemon, t):
    """appendfleet (F1-d): an empty key is derived from the first member's name
    (as a single append's), not refused."""
    daemon.fake_nntp.alive = set(_fleet_ids('nk'))
    api = daemon.wait_ready()
    reply = _fleet(daemon, [('Show.S01E01.nk', _fake_nzb_ids(_fleet_ids('nk'), 400_000).decode())], key='')
    group = next((g for g in api.listgroups() if g['NZBID'] == reply['Chosen']), None) or \
        next((h for h in api.history() if h['NZBID'] == reply['Chosen']), {})
    ok = reply['Chosen'] > 0 and group.get('DupeKey', '').startswith('dupes:')
    return ('fleetnokey', ok, 'chosen=%s key=%s' % (reply['Chosen'], group.get('DupeKey')))


def scenario_fleetresend(daemon, t):
    """appendfleet (F2, Women in Blue S02E02): a client sends the same fleet twice,
    1 s apart (a retry). The second call waits for the first: the whole posting
    stays the one downloading, the second reply names it (ALREADY_QUEUED) and
    reports its members as the same postings as the first's; the 12%-alive copy
    is never queued. Before, the two raced: the whole copy went to history and
    the 12% one downloaded."""
    import threading
    daemon.fake_nntp.alive = set(_fleet_ids('rw', 200)) | set(_fleet_ids('rl')[:5])
    daemon.fake_nntp.delays.update({'rw-': 0.3, 'rl-': 0.3})
    api = daemon.wait_ready()
    # 200 slow articles: still downloading when the second call comes
    members = [('Show.S01E01.whole', _fake_nzb_ids(_fleet_ids('rw', 200), 2_000_000).decode()),
               ('Show.S01E01.low', _fake_nzb_ids(_fleet_ids('rl'), 410_000).decode())]
    box = {}

    def send(slot):
        box[slot] = _fleet(daemon, members, key='fleet:resend', timeout=20)
    first = threading.Thread(target=send, args=('a',))
    first.start()
    time.sleep(1)
    send('b')
    first.join(timeout=60)
    a, b = box.get('a', {}), box.get('b', {})
    whole = next((m for m in a.get('Members', []) if m['Name'] == 'Show.S01E01.whole'), {})
    queued = [g['NZBName'] for g in api.listgroups()]
    ok = (a.get('Chosen') and a['Chosen'] == whole.get('NZBID') and b.get('Chosen') == a['Chosen'] and
          b.get('Reason') == 'ALREADY_QUEUED' and all(m['Status'] == 'SAME_POSTING' for m in b['Members']) and
          queued == ['Show.S01E01.whole'])
    return ('fleetresend', ok, 'a=%s b=%s queued=%s' % (a, b, queued))


def scenario_fleetresendslow(daemon, t):
    """appendfleet (F4): the same fleet sent twice, 1 s apart, against a slow
    server, each call allowing 10 s: the first uses its time checking; the second
    waits for the key, but its own limit counts from its arrival, so it replies
    within about 10 s - its members the same postings as the first's, Chosen the
    running download. Before, it waited out the first and then checked anew
    (the client gave up)."""
    import threading
    daemon.fake_nntp.alive = set(_fleet_ids('sw', 200)) | set(_fleet_ids('sl'))
    daemon.fake_nntp.delays.update({'sw-': 2.0, 'sl-': 2.0})
    daemon.wait_ready()
    members = [('Show.S01E01.whole', _fake_nzb_ids(_fleet_ids('sw', 200), 2_000_000).decode()),
               ('Show.S01E01.other', _fake_nzb_ids(_fleet_ids('sl'), 410_000).decode())]
    box = {}

    def send(slot):
        start = time.time()
        box[slot] = _fleet(daemon, members, key='fleet:slow', timeout=10)
        box[slot + 'took'] = time.time() - start
    first = threading.Thread(target=send, args=('a',))
    first.start()
    time.sleep(1)
    send('b')
    first.join(timeout=60)
    a, b = box.get('a', {}), box.get('b', {})
    ok = (a.get('Chosen', 0) > 0 and box.get('btook', 99) < 13 and b.get('Chosen') == a.get('Chosen') and
          b.get('Reason') == 'ALREADY_QUEUED' and all(m['Status'] == 'SAME_POSTING' for m in b.get('Members', [])))
    return ('fleetresendslow', ok, 'a_took=%.1f b_took=%.1f a=%s b=%s' % (box.get('atook', -1), box.get('btook', -1), a, b))


def scenario_fleetaddbackup(daemon, t):
    """appendfleet (F5, Women in Blue S02E04): while a fleet's chosen copy
    downloads, a second fleet brings the same copies plus a new posting: the new
    one is filed below everything the key holds, so no two items of the key
    share a score. Before, it took the score of the first fleet's backup."""
    daemon.fake_nntp.alive = set(_fleet_ids('aw', 200)) | set(_fleet_ids('ao')) | set(_fleet_ids('an'))
    daemon.fake_nntp.delays.update({'aw-': 0.3})
    api = daemon.wait_ready()
    whole = ('Show.S01E01.whole', _fake_nzb_ids(_fleet_ids('aw', 200), 2_000_000).decode())
    other = ('Show.S01E01.other', _fake_nzb_ids(_fleet_ids('ao'), 410_000).decode())
    new = ('Show.S01E01.new', _fake_nzb_ids(_fleet_ids('an'), 420_000).decode())
    a = _fleet(daemon, [whole, other], key='fleet:add', timeout=15)
    b = _fleet(daemon, [whole, other, new], key='fleet:add', timeout=15)
    items = [x for x in api.listgroups() + api.history() if x.get('DupeKey') == 'fleet:add']
    scores = [x['DupeScore'] for x in items]
    added = next((m for m in b['Members'] if m['Name'] == 'Show.S01E01.new'), {})
    ok = (b.get('Reason') == 'ALREADY_QUEUED' and added.get('NZBID', 0) > 0 and len(items) == 3 and
          len(set(scores)) == len(scores) and min(scores) == next(x['DupeScore'] for x in items if x['NZBID'] == added['NZBID']))
    return ('fleetaddbackup', ok, 'scores=%s b=%s' % (sorted((x['NZBID'], x['DupeScore']) for x in items), b))


def scenario_fleetfailover(daemon, t):
    """appendfleet: the chosen copy dies while it downloads (its articles are
    taken down after the check): nzbget fails over to the fleet's next-ranked
    backup, which downloads whole; the dead-ranked member is never fetched."""
    daemon.fake_nntp.alive = set(_fleet_ids('fa', 300)) | set(_fleet_ids('fb', 300)) | set(_fleet_ids('fc')[:5])
    daemon.fake_nntp.delays.update({'fa-': 0.2})
    api = daemon.wait_ready()
    reply = _fleet(daemon, [('Show.S01E01.a', _fake_nzb_ids(_fleet_ids('fa', 300), 3_000_000).decode()),
                            ('Show.S01E01.b', _fake_nzb_ids(_fleet_ids('fb', 300), 3_100_000).decode()),
                            ('Show.S01E01.c', _fake_nzb_ids(_fleet_ids('fc'), 400_000).decode())], key='fleet:fo')
    chosen = next(m for m in reply['Members'] if m['NZBID'] == reply['Chosen'])
    backup = next(m for m in reply['Members'] if m['Status'] == 'BACKUP')
    # the takedown: the chosen copy's articles are gone from the server
    daemon.fake_nntp.alive -= set(_fleet_ids(chosen['Name'][-1:] == 'a' and 'fa' or 'fb', 300))
    hb = daemon.wait_history(api, backup['Name'], timeout=180)
    deadline = time.time() + 180
    while time.time() < deadline and not hb['Status'].startswith(('SUCCESS', 'FAILURE')):
        time.sleep(0.5)
        hb = daemon.wait_history(api, backup['Name'], timeout=60)
    hc = next((h for h in api.history() if h['NZBName'] == 'Show.S01E01.c'), {})
    tried_c = int(hc.get('SuccessArticles', 0)) + int(hc.get('FailedArticles', 0))
    ok = hb['Status'].startswith('SUCCESS') and tried_c == 0 and _grep_log(t, 'Failing over') + _grep_log(t, 'Found duplicate') >= 1
    return ('fleetfailover', ok, 'chosen=%s backup=%s backup_status=%s c_tried=%d' % (chosen['Name'], backup['Name'], hb['Status'], tried_c))


def scenario_fleetbusy(daemon, t):
    """appendfleet (F7): a long download holds the server's 2 connections when a
    fleet comes. Downloads hold off while the fleet is checked, so the check
    gets connections: it finishes well inside its 20 s and measures both
    copies (the whole one queued, the dead one dead). Before, it used the whole
    limit and measured nothing."""
    daemon.fake_nntp.alive = set(_fleet_ids('busy', 600)) | set(_fleet_ids('bw'))
    daemon.fake_nntp.delays.update({'busy-': 3.0})
    api = daemon.wait_ready()
    _ds_append(api, 'Busy.Download', _fake_nzb_ids(_fleet_ids('busy', 600), 6_000_000).decode(), 'busy-key', 100, paused=False)
    time.sleep(3)
    start = time.time()
    reply = _fleet(daemon, [('Show.S01E01.dead', _fake_nzb_ids(_fleet_ids('bd'), 400_000).decode()),
                            ('Show.S01E01.whole', _fake_nzb_ids(_fleet_ids('bw'), 410_000).decode())],
                   key='fleet:busy', timeout=20)
    took = time.time() - start
    byname = {m['Name']: m for m in reply['Members']}
    busy_after = next((g for g in api.listgroups() if g['NZBName'] == 'Busy.Download'), None)
    ok = (reply['Complete'] and took < 15 and byname['Show.S01E01.whole']['Status'] == 'QUEUED' and
          byname['Show.S01E01.dead']['Status'] == 'DEAD' and busy_after is not None)
    return ('fleetbusy', ok, 'took=%.1f complete=%s members=%s' % (took, reply['Complete'],
            [(m['Name'], m['Status'], m['Alive']) for m in reply['Members']]))


def scenario_fleetdeadtwins(daemon, t):
    """appendfleet (F8): a pair of twins, both dead, nothing added: the copy
    names its twin by rank (SameAsRank), as it has no NZBID to name."""
    daemon.fake_nntp.alive = set()
    daemon.wait_ready()
    reply = _fleet(daemon, [('Show.S01E01.x', _fake_nzb_ids(_fleet_ids('dx'), 400_000).decode()),
                            ('Show.S01E01.y', _fake_nzb_ids(_fleet_ids('dx'), 400_000).decode())], key='fleet:dt')
    twin = next((m for m in reply['Members'] if m['Status'] == 'SAME_POSTING'), {})
    ok = reply['Reason'] == 'ALL_DEAD' and twin.get('SameAsRank') == 1 and twin.get('SameAs') == 0
    return ('fleetdeadtwins', ok, 'reply=%s' % reply)


def scenario_fleetlarge(daemon, t):
    """appendfleet (F7): five large postings (20,000 articles each). A fleet's
    sample of each is capped at 200 articles after the probe, not 5% (1,000):
    ranking a few copies needs less than measuring a donor."""
    tags = ['L%d' % i for i in range(5)]
    daemon.fake_nntp.alive = set().union(*(set(_fleet_ids(tag, 20000)) for tag in tags))
    daemon.wait_ready()
    before = daemon.fake_nntp.stats
    reply = _fleet(daemon, [('Show.S01E01.%s' % tag, _fake_nzb_ids(_fleet_ids(tag, 20000), 200_000_000 + i).decode())
                            for i, tag in enumerate(tags)], key='fleet:large', timeout=60)
    asked = daemon.fake_nntp.stats - before
    ok = reply['Complete'] and reply['Chosen'] > 0 and asked <= 5 * 220
    return ('fleetlarge', ok, 'stats=%d complete=%s chosen=%s' % (asked, reply['Complete'], reply['Chosen']))


def scenario_fleetcopy(daemon, t):
    """appendfleet: a member that is the same nzb-file as an item under another
    key, which the duplicate check files as a copy (DELETED/COPY), is reported
    COPY, not BACKUP."""
    daemon.fake_nntp.alive = set(_fleet_ids('cp')) | set(_fleet_ids('cq'))
    api = daemon.wait_ready()
    nzb = _fake_nzb_ids(_fleet_ids('cp'), 400_000).decode()
    _ds_append(api, 'abcdef0123', nzb, 'other-key', 100, paused=False)
    daemon.wait_history(api, 'abcdef0123', timeout=60)
    reply = _fleet(daemon, [('0123abcdef', _fake_nzb_ids(_fleet_ids('cq'), 410_000).decode()),
                            ('fedcba3210', nzb)], key='fleet:copy')
    byname = {m['Name']: m for m in reply['Members']}
    ok = byname['fedcba3210']['Status'] == 'COPY' and byname['0123abcdef']['Status'] == 'QUEUED'
    return ('fleetcopy', ok, 'members=%s' % [(m['Name'], m['Status'], m['NZBID']) for m in reply['Members']])


def scenario_fleetmostlydead(daemon, t):
    """appendfleet (F10, Slow Horses S04E02): one posting, 20% of it alive, on a
    slow server: the check runs to the limit (missing articles settle slowly),
    but the member is measured below the floor: the reason is ALL_DEAD, not
    INCOMPLETE (which is kept for members not measured at all)."""
    daemon.fake_nntp.alive = set(_fleet_ids('md', 200)[:40])
    daemon.fake_nntp.delays.update({'md-': 0.3})
    daemon.wait_ready()
    reply = _fleet(daemon, [('Show.S01E01.md', _fake_nzb_ids(_fleet_ids('md', 200), 2_000_000).decode())],
                   key='fleet:md', timeout=8)
    m = reply['Members'][0]
    ok = reply['Chosen'] == 0 and m['Alive'] >= 0 and m['Status'] == 'DEAD' and reply['Reason'] == 'ALL_DEAD'
    return ('fleetmostlydead', ok, 'reply=%s' % reply)


SLOW_POST_EXTENSION = '''#!/usr/bin/env python3
##############################################################################
### NZBGET POST-PROCESSING SCRIPT                                          ###
# Keeps a download in post-processing for a while.
### NZBGET POST-PROCESSING SCRIPT                                          ###
##############################################################################
import sys, time
time.sleep(25)
sys.exit(93)
'''


def scenario_fleetduringpost(daemon, t):
    """appendfleet: a fleet sent while another download is in post-processing
    (production: one sent while an item unpacked came back in 3.7 s with every
    member unmeasured, the first one queued by chance). Post-processing holds no
    news-server connection: the fleet is checked and ranked as any other."""
    daemon.fake_nntp.alive = set(_fleet_ids('qp')) | set(_fleet_ids('qw'))
    api = daemon.wait_ready()
    _ds_append(api, 'Post.Busy', _fake_nzb_ids(_fleet_ids('qp'), 400_000).decode(), 'post-key', 100, paused=False)
    # the slow post-processing script runs (25 s)
    def in_script():
        return any(g['NZBName'] == 'Post.Busy' and g['Status'] == 'EXECUTING_SCRIPT' for g in api.listgroups())
    deadline = time.time() + 60
    while time.time() < deadline and not in_script():
        time.sleep(0.3)
    in_post = in_script()
    start = time.time()
    reply = _fleet(daemon, [('Show.S01E01.dead', _fake_nzb_ids(_fleet_ids('qd'), 400_000).decode()),
                            ('Show.S01E01.whole', _fake_nzb_ids(_fleet_ids('qw'), 410_000).decode())],
                   key='fleet:post', timeout=20)
    took = time.time() - start
    still_post = in_script()
    byname = {m['Name']: m for m in reply['Members']}
    ok = (in_post and still_post and reply['Complete'] and all(m['Alive'] >= 0 for m in reply['Members']) and
          byname['Show.S01E01.whole']['Status'] == 'QUEUED' and byname['Show.S01E01.dead']['Status'] == 'DEAD')
    return ('fleetduringpost', ok, 'in_post=%s/%s took=%.1f complete=%s reply=%s' % (
        in_post, still_post, took, reply['Complete'], [(m['Name'], m['Status'], m['Alive']) for m in reply['Members']]))


def _rpc(daemon, method, params, timeout=60):
    """A JSON-RPC call; returns the reply (its 'result' or its 'error')."""
    import json as _json
    import urllib.request as _req
    body = _json.dumps({'method': method, 'params': params}).encode()
    request = _req.Request('http://127.0.0.1:%d/jsonrpc' % daemon.rpc_port, body, {'Content-Type': 'application/json'})
    with _req.urlopen(request, timeout=timeout) as reply:
        return _json.loads(reply.read().decode())


def scenario_appendconcurrent(daemon, t):
    """F14: appends at the same moment each get their own id. Ten with the same
    name (distinct contents, one key) and twenty with distinct names: before,
    most of the same-named ones and some others returned -1 - two appends picked
    the same file name and one overwrote the other."""
    import threading
    daemon.wait_ready()
    jobs = [('Same.Release', 'cs%d' % i) for i in range(10)] + [('Other.Release.%d' % i, 'co%d' % i) for i in range(20)]
    ids = [None] * len(jobs)
    barrier = threading.Barrier(len(jobs))

    def send(i, name, tag):
        nzb = base64.standard_b64encode(_fake_nzb_ids(_fleet_ids(tag, 5), 50_000)).decode()
        barrier.wait()
        ids[i] = _rpc(daemon, 'append', [name, nzb, 'test', 0, False, True, 'conc-key', 10 + i, 'all', []]).get('result')
    threads = [threading.Thread(target=send, args=(i, name, tag)) for i, (name, tag) in enumerate(jobs)]
    for th in threads:
        th.start()
    for th in threads:
        th.join(timeout=120)
    lost = sum(1 for x in ids if not isinstance(x, int) or x <= 0)
    distinct = len(set(x for x in ids if isinstance(x, int) and x > 0))
    return ('appendconcurrent', lost == 0 and distinct == len(jobs), 'lost=%d distinct=%d ids=%s' % (lost, distinct, ids))


def scenario_appendscorerange(daemon, t):
    """F16: a DupeScore out of the int range is an invalid parameter; it wrapped
    around before (2147483648 was stored as -2147483648, the lowest score)."""
    daemon.wait_ready()
    nzb = base64.standard_b64encode(_fake_nzb_ids(_fleet_ids('sr', 3), 30_000)).decode()
    replies = {score: _rpc(daemon, 'append', ['Score.%d' % i, nzb, 'test', 0, False, True, 'sr-key', score, 'all', []])
               for i, score in enumerate([2147483648, 4294967301, -2147483649])}
    rejected = all('error' in r and r['error'] and not r.get('result') for r in replies.values())
    ok_reply = _rpc(daemon, 'append', ['Score.ok', nzb, 'test', 0, False, True, 'sr-key', 2147483647, 'all', []])
    stored = next((g['DupeScore'] for g in daemon.wait_ready().listgroups() if g['NZBName'] == 'Score.ok'), None)
    ok = rejected and ok_reply.get('result', 0) > 0 and stored == 2147483647
    return ('appendscorerange', ok, 'replies=%s stored=%s' % (
        {k: (v.get('result'), (v.get('error') or {}).get('message') if isinstance(v.get('error'), dict) else v.get('error'))
         for k, v in replies.items()}, stored))


def _many_files_nzb(n, tag, same_name=False):
    """An nzb-file of n rar volumes, one article each (all named alike: obfuscated)."""
    files = ''.join(
        '<file poster="p" date="1" subject="&quot;%s&quot; yEnc (1/1)"><groups><group>a.b</group></groups>'
        '<segments><segment bytes="700000" number="1">%s-%d@x</segment></segments></file>' % (
            'edge.rar' if same_name else 'Big.Set.part%04d.rar' % (i + 1), tag, i)
        for i in range(n))
    return ('<?xml version="1.0" encoding="UTF-8"?><nzb xmlns="http://www.newzbin.com/DTD/2003/nzb">%s</nzb>' % files).encode()


def scenario_appendlarge(daemon, t):
    """F15: appending an nzb-file of 2,000 files takes about as long per file as
    one of 500 (production: 0.5 s for 500 files, 26 s for 2,000)."""
    daemon.wait_ready()
    took = {}
    for n in (500, 2000):
        nzb = base64.standard_b64encode(_many_files_nzb(n, 'al%d' % n, same_name=True)).decode()
        start = time.time()
        reply = _rpc(daemon, 'append', ['Big.Set.%d' % n, nzb, 'test', 0, False, True, 'big-%d' % n, 0, 'all', []], timeout=300)
        took[n] = (time.time() - start, reply.get('result'))
    ok = all(r > 0 for _, r in took.values()) and took[2000][0] < max(2.0, took[500][0] * 4 * 2)
    return ('appendlarge', ok, ' '.join('%d files: %.1f s (id %s)' % (n, s_, r) for n, (s_, r) in took.items()))


def scenario_appendurlodd(daemon, t):
    """P0 (production crash, SIGSEGV): appendurl with odd urls - empty, not a url,
    file://, ftp://, a 5,000-character one - is refused or fails cleanly, and the
    daemon stays up with its state files readable after a restart."""
    import http.server
    import threading

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == '/loop':
                self.send_response(302)
                self.send_header('Location', '/loop')
                self.end_headers()
            elif self.path == '/redir':
                self.send_response(302)
                self.send_header('Location', '/ok')
                self.end_headers()
            else:
                # production: the helper answered every other path with an nzb-file
                body = _fake_nzb_ids(['%s-%d@srv' % (uuid.uuid4().hex, i) for i in range(20)], 15_000_000)
                self.send_response(200)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        def log_message(self, *args):
            pass
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = 'http://127.0.0.1:%d' % server.server_address[1]
    daemon.wait_ready()
    replies = {}
    for label, url in [('ok', base + '/ok'), ('redir', base + '/redir'), ('loop', base + '/loop'), ('404', base + '/nope'),
                       ('file', 'file:///etc/hostname'), ('ftp', 'ftp://127.0.0.1:9/x.nzb'), ('notaurl', 'notaurl'),
                       ('long', base + '/' + 'a' * 5000), ('empty', '')]:
        try:
            # every call has the same name, as in production
            r = _rpc(daemon, 'appendurl', ['Odd-u.nzb', url, '', 0, False, True, 'odd-key', 0, 'SCORE', []], timeout=60)
            replies[label] = r.get('result', r.get('error'))
        except Exception as e:
            replies[label] = 'EXC %s' % str(e)[:60]
    time.sleep(8)
    server.shutdown()
    try:
        alive = bool(_rpc(daemon, 'version', [], timeout=10).get('result'))
    except Exception:
        alive = False
    return ('appendurlodd', alive, 'alive=%s replies=%s' % (alive, replies))


def scenario_addstorm(daemon, t):
    """P0 (production SIGSEGV after an API storm): bursts of appendurl and append
    with one name, at once, while the incoming directory is scanned every second:
    the daemon stays up, every call gets an answer, and its state files read back
    after a restart."""
    import http.server
    import threading

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            body = _fake_nzb_ids(['%s-%d@srv' % (uuid.uuid4().hex, i) for i in range(20)], 15_000_000)
            time.sleep(random.random() * 0.3)
            self.send_response(200)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = 'http://127.0.0.1:%d' % server.server_address[1]
    daemon.wait_ready()
    answers = []

    def call(method, i):
        try:
            if method == 'appendurl':
                r = _rpc(daemon, 'appendurl', ['Storm-u.nzb', base + '/x%d' % i, '', 0, False, True, 'storm-key', i, 'SCORE', []], timeout=60)
            else:
                nzb = base64.standard_b64encode(_fake_nzb_ids(['%s@st' % uuid.uuid4().hex for _ in range(5)], 50_000)).decode()
                r = _rpc(daemon, 'append', ['Storm.nzb', nzb, '', 0, False, True, 'storm-key', i, 'SCORE', []], timeout=60)
            answers.append(r.get('result'))
        except Exception as e:
            answers.append('EXC %s' % str(e)[:40])
    for burst in range(6):
        threads = [threading.Thread(target=call, args=('appendurl' if i % 2 else 'append', burst * 100 + i)) for i in range(16)]
        for th in threads:
            th.start()
        for th in threads:
            th.join(timeout=90)
        time.sleep(random.random() * 2)
    time.sleep(10)
    server.shutdown()
    try:
        alive = bool(_rpc(daemon, 'version', [], timeout=10).get('result'))
    except Exception:
        alive = False
    exc = sum(1 for a in answers if isinstance(a, str))
    lost = sum(1 for a in answers if a == -1 or a == 0)
    return ('addstorm', alive and exc == 0 and lost == 0, 'alive=%s answers=%d exceptions=%d lost=%d' % (
        alive, len(answers), exc, sum(1 for a in answers if a == -1 or a == 0)))


def scenario_longstateline(daemon, t):
    """P0 (production SIGSEGV, queue and history unreadable): a field of 1,024
    characters or more in a state line - here a 5,000-character url (appendurl)
    and a 3,000-character dupe key - was written past a fixed buffer onto the
    stack: garbage in the history file (set aside as unreadable at the next
    start) or a crash. Now both items survive a restart and the files load."""
    api = daemon.wait_ready()
    long_url = 'http://127.0.0.1:9/' + 'a' * 5000
    url_id = _rpc(daemon, 'appendurl', ['Long.Url.nzb', long_url, '', 0, False, True, 'long-key', 0, 'SCORE', []]).get('result')
    nzb = base64.standard_b64encode(_fake_nzb_ids(['%s@ls' % uuid.uuid4().hex for _ in range(3)], 30_000)).decode()
    key_id = _rpc(daemon, 'append', ['Long.Key.nzb', nzb, '', 0, False, True, 'k' * 3000, 0, 'SCORE', []]).get('result')
    time.sleep(3)
    alive_before = bool(_rpc(daemon, 'version', [], timeout=10).get('result'))
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    # queued or failed by now, the url item is still there, with its url
    in_history = any(x['NZBID'] == url_id for x in api.history() + api.listgroups())
    in_queue = any(g['NZBID'] == key_id and len(g['DupeKey']) == 3000 for g in api.listgroups())
    unreadable = _grep_log(t, 'could not be read')
    ok = alive_before and in_history and in_queue and unreadable == 0
    return ('longstateline', ok, 'ids=%s/%s alive=%s url_kept=%s key_in_queue=%s unreadable_logs=%d' % (
        url_id, key_id, alive_before, in_history, in_queue, unreadable))


def scenario_newlinestate(daemon, t):
    """A line break in a name, dupe key or category sent through the API split
    its state record over two lines: at the next start the whole history (2,181
    items in production) was set aside as unreadable. Now the items survive a
    restart, the breaks kept as spaces."""
    api = daemon.wait_ready()
    nzb = base64.standard_b64encode(_fake_nzb_ids(['%s@nl' % uuid.uuid4().hex for _ in range(3)], 30_000)).decode()
    nid = _rpc(daemon, 'append', ['Line\nBreak.nzb', nzb, 'cat\negory', 0, False, True, 'key\r\nbreak', 0, 'SCORE', []]).get('result')
    hid = _rpc(daemon, 'append', ['Hist\nItem.nzb', nzb, '', 0, False, True, 'hist\nkey', 0, 'SCORE', []]).get('result')
    _rpc(daemon, 'editqueue', ['GroupDelete', '', [hid]])
    time.sleep(3)
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    queued = [g for g in api.listgroups() if g['NZBID'] == nid]
    in_history = any(h['NZBID'] == hid for h in api.history())
    unreadable = _grep_log(t, 'could not be read')
    ok = (nid > 0 and bool(queued) and '\n' not in queued[0]['DupeKey'] and in_history and unreadable == 0)
    return ('newlinestate', ok, 'ids=%s/%s queued=%s in_history=%s unreadable_logs=%d' % (
        nid, hid, [(g['NZBName'], g['DupeKey'], g['Category']) for g in queued], in_history, unreadable))


def scenario_idsafterunreadable(daemon, t):
    """P0-c: with the history set aside as unreadable at a start, the ids
    restarted at what loaded: production gave new fleets the ids of deleted test
    items. New ids now count on past every id the queue dir holds files for."""
    api = daemon.wait_ready()
    ids = []
    for i in range(4):
        nzb = base64.standard_b64encode(_fake_nzb_ids(['%s@id' % uuid.uuid4().hex for _ in range(3)], 30_000)).decode()
        ids.append(_rpc(daemon, 'append', ['Ids.%d.nzb' % i, nzb, '', 0, False, True, 'ids-key-%d' % i, 0, 'SCORE', []]).get('result'))
    _rpc(daemon, 'editqueue', ['GroupDelete', '', ids])
    time.sleep(3)
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    with open(t.path('main', 'queue', 'history'), 'r+b') as f:
        lines = f.read().split(b'\n')
        f.seek(0)
        f.truncate()
        f.write(b'\n'.join(lines[:1] + [b'garbage'] + lines[1:]))
    daemon.start()
    api = daemon.wait_ready()
    nzb = base64.standard_b64encode(_fake_nzb_ids(['%s@id' % uuid.uuid4().hex for _ in range(3)], 30_000)).decode()
    new_id = _rpc(daemon, 'append', ['Ids.new.nzb', nzb, '', 0, False, True, 'ids-key-new', 0, 'SCORE', []]).get('result')
    unreadable = _grep_log(t, 'could not be read')
    ok = unreadable >= 1 and new_id > max(ids)
    return ('idsafterunreadable', ok, 'old=%s new=%s unreadable_logs=%d' % (ids, new_id, unreadable))


def scenario_fleetscoremax(daemon, t):
    """F17: the key holds an item scored at the int limit (in history): the
    fleet's scores were counted past it and wrapped around, negative, so a dead
    member could outrank the whole one. The whole member now scores above the
    dead one, and no score is negative."""
    daemon.fake_nntp.alive = set(_fleet_ids('xw'))
    api = daemon.wait_ready()
    nzb = base64.standard_b64encode(_fake_nzb_ids(_fleet_ids('xm'), 400_000)).decode()
    top = _rpc(daemon, 'append', ['Show.S01E01.top.nzb', nzb, '', 0, False, True, 'fleet:max', 2147483647, 'SCORE', []]).get('result')
    _rpc(daemon, 'editqueue', ['GroupDelete', '', [top]])
    time.sleep(2)
    reply = _fleet(daemon, [('Show.S01E01.dead', _fake_nzb_ids(_fleet_ids('xd'), 400_000).decode()),
                            ('Show.S01E01.whole', _fake_nzb_ids(_fleet_ids('xw'), 410_000).decode())],
                   key='fleet:max', timeout=20)
    scores = {x['NZBName']: x['DupeScore'] for x in api.listgroups() + api.history()
              if x['NZBName'].startswith('Show.S01E01.') and x['NZBID'] != top}
    whole, dead = scores.get('Show.S01E01.whole'), scores.get('Show.S01E01.dead')
    ok = whole is not None and whole > 0 and (dead is None or (dead > 0 and whole > dead))
    return ('fleetscoremax', ok, 'scores=%s reply=%s' % (scores, [(m['Name'], m['Status']) for m in reply['Members']]))


def scenario_fleetmerged(daemon, t):
    """F20: two postings of a key merged into one item (GroupMerge) aren't added
    again when the fleet is resent: its members are the postings the merged
    item holds (SAME_POSTING). Before, a merged item's postings weren't seen."""
    daemon.fake_nntp.alive = set(_fleet_ids('ga')) | set(_fleet_ids('gb'))
    api = daemon.wait_ready()
    na = _fake_nzb_ids(_fleet_ids('ga'), 400_000)
    nb = _fake_nzb_ids(_fleet_ids('gb'), 410_000)
    a = _rpc(daemon, 'append', ['Show.S01E01.a.nzb', base64.standard_b64encode(na).decode(), '', 0, False, True, 'fleet:merge', 10, 'SCORE', []]).get('result')
    b = _rpc(daemon, 'append', ['Show.S01E01.b.nzb', base64.standard_b64encode(nb).decode(), '', 0, False, True, 'fleet:merge', 5, 'SCORE', []]).get('result')
    merged = _rpc(daemon, 'editqueue', ['GroupMerge', '', [a, b]]).get('result')
    reply = _fleet(daemon, [('Show.S01E01.a', na.decode()), ('Show.S01E01.b', nb.decode())], key='fleet:merge', timeout=20)
    groups = [g['NZBName'] for g in api.listgroups()]
    ok = merged and all(m['Status'] == 'SAME_POSTING' for m in reply['Members']) and len(groups) == 1
    return ('fleetmerged', ok, 'merged=%s groups=%s reply=%s' % (merged, groups, [(m['Name'], m['Status'], m.get('SameAs')) for m in reply['Members']]))


def scenario_readdafterdelete(daemon, t):
    """F26 (the proxy's silent drop, checked on nzbget's side): an item of a key is
    deleted, then the identical nzb-file is sent again under the key. nzbget skips
    it by design (upstream: the same content in history, whatever its status, is
    a duplicate); sent with DupeMode FORCE it queues. Nothing else - no id or
    state remembered by this branch - swallows it."""
    api = daemon.wait_ready()
    nzb = base64.standard_b64encode(_fake_nzb_ids(['%s@rd' % uuid.uuid4().hex for _ in range(5)], 50_000)).decode()
    first = _rpc(daemon, 'append', ['Readd.Release.nzb', nzb, '', 0, False, True, 'readd-key', 100, 'SCORE', []]).get('result')
    _rpc(daemon, 'editqueue', ['GroupDelete', '', [first]])
    time.sleep(2)
    skipped = _rpc(daemon, 'append', ['Readd.Release.nzb', nzb, '', 0, False, True, 'readd-key', 100, 'SCORE', []]).get('result')
    forced = _rpc(daemon, 'append', ['Readd.Release.nzb', nzb, '', 0, False, True, 'readd-key', 100, 'FORCE', []]).get('result')
    time.sleep(2)
    queued = {g['NZBID'] for g in api.listgroups()}
    ok = skipped > 0 and skipped not in queued and _grep_log(t, 'Skipping duplicate Readd.Release') >= 1 and forced in queued
    return ('readdafterdelete', ok, 'first=%s skipped=%s forced=%s queued=%s' % (first, skipped, forced, sorted(queued)))


def scenario_editscorerange(daemon, t):
    """F27: editqueue GroupSetDupeScore with a score past the int range wrapped
    around (2147483648 stored as -2147483648, the lowest score); it is an invalid
    parameter now, the score unchanged, and a parameter without a name too, a
    parameter without a value, and an unknown dupe mode."""
    api = daemon.wait_ready()
    nzb = base64.standard_b64encode(_fake_nzb_ids(['%s@es' % uuid.uuid4().hex for _ in range(3)], 30_000)).decode()
    nid = _rpc(daemon, 'append', ['Edit.Score.nzb', nzb, '', 0, False, True, 'es-key', 500, 'SCORE', []]).get('result')
    replies = [_rpc(daemon, 'editqueue', ['GroupSetDupeScore', v, [nid]]) for v in ('2147483648', '-2147483649')]
    param = _rpc(daemon, 'editqueue', ['GroupSetParameter', '=y', [nid]])
    # a parameter without a value and an unknown dupe mode: refused by the edit
    # (logged), but the call answered true
    noequals = _rpc(daemon, 'editqueue', ['GroupSetParameter', 'novalue', [nid]])
    badmode = _rpc(daemon, 'editqueue', ['GroupSetDupeMode', 'BOGUS', [nid]])
    goodmode = _rpc(daemon, 'editqueue', ['GroupSetDupeMode', 'all', [nid]]).get('result')
    good = _rpc(daemon, 'editqueue', ['GroupSetDupeScore', '700', [nid]]).get('result')
    group = next((g for g in api.listgroups() if g['NZBID'] == nid), {})
    score, mode = group.get('DupeScore'), group.get('DupeMode')
    rejected = all(not r.get('result') for r in replies + [param, noequals, badmode])
    ok = rejected and good and goodmode and score == 700 and mode == 'ALL'
    return ('editscorerange', ok, 'replies=%s param=%s noequals=%s badmode=%s good=%s/%s score=%s mode=%s' % (
        [r.get('result', r.get('error')) for r in replies], param.get('result', param.get('error')),
        noequals.get('result', noequals.get('error')), badmode.get('result', badmode.get('error')),
        good, goodmode, score, mode))


def scenario_fleetsamekey(daemon, t):
    """F28: two fleets of one key at once, with different postings, while a
    download holds the connections. Before, both queued a pick, tied at the top
    score: two downloads of one release. The second waits for the first and adds
    its members as backups below the first's pick."""
    import threading
    daemon.fake_nntp.alive = set(_fleet_ids('sb', 600)) | set(_fleet_ids('s1')) | set(_fleet_ids('s2'))
    daemon.fake_nntp.delays.update({'sb-': 3.0})
    api = daemon.wait_ready()
    _ds_append(api, 'Busy.Download', _fake_nzb_ids(_fleet_ids('sb', 600), 6_000_000).decode(), 'busy-key', 100, paused=False)
    time.sleep(3)
    box = {}

    def send(slot, tag, key):
        box[slot] = _fleet(daemon, [('Show.S02E08.%s.whole' % tag, _fake_nzb_ids(_fleet_ids(tag), 410_000).decode()),
                                    ('Show.S02E08.%s.dead' % tag, _fake_nzb_ids(_fleet_ids(tag + 'd'), 400_000).decode())],
                           key=key, timeout=20)
    threads = [threading.Thread(target=send, args=('1', 's1', 'fleet:same-key')),
               threading.Thread(target=send, args=('2', 's2', 'Fleet:Same-Key'))]
    for th in threads:
        th.start()
    for th in threads:
        th.join(timeout=90)
    groups = [g for g in api.listgroups() if g['DupeKey'].lower() == 'fleet:same-key']
    scores = sorted((g['DupeScore'] for g in groups), reverse=True)
    ok = len(groups) == 1
    return ('fleetsamekey', ok, 'queued=%s replies=%s' % (
        [(g['NZBName'], g['DupeScore'], g['Status']) for g in groups],
        [(box.get(k, {}).get('Chosen'), box.get(k, {}).get('Reason'), box.get(k, {}).get('Complete')) for k in ('1', '2')]))


def scenario_fleetwide(daemon, t):
    """F30: a fleet of 40 dead postings, the server slow to answer, a 20 s limit:
    the reply comes within the limit (production: 48 members, 30 s limit, 85 s),
    Complete false, one member queued."""
    daemon.fake_nntp.alive = set()
    daemon.fake_nntp.delays.update({'fw': 0.5})
    daemon.wait_ready()
    members = [('Show.S01E01.v%02d' % i, _fake_nzb_ids(_fleet_ids('fw%02d' % i), 400_000).decode()) for i in range(40)]
    start = time.time()
    reply = _fleet(daemon, members, key='fleet:wide', timeout=20, http_timeout=200)
    took = time.time() - start
    ok = took < 23 and len(reply['Members']) == 40
    return ('fleetwide', ok, 'took=%.1f complete=%s chosen=%s statuses=%s' % (
        took, reply['Complete'], reply['Chosen'], sorted(set(m['Status'] for m in reply['Members']))))


def scenario_fleetslowurlfirst(daemon, t):
    """F29: both members are urls, the slow one (40 s) listed first, a 10 s limit.
    The urls were fetched one after another: the slow one used up the limit and
    the good one was never fetched (both ERROR, nothing chosen). Fetched at once,
    the good one is queued within the limit."""
    import threading
    from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
    good = _fake_nzb_ids(_fleet_ids('sf'), 400_000)

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path.startswith('/slow'):
                time.sleep(40)
            self.send_response(200)
            self.send_header('Content-Length', str(len(good)))
            self.end_headers()
            self.wfile.write(good)

        def log_message(self, *args):
            pass
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    daemon.fake_nntp.alive = set(_fleet_ids('sf'))
    daemon.wait_ready()
    base = 'http://127.0.0.1:%d' % server.server_address[1]
    start = time.time()
    reply = _rpc(daemon, 'appendfleet', ['fleet:slowfirst', 'test', 0, 10,
                                         'Show.S01E01.slow', base + '/slow.nzb', 'Show.S01E01.good', base + '/good.nzb'],
                 timeout=60).get('result', {})
    took = time.time() - start
    byname = {m['Name']: m for m in reply.get('Members', [])}
    ok = took < 12 and byname.get('Show.S01E01.good', {}).get('Status') == 'QUEUED' and \
        byname.get('Show.S01E01.slow', {}).get('Status') == 'ERROR'
    return ('fleetslowurlfirst', ok, 'took=%.1f members=%s' % (took, [(m['Name'], m['Status']) for m in reply.get('Members', [])]))


def scenario_dupesearchgzip(daemon, t):
    """B91: the indexer answers gzip-compressed, without a length (as NZBHydra2
    does for nzbget's "Accept-Encoding: gzip"): the search still reads its
    results. Production logged "isn't XML" for every search."""
    daemon.newznab.gzip = True
    _, ok, detail = scenario_dupesearchsearch(daemon, t)
    not_xml = _grep_log(t, "isn't XML")
    return ('dupesearchgzip', ok and not_xml == 0, 'not_xml_logs=%d %s' % (not_xml, detail))


def scenario_appendurlgzip(daemon, t):
    """B91: a url answered gzip-compressed without a length (as NZBHydra2 does),
    the body compressing about 20 to 1 like a real search answer: nzbget reads
    all of it. A compressed read that expanded past the decompressor's buffer
    lost the rest."""
    import gzip as _gzip
    import http.server
    import threading
    segs = ''.join('<segment bytes="750000" number="%d">%s-%d@gz.test</segment>' % (i + 1, 'x' * 40, i) for i in range(4000))
    body = ('<?xml version="1.0" encoding="UTF-8"?><nzb xmlns="http://www.newzbin.com/DTD/2003/nzb">'
            '<file poster="p" date="1" subject="&quot;gz.bin&quot; yEnc (1/4000)"><groups><group>a.b</group></groups>'
            '<segments>%s</segments></file></nzb>' % segs).encode()
    packed = _gzip.compress(body)

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200)
            self.send_header('Content-Encoding', 'gzip')
            self.end_headers()
            self.wfile.write(packed)

        def log_message(self, *args):
            pass
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    api = daemon.wait_ready()
    nid = _rpc(daemon, 'appendurl', ['Gz.nzb', 'http://127.0.0.1:%d/gz.nzb' % server.server_address[1],
                                     '', 0, False, True, 'gz-key', 0, 'SCORE', []]).get('result')
    deadline = time.time() + 60
    group = None
    while time.time() < deadline:
        group = next((g for g in api.listgroups() if g['NZBName'] == 'Gz' and g['Kind'] == 'NZB'), None)
        if group or _grep_log(t, 'Gz'):
            if group:
                break
        time.sleep(0.5)
    server.shutdown()
    articles = group['RemainingSizeMB'] if group else None
    ok = group is not None and group['FileSizeMB'] >= 2800
    return ('appendurlgzip', ok, 'compressed=%d plain=%d group=%s errors=%d' % (
        len(packed), len(body), group and (group['FileSizeMB'], group['RemainingFileCount']), _grep_log(t, 'Error parsing')))



def scenario_dupesearchbareurl(daemon, t):
    """B91: DupeSearchUrl is the indexer's address without "/api"
    (http://host:5076): its web page answers every search with HTML, and every
    search was "isn't XML". nzbget asks the standard Newznab path "/api"."""
    inner = daemon.newznab.respond
    daemon.newznab.respond = lambda params, path: (200, b'<!doctype html><html><body>indexer</body></html>') \
        if path.rstrip('/') != '/api' else inner(params, path)
    _, ok, detail = scenario_dupesearchsearch(daemon, t)
    not_xml = _grep_log(t, "isn't XML")
    return ('dupesearchbareurl', ok and not_xml == 0, 'not_xml_logs=%d %s' % (not_xml, detail))


def scenario_appendlongname(daemon, t):
    """A name of 600 characters (a fleet member's, or an append's): the nzb-file
    couldn't be created past the file system's 255 bytes and the download was
    refused (ERROR, -1). It is added under its full name now."""
    api = daemon.wait_ready()
    nzb = base64.standard_b64encode(_fake_nzb_ids(['%s@ln' % uuid.uuid4().hex for _ in range(3)], 30_000)).decode()
    name = 'Long.' + 'n\u00e9' * 300 + '.nzb'
    nid = _rpc(daemon, 'append', [name, nzb, '', 0, False, True, 'ln-key', 0, 'SCORE', []]).get('result')
    group = next((g for g in api.listgroups() if g['NZBID'] == nid), None) if isinstance(nid, int) and nid > 0 else None
    ok = group is not None and group['NZBName'].startswith('Long.n') and len(group['NZBName']) > 255
    return ('appendlongname', ok, 'id=%s name_len=%s' % (nid, group and len(group['NZBName'])))


def scenario_apiedges(daemon, t):
    """API edge cases (all upstream too):
    rate past INT_MAX/1024 is rejected (it wrapped to a 1 KB/s limit);
    listgroups' PostStageProgress is 0 for a queued item (it printed a pointer);
    a post-processing parameter with a quote and a backslash is stored as sent
    (it was stored escaped: a password with them failed to unpack);
    saveconfig with an empty or malformed list is refused and the file keeps every
    option (it was rewritten without them, reporting success); with "Value"
    before "Name" the right option is saved."""
    api = daemon.wait_ready()
    rate = _rpc(daemon, 'rate', [4194305])
    limit = api.status()['DownloadLimit']
    nzb = base64.standard_b64encode(_fake_nzb_ids(['%s@ae' % uuid.uuid4().hex for _ in range(3)], 30_000)).decode()
    nid = _rpc(daemon, 'append', ['Api.Edges.nzb', nzb, '', 0, False, True, 'ae-key', 0, 'SCORE',
                                  [{'*Unpack:Password': 'pa"ss\\x'}]]).get('result')
    group = next((g for g in api.listgroups() if g['NZBID'] == nid), {})
    password = {p['Name']: p['Value'] for p in group.get('Parameters', [])}.get('*Unpack:Password')
    conf_path = t.path(daemon.conf_rel)
    before = open(conf_path).read()
    empty = _rpc(daemon, 'saveconfig', [[]])
    bad = _rpc(daemon, 'saveconfig', [[{'Name': 'ControlPort', 'Value': 1}, {'Name': 'NzbDirInterval', 'Value': '7'}]])
    kept = open(conf_path).read() == before
    swapped = _rpc(daemon, 'saveconfig', [[{'Value': '9', 'Name': 'NzbDirInterval'}]])
    after = open(conf_path).read()
    ok = (not rate.get('result') and limit == 0 and group.get('PostStageProgress') == 0 and password == 'pa"ss\\x' and
          'error' in empty and 'error' in bad and kept and swapped.get('result') and 'NzbDirInterval=9' in after)
    # (saveconfig replaces the whole option list, as the web interface sends every
    # option: the swapped-key call leaves only NzbDirInterval, by design)
    return ('apiedges', ok, 'rate=%s limit=%s progress=%s password=%r empty=%s bad=%s kept=%s swapped=%s interval9=%s' % (
        rate.get('result', rate.get('error')), limit, group.get('PostStageProgress'), password,
        'error' in empty, 'error' in bad, kept, swapped.get('result'), 'NzbDirInterval=9' in after))


def scenario_apiaccess(daemon, t):
    """Access and request-size edges (upstream too). A restricted user's
    system.multicall ran its calls with full control access: config returned the
    control password in plain text, where a direct call masks it. A POST that
    declares a 2 GB body crashed the server (the buffer couldn't be allocated and
    was written through anyway); it is refused now and nzbget stays up."""
    import socket
    daemon.creds = 'nzbget:ctlpass'
    daemon.wait_ready()

    def post(path, body, auth, length=None):
        sock = socket.create_connection(('127.0.0.1', daemon.rpc_port), timeout=10)
        head = ('POST %s HTTP/1.1\r\nHost: x\r\nAuthorization: Basic %s\r\nContent-Length: %d\r\n\r\n' %
                (path, base64.b64encode(auth.encode()).decode(), len(body) if length is None else length))
        sock.sendall(head.encode() + body)
        data = b''
        try:
            while True:
                chunk = sock.recv(65536)
                if not chunk:
                    break
                data += chunk
        except socket.timeout:
            pass
        sock.close()
        return data
    call = (b'<?xml version="1.0"?><methodCall><methodName>system.multicall</methodName><params><param><value>'
            b'<array><data><value><struct><member><name>methodName</name><value><string>config</string></value>'
            b'</member><member><name>params</name><value><array><data></data></array></value></member></struct>'
            b'</value></data></array></value></param></params></methodCall>')
    reply = post('/xmlrpc', call, 'ro:ropass')
    leaked = b'ctlpass' in reply
    masked = b'***' in reply
    huge = post('/jsonrpc', b'{}', 'nzbget:ctlpass', length=2147483647)
    time.sleep(1)
    try:
        alive = bool(daemon.api().version())
    except Exception:
        alive = False
    ok = not leaked and masked and b' 400' in huge.split(b'\r\n', 1)[0] and alive
    return ('apiaccess', ok, 'leaked=%s masked=%s huge_status=%r alive=%s' % (
        leaked, masked, huge.split(b'\r\n', 1)[0][:40], alive))


def scenario_queueedits(daemon, t):
    """Queue edits and state (upstream too): an item's DupeMode survives a restart
    (the hint was loaded into it: FORCE came back as SCORE); GroupMerge with an id
    given twice doesn't use the item it already freed and reports success (it
    reported false on success); FileDelete with a file given twice doesn't crash;
    FileSplit of files from two collections is refused (it crashed)."""
    api = daemon.wait_ready()

    def add(name, n, mode='SCORE'):
        nzb = base64.standard_b64encode(_many_files_nzb(n, uuid.uuid4().hex[:8])).decode()
        return _rpc(daemon, 'append', [name + '.nzb', nzb, '', 0, False, True, 'qe-' + name, 0, mode, []]).get('result')
    forced = add('Forced', 1, 'FORCE')
    a, b, c, d = add('MergeA', 2), add('MergeB', 2), add('SplitC', 2), add('SplitD', 2)
    merged = _rpc(daemon, 'editqueue', ['GroupMerge', '', [a, b, b]]).get('result')
    files_c = [f['ID'] for f in api.listfiles(0, 0, c)]
    files_d = [f['ID'] for f in api.listfiles(0, 0, d)]
    deleted = _rpc(daemon, 'editqueue', ['FileDelete', '', [files_c[0], files_c[0]]]).get('result')
    split = _rpc(daemon, 'editqueue', ['FileSplit', 'Mixed', [files_c[1], files_d[0]]]).get('result')
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    groups = {g['NZBID']: g for g in api.listgroups()}
    mode = groups.get(forced, {}).get('DupeMode')
    ok = mode == 'FORCE' and merged is True and deleted is True and split is False and b not in groups
    return ('queueedits', ok, 'mode_after_restart=%s merged=%s deleted=%s split=%s groups=%d' % (
        mode, merged, deleted, split, len(groups)))


def scenario_authrejected(daemon, t):
    """A second level-0 server refuses every login with 502 (a broken account),
    and an article is missing on the first: the article counts as failed on the
    refusing server and the download completes. It waited for that server for
    good, and every download that needed it stalled (production: 0 KB/s for over
    an hour with eight working servers)."""
    size, seg = 2_000_000, 500_000
    pp = _place_copy(t, 'arA', _payload(size, 7878), 'f.bin')
    api = daemon.wait_ready()
    daemon.append(api, 'RelAR', build_nzb(pp, 'ar.bin', size, seg, {2}), False, 'ar-key', 100)
    deadline = time.time() + 90
    h = None
    while time.time() < deadline:
        h = next((x for x in api.history() if x['NZBName'] == 'RelAR'), None)
        if h:
            break
        time.sleep(0.5)
    logins = daemon.reject_nntp.logins
    ok = h is not None and logins >= 1
    return ('authrejected', ok, 'completed=%s status=%s refused_logins=%d' % (h is not None, h and h['Status'], logins))


def scenario_staleprogress(daemon, t):
    """The progress file names a collection the queue no longer holds (nzbget
    stopped between saving the queue and discarding the progress file): the
    queue loads anyway. The whole load was aborted ("NZB with id N could not
    be found"), and queue and history were set aside as unreadable."""
    api = daemon.wait_ready()
    _ds_append(api, 'Kept.Item', _fake_nzb_ids(_fleet_ids('kp', 5), 50_000).decode(), 'kp-key', 100)
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    # a progress record is the collection's id, then its full record as in the queue
    # file: the queue's only record, under an id the queue doesn't have
    queue_lines = open(t.path('main', 'queue', 'queue')).read().split('\n')
    open(t.path('main', 'queue', 'progress'), 'w').write('\n'.join(queue_lines[:1] + ['1', '99999'] + queue_lines[2:]))
    daemon.start()
    api = daemon.wait_ready()
    groups = {g['NZBName'] for g in api.listgroups()}
    unreadable = _grep_log(t, 'could not be read')
    ok = unreadable == 0 and 'Kept.Item' in groups
    return ('staleprogress', ok, 'unreadable_logs=%d groups=%s skipped_logs=%d' % (
        unreadable, sorted(groups), _grep_log(t, 'no longer queued')))


def scenario_apiformat(daemon, t):
    """Response and GET edge cases (upstream too): a GET parameter holding an encoded
    "&" (%26) stays one value (the whole query was decoded before it was split);
    servervolumes with some arrays turned off is valid JSON (stray and doubled
    commas); a JSON-RPC id over 99 characters comes back whole (it was cut, and the
    response wasn't valid JSON)."""
    import json as _json
    import urllib.request as _req
    api = daemon.wait_ready()
    base = 'http://127.0.0.1:%d' % daemon.rpc_port
    import http.server
    import threading
    import urllib.parse as _parse

    class Echo(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            body = self.path.encode()
            self.send_response(200)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass
    echo = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Echo)
    echo.daemon_threads = True
    threading.Thread(target=echo.serve_forever, daemon=True).start()
    target = 'http://127.0.0.1:%d/p?x=1&y=2' % echo.server_address[1]
    # (GET is only for read-only methods: readurl takes a string)
    with _req.urlopen(base + '/jsonrpc/readurl?1=%s&2=echo' % _parse.quote(target, safe=''), timeout=20) as r:
        fetched = _json.loads(r.read().decode()).get('result')
    echo.shutdown()
    valid = True
    for flags in ([False, True, False, True, False], [True, True, True, False, True], [False, False, False, False, False]):
        body = _json.dumps({'method': 'servervolumes', 'params': flags}).encode()
        with _req.urlopen(_req.Request(base + '/jsonrpc', body), timeout=10) as r:
            try:
                _json.loads(r.read().decode())
            except ValueError:
                valid = False
    long_id = 'i' * 300
    body = _json.dumps({'method': 'version', 'params': [], 'id': long_id}).encode()
    with _req.urlopen(_req.Request(base + '/jsonrpc', body), timeout=10) as r:
        try:
            echoed = _json.loads(r.read().decode()).get('id')
        except ValueError:
            echoed = None
    ok = fetched == '/p?x=1&y=2' and valid and echoed == long_id
    return ('apiformat', ok, 'fetched=%r volumes_valid=%s id_echoed=%s' % (fetched, valid, echoed == long_id))


def scenario_directunpackkeep(daemon, t):
    """DirectUnpack=yes with UseTempUnpackDir=no: a direct unpack that fails (an
    encrypted rar sent without its password) leaves the downloaded files alone.
    Its cleanup deleted the unpack folder with its content - here the download's
    own folder - so every volume downloaded so far vanished mid-download."""
    data = _payload(4_000_000, 8080)
    volumes = generators.rar3_store_volumes_encrypted('movie.mkv', data, 1_400_000, 'secretpw')
    members = []
    for i, vol in enumerate(volumes, 1):
        rel = 'duA/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        members.append((rel, 'Rel.part%02d.rar' % i, len(vol), 500_000, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'RelDU', build_multi_nzb(members), False, 'du-key', 100)
    # the files right after the direct unpack failed, while the download goes on
    deadline = time.time() + 120
    while time.time() < deadline and _grep_log(t, 'Direct unpack for RelDU failed') == 0:
        time.sleep(0.1)
    at_failure = sorted(os.path.basename(rel) for rel in t.find_files('main') if re.search(r'Rel\.part\d+\.rar', rel))
    h = daemon.wait_history(api, 'RelDU', timeout=240)
    kept = sorted(os.path.basename(rel) for rel in t.find_files('main') if re.search(r'Rel\.part\d+\.rar$', rel))
    ok = len(kept) == len(volumes) and 'Rel.part01.rar' in at_failure
    return ('directunpackkeep', ok, 'status=%s at_failure=%s volumes_on_disk=%s of %d' % (
        h['Status'], at_failure, len(kept), len(volumes)))


def scenario_jointwosets(daemon, t):
    """Two sets of split files in one download (a.mkv.001-003, b.srt.001-002): both
    are joined. The fragment check counted the other set's pieces too, and the join
    failed with "missing fragments detected" (upstream too)."""
    a = _payload(2_500_000, 8181)
    b = _payload(150_000, 8182)
    pieces = [('a.mkv', a, 1_000_000), ('b.srt', b, 100_000)]
    members = []
    for name, data, size in pieces:
        for i, start in enumerate(range(0, len(data), size), 1):
            rel = 'jtA/%s.%03d' % (name, i)
            chunk = data[start:start + size]
            t.write_file(os.path.join('data', rel), chunk)
            members.append((rel, '%s.%03d' % (name, i), len(chunk), 500_000, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'RelJT', build_multi_nzb(members), False, 'jt-key', 100)
    h = daemon.wait_history(api, 'RelJT', timeout=180)
    joined = {}
    for rel in t.find_files('main'):
        base = os.path.basename(rel)
        # (the subtitle is renamed after the video by post-download renaming: a.srt)
        if base in ('a.mkv', 'b.srt', 'a.srt'):
            joined['a.mkv' if base == 'a.mkv' else 'srt'] = t.read_file(rel) == (a if base == 'a.mkv' else b)
    ok = joined.get('a.mkv') is True and joined.get('srt') is True and h['Status'].startswith('SUCCESS')
    return ('jointwosets', ok, 'status=%s joined=%s missing_logs=%d' % (
        h['Status'], joined, _grep_log(t, 'missing fragments detected')))


def scenario_fleetpaused(daemon, t):
    """appendfleet (F12): with downloads paused by the user, a fleet is still
    checked - the check doesn't download - and ranked: the whole copy is
    chosen, the dead one measured dead; the queue stays paused."""
    daemon.fake_nntp.alive = set(_fleet_ids('pw'))
    api = daemon.wait_ready()
    api.pausedownload()
    reply = _fleet(daemon, [('Show.S01E01.dead', _fake_nzb_ids(_fleet_ids('pd'), 400_000).decode()),
                            ('Show.S01E01.whole', _fake_nzb_ids(_fleet_ids('pw'), 410_000).decode())],
                   key='fleet:paused', timeout=20)
    byname = {m['Name']: m for m in reply['Members']}
    ok = (reply['Complete'] and byname['Show.S01E01.whole']['Status'] == 'QUEUED' and
          byname['Show.S01E01.dead']['Status'] == 'DEAD' and api.status()['DownloadPaused'])
    return ('fleetpaused', ok, 'reply=%s' % [(m['Name'], m['Status'], m['Alive']) for m in reply['Members']])


def scenario_fleetparallel(daemon, t):
    """appendfleet (F11): two fleets for different keys at once, while a long
    download holds both connections. One fleet's check runs at a time; the other
    waits within its own limit: both finish well inside 20 s with every member
    measured. Before, they shared the connections and both used the whole limit
    with members unmeasured."""
    import threading
    daemon.fake_nntp.alive = set(_fleet_ids('pb', 600)) | set(_fleet_ids('p1')) | set(_fleet_ids('p2'))
    daemon.fake_nntp.delays.update({'pb-': 3.0})
    api = daemon.wait_ready()
    _ds_append(api, 'Busy.Download', _fake_nzb_ids(_fleet_ids('pb', 600), 6_000_000).decode(), 'busy-key', 100, paused=False)
    time.sleep(3)
    box = {}

    def send(slot, tag):
        start = time.time()
        box[slot] = _fleet(daemon, [('Show.S0%sE01.dead' % slot, _fake_nzb_ids(_fleet_ids(tag + 'd'), 400_000).decode()),
                                    ('Show.S0%sE01.whole' % slot, _fake_nzb_ids(_fleet_ids(tag), 410_000).decode())],
                           key='fleet:par%s' % slot, timeout=20)
        box[slot + 'took'] = time.time() - start
    threads = [threading.Thread(target=send, args=(slot, tag)) for slot, tag in (('1', 'p1'), ('2', 'p2'))]
    for th in threads:
        th.start()
    for th in threads:
        th.join(timeout=90)
    ok = all(box.get(slot, {}).get('Complete') and box.get(slot + 'took', 99) < 15 and
             all(m['Alive'] >= 0 for m in box[slot]['Members']) for slot in ('1', '2'))
    return ('fleetparallel', ok, 'took=%.1f/%.1f complete=%s/%s' % (box.get('1took', -1), box.get('2took', -1),
            box.get('1', {}).get('Complete'), box.get('2', {}).get('Complete')))


def scenario_fleetslowurl(daemon, t):
    """appendfleet (F13): a member's url answers only after 40 s; the fleet
    allows 10 s. The call still returns within its limit (the fetch is stopped
    at the deadline), the good member queued and the slow one an error."""
    import threading
    from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

    class Slow(BaseHTTPRequestHandler):
        def do_GET(self):
            time.sleep(40)
            self.send_response(200)
            self.end_headers()

        def log_message(self, *args):
            pass
    server = ThreadingHTTPServer(('127.0.0.1', 0), Slow)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    daemon.fake_nntp.alive = set(_fleet_ids('su'))
    daemon.wait_ready()
    import json as _json
    import urllib.request as _req
    params = ['fleet:slowurl', 'test', 0, 10,
              'Show.S01E01.slow', 'http://127.0.0.1:%d/slow.nzb' % server.server_address[1],
              'Show.S01E01.good', base64.standard_b64encode(_fake_nzb_ids(_fleet_ids('su'), 400_000)).decode()]
    start = time.time()
    request = _req.Request('http://127.0.0.1:%d/jsonrpc' % daemon.rpc_port,
                           _json.dumps({'method': 'appendfleet', 'params': params}).encode(),
                           {'Content-Type': 'application/json'})
    with _req.urlopen(request, timeout=60) as reply:
        result = _json.loads(reply.read().decode())['result']
    took = time.time() - start
    server.shutdown()
    byname = {m['Name']: m for m in result['Members']}
    ok = took < 12 and byname['Show.S01E01.good']['Status'] == 'QUEUED' and byname['Show.S01E01.slow']['Status'] == 'ERROR'
    return ('fleetslowurl', ok, 'took=%.1f members=%s' % (took, [(m['Name'], m['Status']) for m in result['Members']]))


def scenario_fleetone(daemon, t):
    """appendfleet with a single whole member: it is queued and downloads."""
    daemon.fake_nntp.alive = set(_fleet_ids('one'))
    api = daemon.wait_ready()
    reply = _fleet(daemon, [('Show.S01E01.one', _fake_nzb_ids(_fleet_ids('one'), 400_000).decode())])
    h = daemon.wait_history(api, 'Show.S01E01.one', timeout=120)
    ok = reply['Chosen'] > 0 and reply['Members'][0]['Status'] == 'QUEUED' and h['Status'].startswith('SUCCESS')
    return ('fleetone', ok, 'reply=%s status=%s' % (reply, h['Status']))


def scenario_fleetalldead(daemon, t):
    """appendfleet: every member is dead: nothing is queued, the reply says ALL_DEAD."""
    daemon.fake_nntp.alive = set()
    api = daemon.wait_ready()
    reply = _fleet(daemon, [('Show.S01E01.d1', _fake_nzb_ids(_fleet_ids('d1'), 400_000).decode()),
                            ('Show.S01E01.d2', _fake_nzb_ids(_fleet_ids('d2'), 410_000).decode())])
    time.sleep(2)
    queued = [g['NZBName'] for g in api.listgroups()]
    ok = reply['Chosen'] == 0 and reply['Reason'] == 'ALL_DEAD' and not queued and \
        all(m['Status'] == 'DEAD' for m in reply['Members'])
    return ('fleetalldead', ok, 'reply=%s queued=%s' % (reply, queued))


def scenario_fleetshutdown(daemon, t):
    """appendfleet: nzbget shuts down while it checks a fleet (slow server): the
    call ends, nothing is half-added, and nzbget starts again cleanly."""
    import threading
    daemon.fake_nntp.alive = set(_fleet_ids('sd'))
    daemon.fake_nntp.delays.update({'sd-': 2.0})
    api = daemon.wait_ready()
    box = {}

    def send():
        try:
            box['reply'] = _fleet(daemon, [('Show.S01E01.sd', _fake_nzb_ids(_fleet_ids('sd'), 400_000).decode())],
                                  timeout=60, http_timeout=90)
        except Exception as e:
            box['error'] = type(e).__name__
    thread = threading.Thread(target=send)
    thread.start()
    time.sleep(3)
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    thread.join(timeout=30)
    daemon.fake_nntp.delays.clear()
    daemon.start()
    api = daemon.wait_ready()
    queued = [g['NZBName'] for g in api.listgroups()]
    history = [h['NZBName'] for h in api.history()]
    ok = not thread.is_alive() and not queued and not history
    return ('fleetshutdown', ok, 'reply=%s queued=%s history=%s' % (box, queued, history))


def scenario_dupesearchwarnings(daemon, t):
    """Settings that hobble DupeSearch are warned about at start: HealthCheck
    isn't dupe (its duplicates wait for a failure), and a server with fewer than
    4 connections leaves its health checks 1."""
    daemon.wait_ready()
    health = _grep_log(t, "'DupeSearch' is enabled while 'HealthCheck' isn't 'Dupe'")
    conns = _grep_log(t, "DupeSearch's health checks get only 1 connection")
    return ('dupesearchwarnings', health == 1 and conns == 1, 'healthcheck_warnings=%d connection_warnings=%d' % (health, conns))


def scenario_dupesearchwarnnocheck(daemon, t):
    """DupeSearch with DupeCheck=no is warned about: nothing would be searched."""
    daemon.wait_ready()
    warned = _grep_log(t, "'DupeSearch' is enabled while 'DupeCheck' is disabled")
    return ('dupesearchwarnnocheck', warned == 1, 'warnings=%d' % warned)


def scenario_dupesearchambiguous(daemon, t):
    """Industry S04E02: two postings of exactly the same size share the name; one
    (the pick) is alive, its twin dead. The pick is read from its own nzb-file,
    so the twin's verdict never touches it: the pick keeps its score and isn't
    called dead; the twin, a backup, is marked dead."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    api = _ds_donor_env(daemon, t, {}, set())
    _ds_append(api, DS_TITLE, _fake_nzb_ids(ids('tw'), 400_000).decode(), DS_KEY, DS_PICK - 1)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(2)
    pick = _ds_group(api, DS_TITLE)
    twin = [h for h in api.history() if h.get('NZBName') == DS_TITLE]
    alive = {p['Name']: p['Value'] for p in twin[0].get('Parameters', [])}.get('DupeAlive') if twin else None
    called_dead = _grep_log(t, 'the pick is dead')
    ok = pick is not None and pick.get('DupeScore') == DS_PICK and called_dead == 0 and alive == '0'
    return ('dupesearchambiguous', ok, 'pick_score=%s pick_called_dead=%d twin_alive=%s'
            % (pick and pick.get('DupeScore'), called_dead, alive))


def scenario_dupesearchrestartcheck(daemon, t):
    """nzbget restarts (a clean shutdown) while a search checks its postings: after
    the restart the search resumes, and each posting arrives exactly once."""
    ids = lambda p: ['%s-%d@x' % (p, i) for i in range(40)]
    postings = {'one': (ids('ro'), 410_000, 1, 11), 'two': (ids('rt'), 420_000, 1, 12)}
    daemon.fake_nntp.delays.update({'ro-': 1.0, 'rt-': 1.0})
    api = _ds_donor_env(daemon, t, postings, set(ids('ro')) | set(ids('rt')))
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, 'result(s) from') == 0:
        time.sleep(0.2)
    time.sleep(3)
    before = _grep_log(t, ' added=')
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    daemon.fake_nntp.delays.clear()
    daemon.start()
    api = daemon.wait_ready()
    deadline = time.time() + 90
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(2)
    donors = [h for h in api.history() if h.get('NZBName') == DS_TITLE] + \
        [g for g in api.listgroups() if g['NZBName'] == DS_TITLE and g.get('DupeScore') != DS_PICK]
    resumed = _grep_log(t, 'resuming the search of')
    ok = before == 0 and resumed == 1 and len(donors) == 2
    return ('dupesearchrestartcheck', ok, 'summary_before_restart=%d resumed=%d donors=%d (want 2)'
            % (before, resumed, len(donors)))


def scenario_dupesearchkeychanged(daemon, t):
    """B17: the pick gets another DupeKey while its search checks the postings:
    no donor is added under the old key (it would match nothing and download
    beside the pick)."""
    api, pick, donors = _ds_interrupt_pick(daemon, t, 'key')
    stopped = _grep_log(t, 'the duplicate key of the pick changed')
    ok = not donors and stopped >= 1
    return ('dupesearchkeychanged', ok, 'donors=%d stopped_logs=%d' % (len(donors), stopped))


def scenario_dupesearchresumedeleted(daemon, t):
    """B21: a donor the user deleted from history before a crash (kept as a
    hidden duplicate record) is not added again when the search resumes after
    the restart; the other postings still are."""
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
    fast = [h for h in api.history() if h.get('NZBName') == DS_TITLE]
    deleted = fast[0]['NZBID'] if fast else 0
    api.editqueue('HistoryDelete', '', [deleted])
    time.sleep(0.5)
    t.procs[-1].kill()
    t.procs[-1].wait()
    daemon.fake_nntp.delays.clear()
    daemon.start()
    api = daemon.wait_ready()
    deadline = time.time() + 90
    while time.time() < deadline and _grep_log(t, ' added=') == 0:
        time.sleep(0.5)
    time.sleep(1)
    donors = [h for h in api.history() if h.get('NZBName') == DS_TITLE] + \
        [g for g in api.listgroups() if g['NZBName'] == DS_TITLE and g.get('DupeScore') != DS_PICK]
    resumed = _grep_log(t, 'resuming the search of')
    ok = len(fast) == 2 and resumed == 1 and len(donors) == 3
    return ('dupesearchresumedeleted', ok, 'fast=%d resumed=%d donors_after=%d (want 3: one fast kept, two resumed)'
            % (len(fast), resumed, len(donors)))


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
    # a plain delete: with DupeCheck nzbget keeps a hidden duplicate record (hkDup),
    # whose score edit succeeds without meaning anything (B21)
    for h in donors:
        api.editqueue('HistoryDelete', '', [h['ID']])
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
                'other95': (ids('o5'), 420_000, 40, 13), 'twin90': (ids('t9'), 400_000, 50, 14),
                'gone': (ids('gn'), 430_000, 5, 15)}
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
    summary = _grep_log(t, 'verified=5 added=4')
    # a dry run leaves no state a real search would trust: the key isn't recorded
    # as searched (B18) and the dead posting isn't recorded as dead (B19)
    def state(name):
        try:
            return t.read_file(os.path.join('main', 'queue', name)).decode(errors='replace')
        except Exception:
            return ''
    no_state = 'tvdbid=1-s01-e01' not in state('dupesearch') and not state('dupesearch-dead').strip()
    pick_ok = len(groups) == 1 and groups[0]['MaxPostTime'] is not None and groups[0].get('DupeScore') == DS_PICK
    # the search's lines are in the pick's own log too (a server that writes no
    # log file keeps no info lines in the global one)
    item_log = [m['Text'] for m in api.loadlog(groups[0]['NZBID'], 0, 1000)] if groups else []
    in_item = sum(1 for t in item_log if 'verified=5 added=4' in t) == 1 and \
        sum(1 for t in item_log if 'dry run, would add' in t) == 4
    ok = not queued and would == 4 and summary == 1 and pick_ok and in_item and no_state
    return ('dupesearchdryrun', ok, 'queued=%d would=%d summary=%d pick_ok=%s in_item_log=%s no_state=%s'
            % (len(queued), would, summary, pick_ok, in_item, no_state))


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
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    time.sleep(8)
    first = _grep_log(t, 'DupeSearch: searching duplicates of %s ' % DS_TITLE)
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
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


def scenario_deadpickafterreload(daemon, t):
    """B4: a reload stops the dead-pick probes (StopAll refuses new ones while
    nzbget shuts its parts down); the new coordinator allows them again, so the
    probe still abandons a dead pick after a reload."""
    api = daemon.wait_ready()
    api.reload()
    time.sleep(3)
    daemon.wait_ready()
    name, ok, detail = scenario_deadpickprobe(daemon, t)
    return ('deadpickafterreload', ok, detail)


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
    verdict, and the unreachable one is ignored, not counted as missing. Under
    load the probe can take over 10 s; the dead download then fails over by the
    rule that doesn't wait for a slow probe (B83) - the probe still gives its
    verdict, which is what this checks."""
    hp, hb, integ = _deadpick_servers(daemon, t, 'ds')
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, '5 of 6 servers answered definitively') == 0:
        time.sleep(0.5)
    probed = _grep_log(t, '0 of 10 sampled articles exist (5 of 6 servers answered definitively): the posting is dead')
    return ('deadpickservers', probed == 1 and integ and hb['Status'].startswith('SUCCESS'),
            'status=%s backup_status=%s probe_logs=%d failed_articles=%s integrity=%s'
            % (hp['Status'], hb['Status'], probed, hp.get('FailedArticles'), integ))


def scenario_deadpickfewservers(daemon, t):
    """Six news servers, two of them unreachable: only four answer
    definitively, below the minimum of five, so the probe gives no verdict.
    The dead posting fails over by the other rules: the health check, or
    (while the slow probe still waits on the unreachable servers) 200 failed
    articles with none of its own arrived; the probe never calls it dead."""
    hp, hb, integ = _deadpick_servers(daemon, t, 'df')
    probed = _grep_log(t, 'none of 10 sampled articles exists on any server')
    no_verdict = _grep_log(t, '4 of 6 servers answered definitively)')
    return ('deadpickfewservers', probed == 0 and integ and
            hb['Status'].startswith('SUCCESS'),
            'status=%s backup_status=%s probe_logs=%d no_verdict_logs=%d integrity=%s'
            % (hp['Status'], hb['Status'], probed, no_verdict, integ))


def scenario_deadpickstray(daemon, t):
    """B45: a dead posting with a single stray article alive, and that article is
    one the probe samples (part 10 of the first file, the first of the ten
    samples). One sample found doesn't make the posting alive: the probe calls
    it dead, says where it found the stray article, and fails over to the
    backup at once."""
    primary, backup, data = _probe_fixture(t, 'ds')
    m = primary[0]
    primary[0] = (m[0], m[1], m[2], m[3], m[4] - {10})
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, 'ds-key', 100)
    daemon.append(api, 'Backup', backup, False, 'ds-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups()
                                         if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary')
    verdict = _grep_log(t, 'Dupe probe: 1 of 10 sampled articles exist')
    dead = _grep_log(t, 'the posting is dead')
    failed_over = _grep_log(t, 'Failing over Primary to duplicate Backup: none of 10 sampled')
    return ('deadpickstray', verdict == 1 and dead == 1 and failed_over == 1,
            'status=%s probe_one_found_logs=%d dead_logs=%d probe_failover_logs=%d'
            % (hp['Status'], verdict, dead, failed_over))


def scenario_deaddownloadlate(daemon, t):
    """B45: a large dead posting (1,800 articles, no par2) whose backup arrives
    only after the download started, so the probe had no backup to fail over to
    and stepped aside. Once 64 articles beyond the first of each file failed and
    none of its own arrived, the download fails over to the backup - long before
    its health falls below the critical 85% (270 failures)."""
    seg = 10_000
    vol = 3_000_000
    n = vol // seg
    primary = [('dlA/d%d.bin' % i, 'Dead%d.bin' % i, vol, seg, set(range(1, n + 1))) for i in range(6)]
    for m in primary:
        t.write_file(os.path.join('data', m[0]), _payload(vol, 9720))
    data = _payload(2_900_000, 9721)
    bp = _place_copy(t, 'dlB', data)
    backup = build_nzb(bp, 'Backup.bin', 2_900_000, 100_000, set())
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), False, 'dl-key', 100)
    time.sleep(1)
    daemon.append(api, 'Backup', backup, False, 'dl-key', 90)
    hp = daemon.wait_history(api, 'Primary')
    deadline = time.time() + 120
    hb = daemon.wait_history(api, 'Backup')
    while not hb['Status'].startswith('SUCCESS') and time.time() < deadline:
        time.sleep(0.5)
        hb = daemon.wait_history(api, 'Backup')
    # the early failover: B45 (0 of its own arrived) or, first since B49, 40 failed in a row
    rule = (_grep_log(t, 'Failing over Primary to duplicate Backup: 0 of its own articles downloaded') +
            _grep_log(t, 'failed in a row'))
    failed = int(hp.get('FailedArticles', 0))
    ok = rule == 1 and failed < 270 and hb['Status'].startswith('SUCCESS') and _verify_output(t, data)
    return ('deaddownloadlate', ok, 'status=%s backup=%s rule_failover_logs=%d failed_articles=%d'
            % (hp['Status'], hb['Status'], rule, failed))


def scenario_forcefailover(daemon, t):
    """B46: a pick sent in DupeMode FORCE (a client re-sending an nzb-file nzbget
    skipped as a copy) is dead, and a healthy backup waits in history: with
    HealthCheck=dupe it fails over like a score pick instead of failing with
    the backup unused (production 4144)."""
    primary, backup, data = _probe_fixture(t, 'ff')
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, 'ff-key', 100)
    daemon.append(api, 'Backup', backup, False, 'ff-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    pid = [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'][0]
    api.editqueue('GroupSetDupeMode', 0, 'FORCE', [pid])
    api.editqueue('GroupResume', 0, '', [pid])
    hp = daemon.wait_history(api, 'Primary')
    deadline = time.time() + 120
    hb = daemon.wait_history(api, 'Backup')
    while not hb['Status'].startswith('SUCCESS') and time.time() < deadline:
        time.sleep(0.5)
        hb = daemon.wait_history(api, 'Backup')
    over = _grep_log(t, 'Failing over Primary to duplicate Backup')
    mode = [h.get('DupeMode') for h in api.history() if h['NZBID'] == pid]
    ok = mode == ['FORCE'] and over == 1 and hb['Status'].startswith('SUCCESS') and _verify_output(t, data)
    return ('forcefailover', ok, 'primary=%s mode=%s backup=%s failover_logs=%d' % (hp['Status'], mode, hb['Status'], over))


def scenario_copybackup(daemon, t):
    """B46: the only other posting in history is a copy (sent twice: the second
    time nzbget skipped it as the same content, then the first was deleted for
    good): when the dead pick fails over, the copy - another posting - is
    fetched; a copy of the pick itself never is."""
    primary, backup, data = _probe_fixture(t, 'cb')
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, 'cb-key', 100)
    daemon.append(api, 'Backup', backup, False, 'cb-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    first = [h['NZBID'] for h in api.history() if h['NZBName'] == 'Backup']
    daemon.append(api, 'Backup', backup, False, 'cb-key', 91)
    time.sleep(2)
    copies = [h for h in api.history() if h['NZBName'] == 'Backup' and h['NZBID'] not in first]
    api.editqueue('HistoryFinalDelete', 0, '', first)
    # the pick's own copy in history, which must not come back
    daemon.append(api, 'Primary', build_multi_nzb(primary), False, 'cb-key', 99)
    time.sleep(2)
    pid = [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'][0]
    api.editqueue('GroupResume', 0, '', [pid])
    daemon.wait_history(api, 'Primary')
    deadline = time.time() + 120
    done = []
    while time.time() < deadline and not done:
        done = [h for h in api.history() if h['NZBName'] == 'Backup' and h['Status'].startswith('SUCCESS')]
        time.sleep(0.5)
    self_copy_back = _grep_log(t, 'Found duplicate Primary')
    # since B43 a re-sent backup's nzb-file is filed as a backup (DUPE), no longer
    # skipped as a copy: either way it is a backup, and the pick's own copy isn't
    ok = (len(copies) == 1 and copies[0]['Status'] in ('DELETED/COPY', 'DELETED/DUPE') and done and
          self_copy_back == 0 and _verify_output(t, data))
    return ('copybackup', ok, 'copy=%s backup_done=%s self_copy_returned=%d'
            % (copies[0]['Status'] if copies else None, bool(done), self_copy_back))


def _big_dead(t, tag, alive=()):
    """A 1,800-article posting (six files of 300 articles, no par2) whose
    articles are all missing but the global indexes in <alive>, and a healthy
    backup in another packing."""
    seg, vol = 10_000, 3_000_000
    n = vol // seg
    primary = []
    for i in range(6):
        missing = {p for p in range(1, n + 1) if i * n + p - 1 not in alive}
        primary.append(('%sA/d%d.bin' % (tag, i), 'Dead%d.bin' % i, vol, seg, missing))
        t.write_file(os.path.join('data', primary[-1][0]), _payload(vol, 9730))
    data = _payload(2_900_000, 9731)
    bp = _place_copy(t, '%sB' % tag, data)
    return primary, build_nzb(bp, 'Backup.bin', 2_900_000, 100_000, set()), data


def _late_backup_run(daemon, t, tag, alive=(), restart_after=0):
    primary, backup, data = _big_dead(t, tag, alive)
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), False, tag + '-key', 100)
    time.sleep(1)
    daemon.append(api, 'Backup', backup, False, tag + '-key', 90)
    at_restart = 0
    if restart_after:
        deadline = time.time() + 60
        while time.time() < deadline:
            g = [x for x in api.listgroups() if x['NZBName'] == 'Primary']
            at_restart = g[0]['FailedArticles'] if g else -1
            if not g or at_restart >= restart_after:
                break
            time.sleep(0.05)
        try:
            api.shutdown()
        except Exception:
            pass
        t.procs[-1].wait(timeout=60)
        daemon.start()
        api = daemon.wait_ready()
    hp = daemon.wait_history(api, 'Primary')
    deadline = time.time() + 120
    hb = daemon.wait_history(api, 'Backup')
    while not hb['Status'].startswith('SUCCESS') and time.time() < deadline:
        time.sleep(0.5)
        hb = daemon.wait_history(api, 'Backup')
    rule = _grep_log(t, 'Failing over Primary to duplicate Backup: ')
    return hp, hb, rule, data, at_restart


def scenario_deaddownloadstray2(daemon, t):
    """R1c: 2 of the 1,800 articles of the posting exist (one stray in two
    files): the early failover still fires - once fewer than 1 in 100 tried
    articles arrived - long before the health check would (270 failures)."""
    # the first articles of two files: direct-rename fetches those first
    hp, hb, rule, data, _ = _late_backup_run(daemon, t, 'dt', alive={0, 300})
    failed = int(hp.get('FailedArticles', 0))
    ok = (rule == 1 and int(hp.get('SuccessArticles', 0)) >= 1 and failed < 270 and
          hb['Status'].startswith('SUCCESS') and _verify_output(t, data))
    return ('deaddownloadstray2', ok, 'status=%s backup=%s failover_logs=%d failed_articles=%d success=%s'
            % (hp['Status'], hb['Status'], rule, failed, hp.get('SuccessArticles')))


def scenario_deaddownloadrestart(daemon, t):
    """R1f: nzbget restarts after 40 failed articles of a dead posting
    (ContinuePartial=yes, as in production): it fails over promptly after the
    restart - the kept failure count, or the probe that runs again - not
    after another full run of failures."""
    hp, hb, rule, data, at_restart = _late_backup_run(daemon, t, 'dr', restart_after=40)
    failed = int(hp.get('FailedArticles', 0))
    ok = (rule == 1 and at_restart >= 40 and failed < 105 and hb['Status'].startswith('SUCCESS')
          and _verify_output(t, data))
    return ('deaddownloadrestart', ok, 'status=%s backup=%s failover_logs=%d failed_at_restart=%d failed_articles=%d'
            % (hp['Status'], hb['Status'], rule, at_restart, failed))


def scenario_deadbackupsonly(daemon, t):
    """R2c: every backup was found dead by a dupe tool (DupeAlive=0): none is
    fetched, the dead pick fails as before."""
    primary, backup, data = _probe_fixture(t, 'db')
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, 'db-key', 100)
    _ds_append(api, 'Backup', backup, 'db-key', 90, paused=False, params=[{'Name': 'DupeAlive', 'Value': '0'}])
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary')
    time.sleep(3)
    back = _grep_log(t, 'Found duplicate Backup') + _grep_log(t, 'Failing over Primary')
    hb = daemon.wait_history(api, 'Backup')
    ok = back == 0 and hb['Status'] == 'DELETED/DUPE' and hp['Status'].startswith('FAILURE')
    return ('deadbackupsonly', ok, 'primary=%s backup=%s returned_logs=%d' % (hp['Status'], hb['Status'], back))


def scenario_backuporder(daemon, t):
    """R2f: two backups of equal score, one found 50% alive and one 90% alive
    (DupeAlive): the more alive one takes the dead pick's place."""
    primary, backup, data = _probe_fixture(t, 'bo')
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, 'bo-key', 100)
    _ds_append(api, 'Half', backup, 'bo-key', 90, paused=False, params=[{'Name': 'DupeAlive', 'Value': '50'}])
    _ds_append(api, 'Most', backup.replace('Backup.bin', 'Backup2.bin'), 'bo-key', 90, paused=False,
               params=[{'Name': 'DupeAlive', 'Value': '90'}])
    daemon.wait_history(api, 'Half', timeout=60)
    daemon.wait_history(api, 'Most', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    daemon.wait_history(api, 'Primary')
    time.sleep(2)
    most = _grep_log(t, 'to duplicate Most') + _grep_log(t, 'Found duplicate Most')
    half = _grep_log(t, 'to duplicate Half') + _grep_log(t, 'Found duplicate Half')
    ok = most >= 1 and half == 0
    return ('backuporder', ok, 'most_alive_chosen_logs=%d half_alive_chosen_logs=%d' % (most, half))


def scenario_copyoffailed(daemon, t):
    """B47: a pick failed earlier (a dead posting), and the same nzb-file was
    sent again and skipped as a copy - scored above the healthy backup. When
    the next dead pick fails over, the copy (the same dead posting) is passed
    over for the healthy backup (production 4145 after 4144 had died)."""
    first, backup, data = _probe_fixture(t, 'cf')
    second, _, _ = _probe_fixture(t, 'cg')
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(first), False, 'cf-key', 100)
    daemon.wait_history(api, 'Primary')
    daemon.append(api, 'Primary', build_multi_nzb(first), False, 'cf-key', 98)
    time.sleep(2)
    copies = [h['Status'] for h in api.history() if h['NZBName'] == 'Primary']
    daemon.append(api, 'Second', build_multi_nzb(second), True, 'cf-key', 100)
    daemon.append(api, 'Backup', backup, False, 'cf-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Second'])
    daemon.wait_history(api, 'Second')
    deadline = time.time() + 120
    done = []
    while time.time() < deadline and not done:
        done = [h for h in api.history() if h['NZBName'] == 'Backup' and h['Status'].startswith('SUCCESS')]
        time.sleep(0.5)
    copy_back = _grep_log(t, 'to duplicate Primary') + _grep_log(t, 'Found duplicate Primary')
    ok = 'DELETED/COPY' in copies and bool(done) and copy_back == 0 and _verify_output(t, data)
    return ('copyoffailed', ok, 'first_statuses=%s backup_done=%s dead_copy_chosen=%d' % (copies, bool(done), copy_back))


def scenario_resendbackup(daemon, t):
    """B43: a client cancels its pick and sends one of the backups' nzb-files
    again as the new pick. Its content waits in history only as an untried
    backup: it is downloaded, not skipped as a copy (before, nothing
    downloaded and clients re-sent it in DupeMode force)."""
    primary, backup, data = _probe_fixture(t, 'rb')
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, 'rb-key', 100)
    daemon.append(api, 'Backup', backup, False, 'rb-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupFinalDelete', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    time.sleep(1)
    daemon.append(api, 'Resent', backup, False, 'rb-key', 100)
    deadline = time.time() + 120
    done = None
    while time.time() < deadline and not done:
        done = next((h for h in api.history() if h['NZBName'] == 'Resent' and h['Status'] != 'DELETED/COPY'
                     and h['Status'].startswith('SUCCESS')), None)
        copy = next((h for h in api.history() if h['NZBName'] == 'Resent' and h['Status'] == 'DELETED/COPY'), None)
        if copy:
            break
        time.sleep(0.5)
    ok = bool(done) and _grep_log(t, 'waits in history only as an untried backup') == 1 and _verify_output(t, data)
    return ('resendbackup', ok, 'resent=%s' % (done['Status'] if done else (copy and copy['Status'])))


def scenario_resendfailed(daemon, t):
    """B43: the nzb-file of a pick that failed (a dead posting) is sent again
    while a healthy backup waits in history and nothing of the key is queued:
    it is skipped as the same dead posting, and the backup is fetched in its
    place - the re-send still leads to a download."""
    first, backup, data = _probe_fixture(t, 'rf')
    other, _, _ = _probe_fixture(t, 'rg')
    api = daemon.wait_ready()
    daemon.append(api, 'Dead', build_multi_nzb(first), False, 'rf-key', 100)
    daemon.wait_history(api, 'Dead')
    daemon.append(api, 'Holder', build_multi_nzb(other), True, 'rf-key', 200)
    daemon.append(api, 'Backup', backup, False, 'rf-key', 90)
    daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupFinalDelete', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Holder'])
    time.sleep(1)
    daemon.append(api, 'Dead', build_multi_nzb(first), False, 'rf-key', 100)
    deadline = time.time() + 120
    done = []
    while time.time() < deadline and not done:
        done = [h for h in api.history() if h['NZBName'] == 'Backup' and h['Status'].startswith('SUCCESS')]
        time.sleep(0.5)
    skipped = [h['Status'] for h in api.history() if h['NZBName'] == 'Dead']
    fetched = _grep_log(t, 'is the same posting as a failed download: fetching the best backup instead')
    ok = 'DELETED/COPY' in skipped and fetched == 1 and bool(done) and _verify_output(t, data)
    return ('resendfailed', ok, 'dead_statuses=%s fetch_logs=%d backup_done=%s' % (skipped, fetched, bool(done)))


def _projected_run(daemon, t, tag, alive_percent, backup_alive):
    """A 1,800-article posting (twenty files, no par2: critical health 85%) of which
    <alive_percent>% of the articles exist, spread evenly; a healthy backup marked
    <backup_alive>% alive (DupeAlive) waits in history, or none if None."""
    seg, vol = 10_000, 900_000
    n = vol // seg
    primary = []
    # twenty files of 90 articles: 200 tried articles span three files
    for i in range(20):
        missing = {p for p in range(1, n + 1) if ((i * n + p - 1) * 37) % 100 >= alive_percent}
        primary.append(('%sA/d%d.bin' % (tag, i), 'Part%d.bin' % i, vol, seg, missing))
        t.write_file(os.path.join('data', primary[-1][0]), _payload(vol, 9740 + i))
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, tag + '-key', 100)
    if backup_alive is not None:
        data = _payload(2_900_000, 9749)
        bp = _place_copy(t, '%sB' % tag, data)
        _ds_append(api, 'Backup', build_nzb(bp, 'Backup.bin', 2_900_000, 100_000, set()), tag + '-key', 90,
                   paused=False, params=[{'Name': 'DupeAlive', 'Value': str(backup_alive)}])
        daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary', timeout=300)
    swaps = _grep_log(t, 'projected health')
    return hp, swaps


def scenario_projectedswap(daemon, t):
    """B48a: 31% of the pick's articles arrive and a backup is known 100% alive:
    the pick is swapped once about 200 articles were tried, long before its
    health (which counts failures against the whole posting) reaches critical."""
    hp, swaps = _projected_run(daemon, t, 'pa', 31, 100)
    failed = int(hp.get('FailedArticles', 0))
    return ('projectedswap', swaps == 1 and failed < 250,
            'status=%s swap_logs=%d failed_articles=%d' % (hp['Status'], swaps, failed))


def scenario_projectednobackup(daemon, t):
    """B48b/B69: 31% arrive and no backup waits: no swap, but the download is
    parked once about 200 articles were tried (projected under half the
    critical 85%), instead of running through every article."""
    hp, _ = _projected_run(daemon, t, 'pb', 31, None)
    swaps = _grep_log(t, 'Failing over')
    parked = _grep_log(t, 'under half the critical')
    failed = int(hp.get('FailedArticles', 0))
    return ('projectednobackup', swaps == 0 and parked == 1 and failed < 250,
            'status=%s swap_logs=%d park_logs=%d failed_articles=%d' % (hp['Status'], swaps, parked, failed))


def scenario_projectedhalfnobackup(daemon, t):
    """B69/B74: 50% arrive and no backup waits: above half the critical 85%, so
    not parked after 200 tried, but below critical: parked once 1,000 were
    tried (about 500 failed), not at the end (900)."""
    hp, _ = _projected_run(daemon, t, 'ph', 50, None)
    early = _grep_log(t, 'under half the critical')
    parked = _grep_log(t, 'below critical')
    failed = int(hp.get('FailedArticles', 0))
    return ('projectedhalfnobackup', early == 0 and parked == 1 and 400 < failed < 700,
            'status=%s early_park_logs=%d park_logs=%d failed_articles=%d' % (hp['Status'], early, parked, failed))


def scenario_projectedbelownobackup(daemon, t):
    """B74 (Las Azules S02E05 4417: 79.3% arriving against a critical 80%, ran
    through 10 GB): 80% arrive against the critical 85%, no backup waits:
    parked once 1,000 articles were tried (about 200 failed), instead of
    running to the end (360)."""
    hp, _ = _projected_run(daemon, t, 'pz', 80, None)
    parked = _grep_log(t, 'below critical')
    failed = int(hp.get('FailedArticles', 0))
    return ('projectedbelownobackup', parked == 1 and failed < 300,
            'status=%s park_logs=%d failed_articles=%d' % (hp['Status'], parked, failed))


def scenario_keepreturned(daemon, t):
    """B73 (Las Azules S02E01 4404): a dead pick fails over to a backup, which
    downloads in good health (slowly here). A duplicate scored higher arrives
    meanwhile, as a dupe tool appends one: the returned backup keeps
    downloading and succeeds; the newcomer waits in history as a backup.
    Before, nzbget moved the downloading backup to history and started over."""
    size, seg = 3_000_000, 100_000
    data = _payload(size, 7373)
    pick = build_nzb(_place_copy(t, 'krP', data), 'Pick.bin', size, seg, set(range(1, 31)))
    backup = build_nzb(_place_copy(t, 'krA', data), 'BackA.bin', size, seg, set())
    late = build_nzb(_place_copy(t, 'krC', data), 'LateC.bin', size, seg, set())
    api = daemon.wait_ready()
    pick_id = daemon.append(api, 'Pick', pick, True, 'kr-key', 100)
    daemon.append(api, 'BackA', backup, False, 'kr-key', 90)
    daemon.wait_history(api, 'BackA', timeout=30)
    api.editqueue('GroupResume', 0, '', [pick_id])
    deadline = time.time() + 60
    while time.time() < deadline:
        g = _ds_group(api, 'BackA')
        if g and int(g.get('SuccessArticles', 0)) >= 3:
            break
        time.sleep(0.2)
    daemon.append(api, 'LateC', late, True, 'kr-key', 95)
    deadline = time.time() + 120
    a_status = c_status = None
    while time.time() < deadline:
        hist = {x['NZBName']: x['Status'] for x in api.history()}
        a_status, c_status = hist.get('BackA'), hist.get('LateC')
        if a_status and a_status.startswith(('SUCCESS', 'FAILURE')) and c_status:
            break
        time.sleep(0.5)
    moved = _grep_log(t, 'Moving collection BackA with lower duplicate score')
    ok = moved == 0 and (a_status or '').startswith('SUCCESS') and c_status == 'DELETED/DUPE'
    return ('keepreturned', ok, 'backup=%s late=%s moved_logs=%d' % (a_status, c_status, moved))


def scenario_restartfailover(daemon, t):
    """A dead pick fails over to a backup, and nzbget restarts while the backup
    downloads (slowly here). After the restart the backup resumes and succeeds;
    the pick stays failed and no other backup is fetched."""
    size, seg = 3_000_000, 100_000
    data = _payload(size, 7474)
    pick = build_nzb(_place_copy(t, 'rfP', data), 'Pick.bin', size, seg, set(range(1, 31)))
    backup = build_nzb(_place_copy(t, 'rfA', data), 'BackA.bin', size, seg, set())
    other = build_nzb(_place_copy(t, 'rfB', data), 'BackB.bin', size, seg, set())
    api = daemon.wait_ready()
    pick_id = daemon.append(api, 'Pick', pick, True, 'rf-key', 100)
    daemon.append(api, 'BackA', backup, False, 'rf-key', 90)
    daemon.append(api, 'BackB', other, False, 'rf-key', 80)
    deadline = time.time() + 30
    while time.time() < deadline and len([h for h in api.history() if h['NZBName'].startswith('Back')]) < 2:
        time.sleep(0.2)
    api.editqueue('GroupResume', 0, '', [pick_id])
    deadline = time.time() + 60
    while time.time() < deadline:
        g = _ds_group(api, 'BackA')
        if g and int(g.get('SuccessArticles', 0)) >= 3:
            break
        time.sleep(0.2)
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    daemon.start()
    api = daemon.wait_ready()
    deadline = time.time() + 120
    hist = {}
    while time.time() < deadline:
        hist = {x['NZBName']: x['Status'] for x in api.history()}
        if hist.get('BackA', '').startswith(('SUCCESS', 'FAILURE')):
            break
        time.sleep(0.5)
    integ = _verify_output(t, data)
    ok = (hist.get('BackA', '').startswith('SUCCESS') and hist.get('Pick', '').startswith('FAILURE') and
          hist.get('BackB') == 'DELETED/DUPE' and integ)
    return ('restartfailover', ok, 'pick=%s backupA=%s backupB=%s integrity=%s'
            % (hist.get('Pick'), hist.get('BackA'), hist.get('BackB'), integ))


def scenario_slowprobe(daemon, t):
    """Physical S02E01 4948: a dead backup ran to 1,120 failed articles in 34 s,
    none of its own arriving, while its dead-pick probe waited (here every STAT
    takes 5 s, so the probe needs most of a minute; articles fail at about 20 a
    second, as on a real server). The rules that fail over a dead download stood
    aside while the probe ran. Now, once the probe ran 10 s with 200 articles
    failed and none arrived, the download fails over without it."""
    seg, vol = 10_000, 3_000_000
    n = vol // seg
    primary = []
    for i in range(20):
        primary.append(('spA/d%d.bin' % i, 'Dead%d.bin' % i, vol, seg, set(range(1, n + 1))))
        t.write_file(os.path.join('data', primary[-1][0]), _payload(vol, 9800 + i))
    data = _payload(2_900_000, 9821)
    backup = build_nzb(_place_copy(t, 'spB', data), 'Backup.bin', 2_900_000, 100_000, set())
    api = daemon.wait_ready()
    pick_id = daemon.append(api, 'DeadSp', build_multi_nzb(primary), True, 'sp-key', 100)
    daemon.append(api, 'BackSp', backup, False, 'sp-key', 90)
    daemon.wait_history(api, 'BackSp', timeout=30)
    api.editqueue('GroupResume', 0, '', [pick_id])
    h = daemon.wait_history(api, 'DeadSp', timeout=240)
    failed = int(h.get('FailedArticles', 0))
    swapped = _grep_log(t, 'Failing over DeadSp')
    ok = swapped == 1 and failed < 500
    return ('slowprobe', ok, 'status=%s failed_articles=%d failover_logs=%d' % (h['Status'], failed, swapped))


def scenario_runparcovers(daemon, t):
    """A pick lacks one stretch of 60 articles in a row (a missing volume, 3% of
    its data) while its par2 files hold 10%: par2 can repair it. The rule that
    fails over after 40 failures in a row (B49) must not swap it for the live
    backup waiting in history: the download completes. (The par2 files are
    recognized by name; ParCheck=manual leaves them unread.)"""
    seg, n = 4_000, 2_000
    size = seg * n
    data = _payload(size, 6161)
    pp = _place_copy(t, 'rpA', data, 'show.mkv')
    par_index = _place_copy(t, 'rpA', _payload(4_000, 6162), 'show.par2')
    par_vol = _place_copy(t, 'rpA', _payload(seg * 200, 6163), 'show.vol00+200.par2')
    primary = build_multi_nzb([(pp, 'show.mkv', size, seg, set(range(1000, 1060))),
                               (par_index, 'show.par2', 4_000, seg, set()),
                               (par_vol, 'show.vol00+200.par2', seg * 200, seg, set())])
    backup = build_nzb(_place_copy(t, 'rpB', data, 'show.mkv'), 'show.mkv', size, seg, set())
    api = daemon.wait_ready()
    pick_id = daemon.append(api, 'PickRp', primary, True, 'rp-key', 100)
    _ds_append(api, 'BackRp', backup, 'rp-key', 90, params=[{'Name': 'DupeAlive', 'Value': '100'}])
    daemon.wait_history(api, 'BackRp', timeout=30)
    api.editqueue('GroupResume', 0, '', [pick_id])
    h = daemon.wait_history(api, 'PickRp', timeout=180)
    swapped = _grep_log(t, 'Failing over PickRp')
    ok = swapped == 0 and int(h.get('FailedArticles', 0)) == 60
    return ('runparcovers', ok, 'status=%s failed_articles=%s failover_logs=%d' % (h['Status'], h.get('FailedArticles'), swapped))


def _ondisk_first(daemon, t, tag):
    """A release downloaded once (SUCCESS, its file on disk), then another posting
    of it under the same key is sent, as when a client asks again."""
    size, seg = 2_000_000, 100_000
    data = _payload(size, 7070)
    first = build_nzb(_place_copy(t, tag + '1', data, 'show.mkv'), 'show.mkv', size, seg, set())
    again = build_nzb(_place_copy(t, tag + '2', data, 'show.mkv'), 'show.mkv', size, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'First', first, False, tag + '-key', 100)
    h = daemon.wait_history(api, 'First')
    return api, h, again


def scenario_ondiskskip(daemon, t):
    """B70 (Knife Edge S02E01): the release is downloaded and on disk; another
    posting of it, scored higher, arrives under its key: it isn't downloaded,
    but kept in history as a backup."""
    api, h, again = _ondisk_first(daemon, t, 'os')
    daemon.append(api, 'Again', again, False, 'os-key', 200)
    h2 = daemon.wait_history(api, 'Again', timeout=60)
    skipped = _grep_log(t, 'is downloaded and on disk')
    ok = h['Status'].startswith('SUCCESS') and h2['Status'] == 'DELETED/DUPE' and skipped == 1
    return ('ondiskskip', ok, 'first=%s again=%s skip_logs=%d' % (h['Status'], h2['Status'], skipped))


def scenario_ondiskgone(daemon, t):
    """B70: the release succeeded, but its files were deleted from disk since:
    the success no longer counts, and the posting sent again downloads."""
    api, h, again = _ondisk_first(daemon, t, 'og')
    for rel in t.find_files('main', 'dst'):
        os.remove(t.path(rel))
    daemon.append(api, 'Again', again, False, 'og-key', 200)
    h2 = daemon.wait_history(api, 'Again', timeout=120)
    ok = h['Status'].startswith('SUCCESS') and h2['Status'].startswith('SUCCESS') and \
        _grep_log(t, 'is downloaded and on disk') == 0
    return ('ondiskgone', ok, 'first=%s again=%s' % (h['Status'], h2['Status']))


def scenario_ondiskstop(daemon, t):
    """B77 (Sugar S01E01): one copy is in stream repair (from a slow duplicate,
    1 s a request) when another copy of the same key downloads whole and
    succeeds: the repair is stopped, and the repairing copy is kept in history
    as a backup instead of running on."""
    # about 80 requests to the slow duplicate: the repair outlasts the clean download
    size, seg = 40_000_000, 500_000
    data = _payload(size, 7171)
    damaged = build_nzb(_place_copy(t, 'odA', data, 'file.mkv'), 'odA.mkv', size, seg, set(range(2, 80, 2)))
    donor = build_nzb(_place_copy(t, 'odB', data, 'file.mkv'), 'obf-od.mkv', size, 250_000, set())
    clean = build_nzb(_place_copy(t, 'odC', data, 'file.mkv'), 'odC.mkv', size, seg, set())
    api = daemon.wait_ready()
    # a dupe tool found the duplicate dead (DupeAlive=0): no failover fetches it,
    # so stream repair is the only option left (B78) - yet its articles exist
    _ds_append(api, 'DonOd', donor, 'od-key', 50, params=[{'Name': 'DupeAlive', 'Value': '0'}])
    daemon.append(api, 'Damaged', damaged, False, 'od-key', 100)
    deadline = time.time() + 60
    while time.time() < deadline and _grep_log(t, 'Queueing Damaged for post-processing') == 0:
        time.sleep(0.2)
    daemon.append(api, 'Clean', clean, False, 'od-key', 110)
    hc = daemon.wait_history(api, 'Clean', timeout=120)
    hd = daemon.wait_history(api, 'Damaged', timeout=120)
    stopped = _grep_log(t, 'Stopping Damaged')
    ok = hc['Status'].startswith('SUCCESS') and hd['Status'] == 'DELETED/DUPE' and stopped == 1
    return ('ondiskstop', ok, 'clean=%s damaged=%s stop_logs=%d' % (hc['Status'], hd['Status'], stopped))


def scenario_repairlast(daemon, t):
    """B78: stream repair is the last option. A download is damaged beyond its
    par2 (none here) while a whole backup waits in history: it isn't repaired
    from the backup, it fails over to it, which then downloads whole. Before,
    the download was stream-repaired first, which can take longer than
    downloading a good posting."""
    size, seg = 6_000_000, 500_000
    data = _payload(size, 7272)
    damaged = build_nzb(_place_copy(t, 'rlA', data, 'file.mkv'), 'rlA.mkv', size, seg, set(range(2, 10)))
    backup = build_nzb(_place_copy(t, 'rlB', data, 'file.mkv'), 'obf-rl.mkv', size, 250_000, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DamagedRl', damaged, True, 'rl-key', 100)
    daemon.append(api, 'BackRl', backup, False, 'rl-key', 90)
    daemon.wait_history(api, 'BackRl', timeout=30)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'DamagedRl'])
    deadline = time.time() + 180
    hist = {}
    while time.time() < deadline:
        hist = {x['NZBName']: x['Status'] for x in api.history()}
        if hist.get('BackRl', '').startswith(('SUCCESS', 'FAILURE')) and hist.get('DamagedRl', '').startswith('FAILURE'):
            break
        time.sleep(0.5)
    skipped = _grep_log(t, 'a backup in history is tried first')
    # (with no par2 nothing is borrowed either, so the download may fail over
    # before the repair decision that logs the skip: no repair is what counts)
    repaired = _grep_log(t, 'Stream repair of rlA') + _grep_log(t, 'Recovered') 
    ok = (skipped >= 1 or repaired == 0) and hist.get('DamagedRl', '').startswith('FAILURE') and \
        hist.get('BackRl', '').startswith('SUCCESS')
    return ('repairlast', ok, 'damaged=%s backup=%s skip_logs=%d repair_logs=%d' % (
        hist.get('DamagedRl'), hist.get('BackRl'), skipped, repaired))


def scenario_samepostingrepair(daemon, t):
    """The proxy's stream-mode case: a pick without par2 missing a few articles,
    and in history a donor that shares every other article's message-id (the
    same posting with the missing articles intact). Before: the pick downloaded,
    ended FAILURE/HEALTH, the donor was never used. The release must end up
    complete: repaired from the donor, or the donor downloaded in its place."""
    size, seg = 6_000_000, 500_000
    data = _payload(size, 7373)
    pp = _place_copy(t, 'spA', data, 'file.mkv')
    pick = build_nzb(pp, 'sp.mkv', size, seg, {3, 7})
    donor = build_nzb(pp, 'sp.mkv', size, seg, set())
    api = daemon.wait_ready()
    # (unpaused, as DupeDonors and appendfleet add their copies: a backup added
    # paused comes back paused when it is failed over to, by design)
    daemon.append(api, 'DonSP', donor, False, 'sp-key', 99)
    daemon.append(api, 'PickSP', pick, False, 'sp-key', 100)
    deadline = time.time() + 180
    hist = {}
    while time.time() < deadline:
        hist = {x['NZBName']: x['Status'] for x in api.history()}
        done = hist.get('PickSP', '').startswith('SUCCESS') or hist.get('DonSP', '').startswith('SUCCESS')
        if done or (hist.get('PickSP', '').startswith('FAILURE') and not any(g['NZBName'] == 'DonSP' for g in api.listgroups())
                    and time.time() > deadline - 150):
            if done:
                break
        time.sleep(0.5)
    ok = hist.get('PickSP', '').startswith('SUCCESS') or hist.get('DonSP', '').startswith('SUCCESS')
    return ('samepostingrepair', ok, 'pick=%s donor=%s failover_logs=%d repair_logs=%d' % (
        hist.get('PickSP'), hist.get('DonSP'), _grep_log(t, 'Failing over'), _grep_log(t, 'Stream repair')))


def scenario_samepostinglate(daemon, t):
    """As samepostingrepair, but only 1 of 24 articles missing (health 96%, above
    critical: no failover while downloading), no par2: the pick ends damaged, and
    the release must still end up complete through the donor."""
    size, seg = 12_000_000, 500_000
    data = _payload(size, 7474)
    pp = _place_copy(t, 'slA', data, 'file.mkv')
    pick = build_nzb(pp, 'sl.mkv', size, seg, {7})
    donor = build_nzb(pp, 'sl.mkv', size, seg, set())
    api = daemon.wait_ready()
    daemon.append(api, 'DonSL', donor, False, 'sl-key', 99)
    daemon.append(api, 'PickSL', pick, False, 'sl-key', 100)
    deadline = time.time() + 180
    hist = {}
    while time.time() < deadline:
        hist = {x['NZBName']: x['Status'] for x in api.history()}
        if hist.get('PickSL', '').startswith('SUCCESS') or hist.get('DonSL', '').startswith('SUCCESS'):
            break
        time.sleep(0.5)
    ok = hist.get('PickSL', '').startswith('SUCCESS') or hist.get('DonSL', '').startswith('SUCCESS')
    return ('samepostinglate', ok, 'pick=%s donor=%s queued=%s' % (
        hist.get('PickSL'), hist.get('DonSL'), [(g['NZBName'], g['Status']) for g in api.listgroups()]))


def scenario_recheckwholefail(daemon, t):
    """The failed-article recheck (HealthCheck=dupe, no backup) samples a file none
    of whose articles arrived too: it read a saved state such a file never has
    ("could not open file .../<id>c"), and left its articles out."""
    size, seg = 2_000_000, 500_000
    a = _place_copy(t, 'rwA', _payload(size, 7575), 'a.bin')
    b = _place_copy(t, 'rwB', _payload(size, 7576), 'b.bin')
    nzb = build_multi_nzb([(a, 'a.bin', size, seg, {2}), (b, 'b.bin', size, seg, {1, 2, 3, 4})])
    api = daemon.wait_ready()
    daemon.append(api, 'RelRW', nzb, False, 'rw-key', 100)
    h = daemon.wait_history(api, 'RelRW', timeout=120)
    deadline = time.time() + 30
    while time.time() < deadline and _grep_log(t, 'failed articles exist on the servers') == 0:
        time.sleep(0.5)
    with open(t.path('nzbget.log'), errors='replace') as f:
        found = re.findall(r'(\d+) of (\d+) failed articles exist on the servers', f.read())
    sampled = int(found[-1][1]) if found else -1
    no_state = _grep_log(t, 'could not open file')
    ok = no_state == 0 and sampled >= 5
    return ('recheckwholefail', ok, 'status=%s sampled=%d state_errors=%d' % (h['Status'], sampled, no_state))


def scenario_projectedhealthy(daemon, t):
    """B48c: 96% arrive (above the critical 85%): no swap, though a whole backup waits."""
    hp, swaps = _projected_run(daemon, t, 'pc', 96, 100)
    return ('projectedhealthy', swaps == 0, 'status=%s swap_logs=%d' % (hp['Status'], swaps))


def scenario_projectedworsebackup(daemon, t):
    """B48d: 60% arrive and the only backup is 50% alive: no swap for a worse copy."""
    hp, swaps = _projected_run(daemon, t, 'pd', 60, 50)
    return ('projectedworsebackup', swaps == 0, 'status=%s swap_logs=%d' % (hp['Status'], swaps))


def scenario_projectededge(daemon, t):
    """B58 (Dark Matter S02E05 4180): 87% arrive, within 5 points of the critical
    85%, and a backup is known 100% alive: par-repair can't be counted on that
    close to critical, so the pick is swapped once about 200 articles were
    tried. Before, it ran to the end: its projection never fell below critical."""
    hp, swaps = _projected_run(daemon, t, 'pe', 87, 100)
    failed = int(hp.get('FailedArticles', 0))
    close = _grep_log(t, 'too close to critical')
    return ('projectededge', swaps == 1 and close == 1 and failed < 60,
            'status=%s swap_logs=%d too_close_logs=%d failed_articles=%d' % (hp['Status'], swaps, close, failed))


def scenario_projectededgelead(daemon, t):
    """B58: 87% arrive, close to the critical 85%, but the backup is only 95%
    alive, less than 10 points wholer: no swap."""
    hp, swaps = _projected_run(daemon, t, 'pf', 87, 95)
    return ('projectededgelead', swaps == 0, 'status=%s swap_logs=%d' % (hp['Status'], swaps))


def scenario_parprojection(daemon, t):
    """B59 (Dark Matter S02E05 4180): borrowing waits for par-check while par2 may
    cover the damage, but 15% of the data is missing (evenly) and the par2 files
    hold 14%. Judged by the articles tried so far, par2 can't cover it: the wait
    ends once 200 were tried, and the missing articles are borrowed from the
    whole duplicate from then on, and it doesn't come back when the articles
    fetched from the lead duplicate lift the projection again. Before, it ended
    only when the failures outgrew the par2 data, near the end of the download,
    and about 140 articles failed; now only those that failed before 200 were
    tried (about 30) are left to post-processing. (The par2 files are
    recognized by name; ParCheck=manual leaves them unread.)"""
    seg, n = 4_000, 1_000
    size = seg * n
    data = _payload(size, 5959)
    missing = {p for p in range(2, n + 1) if (p * 37) % 100 < 15}
    pp = _place_copy(t, 'projA', data, 'show.mkv')
    par_index = _place_copy(t, 'projA', _payload(4_000, 5960), 'show.par2')
    par_vol = _place_copy(t, 'projA', _payload(seg * 140, 5961), 'show.vol00+140.par2')
    primary = build_multi_nzb([(pp, 'show.mkv', size, seg, missing),
                               (par_index, 'show.par2', 4_000, seg, set()),
                               (par_vol, 'show.vol00+140.par2', seg * 140, seg, set())])
    dp = _place_copy(t, 'projB', data, 'show.mkv')
    donor = build_multi_nzb([(dp, 'show.mkv', size, seg, set())])
    api = daemon.wait_ready()
    daemon.append(api, 'DonProj', donor, True, 'proj-key', 50)
    daemon.append(api, 'Proj', primary, False, 'proj-key', 100)
    h = daemon.wait_history(api, 'Proj', timeout=180)
    lifted = _grep_log(t, 'or will, judged by the articles tried so far')
    failed = int(h.get('FailedArticles', 0))
    ok = lifted == 1 and failed < 50
    return ('parprojection', ok, 'status=%s missing=%d lifted_logs=%d failed_articles=%d'
            % (h['Status'], len(missing), lifted, failed))


def _run_case(daemon, t, tag, missing_of, backup):
    """Six files of 300 articles (no par2); <missing_of(index)> says which
    articles (by their index over the whole posting) are missing; <backup> adds
    a healthy backup in history (DupeAlive unknown, so only the run rule applies)."""
    seg, vol = 10_000, 3_000_000
    n = vol // seg
    primary = []
    for i in range(6):
        missing = {p for p in range(1, n + 1) if missing_of(i * n + p - 1)}
        primary.append(('%sA/d%d.bin' % (tag, i), 'Part%d.bin' % i, vol, seg, missing))
        t.write_file(os.path.join('data', primary[-1][0]), _payload(vol, 9760 + i))
    api = daemon.wait_ready()
    daemon.append(api, 'Primary', build_multi_nzb(primary), True, tag + '-key', 100)
    if backup:
        data = _payload(2_900_000, 9769)
        bp = _place_copy(t, '%sB' % tag, data)
        daemon.append(api, 'Backup', build_nzb(bp, 'Backup.bin', 2_900_000, 100_000, set()), False, tag + '-key', 90)
        daemon.wait_history(api, 'Backup', timeout=60)
    api.editqueue('GroupResume', 0, '', [g['NZBID'] for g in api.listgroups() if g['NZBName'] == 'Primary'])
    hp = daemon.wait_history(api, 'Primary', timeout=300)
    return hp, _grep_log(t, 'failed in a row')


def scenario_runswap(daemon, t):
    """B49a: after its first article, 59 articles of the first file in a row are
    missing, the rest exist; a backup waits: the pick is swapped at the 40th
    failure in a row."""
    hp, swaps = _run_case(daemon, t, 'ra', lambda k: 1 <= k <= 59, True)
    failed = int(hp.get('FailedArticles', 0))
    return ('runswap', swaps == 1 and failed <= 59, 'status=%s run_swap_logs=%d failed=%d' % (hp['Status'], swaps, failed))


def scenario_runnobackup(daemon, t):
    """B49b: the same run with no backup: the download goes on."""
    hp, swaps = _run_case(daemon, t, 'rb', lambda k: 1 <= k <= 59, False)
    return ('runnobackup', swaps == 0,
            'status=%s run_swap_logs=%d' % (hp['Status'], swaps))


def scenario_runscattered(daemon, t):
    """B49c: failures scattered, never more than 8 in a row: no swap for a run."""
    hp, swaps = _run_case(daemon, t, 'rc', lambda k: k % 12 < 8 and k % 300 != 0, True)
    return ('runscattered', swaps == 0, 'status=%s run_swap_logs=%d' % (hp['Status'], swaps))


def scenario_runreset(daemon, t):
    """B49d: two runs of 30 failures with articles arriving between them: the
    count starts again after the arrivals, so no swap."""
    hp, swaps = _run_case(daemon, t, 'rd', lambda k: 1 <= k <= 30 or 61 <= k <= 90, True)
    return ('runreset', swaps == 0, 'status=%s run_swap_logs=%d' % (hp['Status'], swaps))


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
    alive_logs = _grep_log(t, 'not abandoning it')
    return ('deadpickpartial', failed_over == 0 and alive_logs == 1,
            'status=%s failover_logs=%d alive_probe_logs=%d' % (hp['Status'], failed_over, alive_logs))


def scenario_yencrangefar(daemon, t):
    """F23/F24: an article's yEnc range lies far past the file size it declares
    (here the last part says begin=9000001 end=9500000 of a 1.5 MB file). The
    file was filled with zeros up to that offset (a crafted or damaged posting
    could fill the disk); now the article is malformed: it fails, and the file
    never grows past its declared size."""
    size, seg = 1_500_000, 500_000
    data = _payload(size, 5400)
    pp = _place_copy(t, 'yrA', data, 'far.bin')
    api = daemon.wait_ready()
    daemon.append(api, 'RelYR', build_nzb(pp, 'far.bin', size, seg, set()), False, 'yr-key', 100)
    h = daemon.wait_history(api, 'RelYR', timeout=120)
    sizes = [os.path.getsize(t.path(rel)) for rel in t.find_files('main') if rel.endswith('far.bin')]
    rewritten = daemon.proxy.rewritten if daemon.proxy else 0
    ok = rewritten >= 1 and sizes and max(sizes) <= size
    return ('yencrangefar', ok, 'status=%s rewritten=%d sizes=%s failed=%s' % (
        h['Status'], rewritten, sizes, h.get('FailedArticles')))


def scenario_yencrangeshort(daemon, t):
    """An article's yEnc range is shorter than its data (the last part says
    end=1000100 of 1,000,001..1,500,000, its body decodes to 500 KB with a valid
    crc), the article cache on: the article fails. It counted as finished, its
    size counted the bytes past the range, and the cache padded the segment with
    memory never written (into the file)."""
    size, seg = 1_500_000, 500_000
    data = _payload(size, 8383)
    pp = _place_copy(t, 'ysA', data)
    api = daemon.wait_ready()
    daemon.append(api, 'RelYS', build_nzb(pp, 'short.bin', size, seg, set()), False, 'ys-key', 100)
    h = daemon.wait_history(api, 'RelYS', timeout=120)
    integ = _verify_output(t, data)
    wrong_success = h['Status'].startswith('SUCCESS') and not integ
    rewritten = daemon.proxy.rewritten if daemon.proxy else 0
    ok = rewritten >= 1 and not wrong_success and int(h.get('FailedArticles', 0)) >= 1
    return ('yencrangeshort', ok, 'status=%s integrity=%s failed=%s rewritten=%d' % (
        h['Status'], integ, h.get('FailedArticles'), rewritten))


def scenario_quotafutureday(daemon, t):
    """The clock went back a day or more since the volume stats were saved
    (here the saved first day and save time are moved 5 days ahead), with a
    daily quota set:
    the quota check read day slot -1 (std::out_of_range) and the daemon died
    at the start. Now it stays up and downloads."""
    data = _payload(300_000, 8484)
    pp = _place_copy(t, 'qfA', data)
    api = daemon.wait_ready()
    daemon.append(api, 'RelQF1', build_nzb(pp, 'first.bin', len(data), 100_000, set()), False, 'qf-key1', 100)
    daemon.wait_history(api, 'RelQF1', timeout=60)
    try:
        api.shutdown()
    except Exception:
        pass
    t.procs[-1].wait(timeout=60)
    stats = t.path('main', 'queue', 'stats')
    today = int(time.time()) // 86400
    lines = open(stats).read().split('\n')
    moved = 0
    for i, line in enumerate(lines):
        m = re.match(r'^(\d+),(-?\d+),(-?\d+),(-?\d+)$', line)
        if m and abs(int(m.group(1)) - today) <= 2:
            lines[i] = '%d,%d,%s,%s' % (today + 5, int(m.group(2)) + 5 * 86400, m.group(3), m.group(4))
            moved += 1
    open(stats, 'w').write('\n'.join(lines))
    daemon.start()
    api = daemon.wait_ready()
    time.sleep(4)
    data2 = _payload(300_000, 8485)
    pp2 = _place_copy(t, 'qfB', data2)
    daemon.append(api, 'RelQF2', build_nzb(pp2, 'second.bin', len(data2), 100_000, set()), False, 'qf-key2', 100)
    h = daemon.wait_history(api, 'RelQF2', timeout=60)
    alive = t.procs[-1].poll() is None
    ok = moved >= 1 and alive and h['Status'].startswith('SUCCESS')
    return ('quotafutureday', ok, 'moved=%d alive=%s status=%s' % (moved, alive, h['Status']))


def scenario_connhold(daemon, t):
    """Idle connections are held for 5 seconds before they're closed. The
    hold took the longest idle time of a level's connections, and a connection
    never used counts as idle since 1970: whenever none was in use, all were
    closed at once (a download soon after logged in again). Now the
    connection stays open 2 seconds after a one-article download and is
    closed once the hold is over."""
    data = _payload(90_000, 8686)
    pa = _place_copy(t, 'chA', data)
    api = daemon.wait_ready()
    daemon.append(api, 'RelCH1', build_nzb(pa, 'first.bin', len(data), 100_000, set()), False, 'ch-key1', 100)
    h = daemon.wait_history(api, 'RelCH1', timeout=60)
    time.sleep(2)
    open_held = daemon.proxy.accepted - daemon.proxy.closed
    time.sleep(8)
    open_after = daemon.proxy.accepted - daemon.proxy.closed
    ok = h['Status'].startswith('SUCCESS') and open_held >= 1 and open_after == 0
    return ('connhold', ok, 'status=%s open 2s after=%d 10s after=%d (accepted=%d)' % (
        h['Status'], open_held, open_after, daemon.proxy.accepted))

# a scan extension that marks every nzb it sees
CATEGORY_SCAN_EXTENSION = '''#!/usr/bin/env python3
##############################################################################
### NZBGET SCAN SCRIPT                                                     ###
# Marks the nzb.
### NZBGET SCAN SCRIPT                                                     ###
##############################################################################
import sys
print('[NZB] NZBPR_scanmark=yes')
sys.exit(0)
'''


def scenario_categoryscan(daemon, t):
    """A scan extension set only in a category's Extensions (none in the
    global Extensions) never ran: the scanner checked the global list alone
    to decide whether scan scripts exist. Now it runs for the category's
    nzbs."""
    data = _payload(90_000, 8787)
    pp = _place_copy(t, 'csA', data)
    api = daemon.wait_ready()
    daemon.append(api, 'RelCS', build_nzb(pp, 'cat.bin', len(data), 100_000, set()), False, 'cs-key', 100)
    h = daemon.wait_history(api, 'RelCS', timeout=60)
    params = {p['Name']: p['Value'] for p in h.get('Parameters', [])}
    ok = h['Status'].startswith('SUCCESS') and params.get('scanmark') == 'yes'
    return ('categoryscan', ok, 'status=%s category=%s params=%s' % (h['Status'], h.get('Category'), params))


# a scan extension that sends a 3,000-character parameter and dupe key
LONG_COMMAND_EXTENSION = '''#!/usr/bin/env python3
##############################################################################
### NZBGET SCAN SCRIPT                                                     ###
# Sends long commands.
### NZBGET SCAN SCRIPT                                                     ###
##############################################################################
import sys
print('[NZB] NZBPR_longmark=' + 'v' * 3000 + 'end')
print('[NZB] DUPEKEY=' + 'k' * 3000 + 'end')
sys.exit(0)
'''


def scenario_scanlongcommand(daemon, t):
    """[NZB] commands from a script longer than about 1,000 characters were
    cut to fit a 1,024-byte log buffer before they were parsed: a long
    parameter value or dupe key was silently shortened. Now they arrive
    whole."""
    data = _payload(90_000, 8888)
    pp = _place_copy(t, 'lcA', data)
    api = daemon.wait_ready()
    daemon.append(api, 'RelLC', build_nzb(pp, 'long.bin', len(data), 100_000, set()), False, 'lc-key', 100)
    h = daemon.wait_history(api, 'RelLC', timeout=60)
    params = {p['Name']: p['Value'] for p in h.get('Parameters', [])}
    mark = params.get('longmark', '')
    key = h.get('DupeKey', '')
    ok = h['Status'].startswith('SUCCESS') and mark == 'v' * 3000 + 'end' and key == 'k' * 3000 + 'end'
    return ('scanlongcommand', ok, 'status=%s param_len=%d key_len=%d' % (h['Status'], len(mark), len(key)))


def _inner_archive_keep(daemon, t, name):
    """A rar whose content is itself an archive (setup.7z), UseTempUnpackDir=no,
    UnpackCleanupDisk=yes, no InterDir: the cleanup deleted every *.7z/*.rar in the
    folder, the extracted setup.7z too, and reported success. Now only the
    downloaded volumes are deleted."""
    inner = _payload(600_000, 8989)
    volumes = generators.rar3_store_volumes_valid('setup.7z', inner, 300_000)
    members = []
    for i, vol in enumerate(volumes, 1):
        rel = 'iaA/rel.part%02d.rar' % i
        t.write_file(os.path.join('data', rel), vol)
        members.append((rel, 'Rel.part%02d.rar' % i, len(vol), 500_000, set()))
    api = daemon.wait_ready()
    daemon.append(api, 'RelIA', build_multi_nzb(members), False, 'ia-key', 100)
    h = daemon.wait_history(api, 'RelIA', timeout=180)
    files = {os.path.basename(rel): rel for rel in t.find_files('main', 'dst')}
    kept = 'setup.7z' in files and t.read_file(files['setup.7z']) == inner
    volumes_left = [f for f in files if re.search(r'Rel\.part\d+\.rar$', f)]
    ok = h['Status'].startswith('SUCCESS') and kept and not volumes_left
    return (name, ok, 'status=%s setup.7z_kept=%s volumes_left=%s files=%s' % (
        h['Status'], kept, volumes_left, sorted(files)))


def scenario_innerarchivekeep(daemon, t):
    return _inner_archive_keep(daemon, t, 'innerarchivekeep')


def scenario_innerarchivekeepdirect(daemon, t):
    """As innerarchivekeep, with the archive extracted by direct unpack."""
    return _inner_archive_keep(daemon, t, 'innerarchivekeepdirect')


def _serve_rss(items, delay=0.0):
    """A local http server answering every GET with an RSS feed of ``items``
    (title, url); returns its port."""
    import http.server
    body = ('<?xml version="1.0"?><rss version="2.0"><channel><title>t</title>' + ''.join(
        '<item><title>%s</title><link>%s</link><enclosure url="%s" length="1000" type="application/x-nzb"/></item>'
        % (title, url, url) for title, url in items) + '</channel></rss>').encode()

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            time.sleep(delay)
            self.send_response(200)
            self.send_header('Content-Type', 'application/rss+xml')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass

    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv.server_address[1]


def scenario_feedpreviewparallel(daemon, t):
    """Previews of a configured feed (id 1) at the same time as its fetch: the
    previews wrote the feed's own temp file (feed-1.tmp), so parallel ones
    read each other's half-written or deleted file. Every preview now gets
    its own file and returns all items."""
    import concurrent.futures
    items = [('Item.%d' % i, 'http://127.0.0.1:9/item%d.nzb' % i) for i in range(5)]
    port = _serve_rss(items, delay=0.3)
    url = 'http://127.0.0.1:%d/rss' % port
    daemon.wait_ready()

    def preview(_):
        r = _rpc(daemon, 'previewfeed', [1, 'f1', url, '', False, True, '', 0, 0, '', True, 0, ''], timeout=60)
        return len(r.get('result') or []), r.get('error')

    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        fetch = pool.submit(_rpc, daemon, 'fetchfeed', [1])
        results = list(pool.map(preview, range(8)))
        fetch.result()
    ok = all(n == len(items) and not err for n, err in results)
    return ('feedpreviewparallel', ok, 'previews=%s' % results)


def _next_minute_task_options():
    at = time.localtime(time.time() + 60)
    return ['Task1.Time=%02d:%02d' % (at.tm_hour, at.tm_min), 'Task1.Command=unpausedownlaod']


def scenario_tasktypo(daemon, t):
    """A misspelled Task1.Command (unpausedownlaod) was logged as an invalid
    value and then became the first command, pausedownload: once the user
    resumed after the configuration error, the task paused downloads at its
    time every day. The task is skipped now."""
    api = daemon.wait_ready()
    _rpc(daemon, 'resume', [])
    resumed = not _rpc(daemon, 'status', []).get('result', {}).get('DownloadPaused')
    time.sleep(75)
    paused = _rpc(daemon, 'status', []).get('result', {}).get('DownloadPaused')
    errors = _grep_log(t, 'Invalid value for option "Task1.Command"')
    ok = resumed and not paused and errors >= 1
    return ('tasktypo', ok, 'resumed=%s paused_after_task_time=%s invalid_logs=%d' % (resumed, paused, errors))


def scenario_scriptdirlist(daemon, t):
    """ScriptDir with two folders (scripts;scripts2, relative to MainDir): the
    whole value was made absolute and created as one path, a junk folder
    "scripts;scripts2" in MainDir, and the second folder was taken relative to
    the working directory. Each folder is resolved and created on its own now,
    and an extension in the first one still runs."""
    data = _payload(90_000, 9090)
    pp = _place_copy(t, 'sdA', data)
    api = daemon.wait_ready()
    daemon.append(api, 'RelSD', build_nzb(pp, 'sd.bin', len(data), 100_000, set()), False, 'sd-key', 100)
    h = daemon.wait_history(api, 'RelSD', timeout=60)
    params = {p['Name']: p['Value'] for p in h.get('Parameters', [])}
    main = t.path('main')
    junk = [d for d in os.listdir(main) if ';' in d or ',' in d]
    second = os.path.isdir(os.path.join(main, 'scripts2'))
    errors = _grep_log(t, 'Invalid value for option "ScriptDir"')
    ok = params.get('scanmark') == 'yes' and not junk and second and errors == 0
    return ('scriptdirlist', ok, 'scanmark=%s junk=%s scripts2=%s errors=%d' % (params.get('scanmark'), junk, second, errors))


def scenario_urlredirects(daemon, t):
    """URL downloads through three kinds of request:
    - a relative redirect with "://" in its query (/get?u=https://...) was taken
      for an absolute URL with the protocol "/get?u=https" and failed;
    - a protocol-relative redirect (//host:port/path) was requested as a path on
      the old host;
    - a url of about 1,500 characters lost "HTTP/1.0" and its line end in a
      1,024-byte buffer, a malformed request.
    All three fetch their nzb now."""
    import http.server
    nzbs = {}
    for tag, seed in (('A', 9191), ('B', 9192), ('C', 9193)):
        data = _payload(90_000, seed)
        pp = _place_copy(t, 'ur' + tag, data)
        nzbs[tag] = build_nzb(pp, 'url%s.bin' % tag, len(data), 100_000, set()).encode()
    port_box = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            path = self.path
            if path == '/a':
                self._redirect('/get?u=https://example.invalid/x.nzb')
            elif path.startswith('/get?u='):
                self._nzb('A')
            elif path == '/b':
                self._redirect('//127.0.0.1:%d/other/b.nzb' % port_box[0])
            elif path == '/other/b.nzb':
                self._nzb('B')
            elif path.startswith('/long?'):
                self._nzb('C')
            else:
                self.send_response(404)
                self.end_headers()

        def _redirect(self, location):
            self.send_response(302)
            self.send_header('Location', location)
            self.send_header('Content-Length', '0')
            self.end_headers()

        def _nzb(self, tag):
            self.send_response(200)
            self.send_header('Content-Type', 'application/x-nzb')
            self.send_header('Content-Length', str(len(nzbs[tag])))
            self.end_headers()
            self.wfile.write(nzbs[tag])

        def log_message(self, *args):
            pass

    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    port_box.append(srv.server_address[1])
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    base = 'http://127.0.0.1:%d' % port_box[0]
    api = daemon.wait_ready()
    urls = {'RelURA': base + '/a', 'RelURB': base + '/b', 'RelURC': base + '/long?sig=' + 'q' * 1500}
    for name, url in urls.items():
        _rpc(daemon, 'appendurl', [name + '.nzb', url, '', 0, False, False, name.lower(), 0, 'SCORE', []])
    statuses = {}
    for name in urls:
        try:
            statuses[name] = daemon.wait_history(api, name, timeout=60)['Status']
        except RuntimeError:
            statuses[name] = 'TIMEOUT'
    srv.shutdown()
    ok = all(st.startswith('SUCCESS') for st in statuses.values())
    return ('urlredirects', ok, 'statuses=%s' % statuses)


def scenario_urlclosed(daemon, t):
    """A server that closes the connection without answering: the first-line
    check read the line buffer instead of the read's result, so the "closed by
    remote host" case (a connection error) was never taken, and the url
    failed with "URL ... failed: " and whatever the buffer held."""
    srv = socket.socket()
    srv.bind(('127.0.0.1', 0))
    srv.listen(16)
    port = srv.getsockname()[1]

    def serve():
        while True:
            try:
                conn, _ = srv.accept()
            except OSError:
                return
            conn.recv(65536)
            conn.close()
    threading.Thread(target=serve, daemon=True).start()
    api = daemon.wait_ready()
    _rpc(daemon, 'appendurl', ['RelUC.nzb', 'http://127.0.0.1:%d/x.nzb' % port, '', 0, False, False, 'uc', 0, 'SCORE', []])
    try:
        status = daemon.wait_history(api, 'RelUC', timeout=90)['Status']
    except RuntimeError:
        status = 'TIMEOUT'
    srv.close()
    closed = _grep_log(t, 'Connection closed by remote host')
    garbage = _grep_log(t, 'RelUC.nzb failed: ')
    ok = status.startswith('FAILURE') and closed >= 1 and garbage == 0
    return ('urlclosed', ok, 'status=%s closed_logs=%d failed_logs=%d' % (status, closed, garbage))


def scenario_urlretrywait(daemon, t):
    """UrlInterval (20 s) longer than UrlTimeout + 10 (12 s), a server that always
    answers 500, UrlRetries=2: the wait between tries didn't count as
    activity, so the download was cancelled as hanging and restarted with
    all its retries again - forever. Now it fails after its two tries."""
    import http.server

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(500)
            self.send_header('Content-Length', '0')
            self.end_headers()

        def log_message(self, *args):
            pass

    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    api = daemon.wait_ready()
    _rpc(daemon, 'appendurl', ['RelUW.nzb', 'http://127.0.0.1:%d/x.nzb' % srv.server_address[1], '', 0, False, False, 'uw', 0, 'SCORE', []])
    try:
        status = daemon.wait_history(api, 'RelUW', timeout=60)['Status']
    except RuntimeError:
        status = 'TIMEOUT'
    srv.shutdown()
    hanging = _grep_log(t, 'Cancelling hanging url download')
    ok = status.startswith('FAILURE') and hanging == 0
    return ('urlretrywait', ok, 'status=%s hanging_cancels=%d' % (status, hanging))


def scenario_historyeditlist(daemon, t):
    """HistorySetCategory for [a normal item, a hidden duplicate record]: the
    record can't take a category, and its failure, the last in the list,
    replaced the first item's success - the call answered false and the
    change wasn't saved (gone after a crash). Now it answers true and the
    change is on disk."""
    data = _payload(90_000, 9393)
    pp = _place_copy(t, 'heA', data)
    data2 = _payload(90_000, 9394)
    pp2 = _place_copy(t, 'heB', data2)
    api = daemon.wait_ready()
    daemon.append(api, 'RelHE1', build_nzb(pp, 'he1.bin', len(data), 100_000, set()), False, 'he-key1', 100)
    daemon.append(api, 'RelHE2', build_nzb(pp2, 'he2.bin', len(data2), 100_000, set()), False, 'he-key2', 100)
    h1 = daemon.wait_history(api, 'RelHE1', timeout=60)
    h2 = daemon.wait_history(api, 'RelHE2', timeout=60)
    # deleting a finished item keeps a hidden duplicate record of it
    _rpc(daemon, 'editqueue', ['HistoryDelete', '', [h2['NZBID']]])
    time.sleep(1)
    hidden = [x for x in _rpc(daemon, 'history', [True]).get('result', []) if x.get('Kind') == 'DUP']
    dup_id = hidden[0]['NZBID'] if hidden else h2['NZBID']
    reply = _rpc(daemon, 'editqueue', ['HistorySetCategory', 'newcat', [h1['NZBID'], dup_id]]).get('result')
    t.procs[-1].kill()
    t.procs[-1].wait(timeout=30)
    daemon.start()
    api = daemon.wait_ready()
    category = next((x.get('Category') for x in api.history() if x['NZBID'] == h1['NZBID']), None)
    ok = bool(hidden) and reply is True and category == 'newcat'
    return ('historyeditlist', ok, 'hidden=%d reply=%s category_after_crash=%s' % (len(hidden), reply, category))


def scenario_speedtestnohistory(daemon, t):
    """testserverspeed with KeepHistory=0: the finished test download (no
    scripts, no disk writes) was removed from the queue and freed, and the
    cleanup then used it (use after free, a crash under AddressSanitizer).
    The cleanup runs first now; the daemon stays up."""
    import http.server
    data = _payload(300_000, 9595)
    pp = _place_copy(t, 'stA', data)
    nzb = build_nzb(pp, 'speed.bin', len(data), 100_000, set()).encode()

    hits = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            hits.append(self.path)
            self.send_response(200)
            self.send_header('Content-Type', 'application/x-nzb')
            self.send_header('Content-Length', str(len(nzb)))
            self.end_headers()
            self.wfile.write(nzb)

        def log_message(self, *args):
            pass

    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    api = daemon.wait_ready()
    url = 'http://127.0.0.1:%d/speed.nzb' % srv.server_address[1]
    reply = _rpc(daemon, 'testserverspeed', [url, 1])
    # the test download is quick: wait until the nzb was fetched and the queue is empty
    deadline = time.time() + 60
    empty = False
    while time.time() < deadline and t.procs[-1].poll() is None:
        time.sleep(1)
        try:
            empty = bool(hits) and not api.listgroups()
        except Exception:
            break
        if empty:
            time.sleep(3)
            break
    srv.shutdown()
    alive = t.procs[-1].poll() is None
    ok = alive and empty and reply.get('result') is True
    return ('speedtestnohistory', ok, 'reply=%s fetched=%d done=%s alive=%s' % (reply.get('result', reply.get('error')), len(hits), empty, alive))


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
    'recheckfailed': scenario_recheckfailed,
    'nzbgapborrow': scenario_nzbgapborrow,
    'sidefilenorepair': scenario_sidefilenorepair,
    'finaldeletemidway': scenario_finaldeletemidway,
    'finaldeleterestart': scenario_finaldeleterestart,
    'restartmidway': scenario_restartmidway,
    'declaredcountnopair': scenario_declaredcountnopair,
    'rejectnextserver': scenario_rejectnextserver,
    'cutover': scenario_cutover,
    'leadswitch': scenario_leadswitch,
    'cutovertruth': scenario_cutovertruth,
    'manydonors': scenario_manydonors,
    'stream': scenario_stream,
    'truncatedstate': scenario_truncatedstate,
    'streamrestartcredit': scenario_streamrestartcredit,
    'streamtimeout': scenario_streamtimeout,
    'streamdeaddonor': scenario_streamdeaddonor,
    'streamslowprogress': scenario_streamslowprogress,
    'streamtooslow': scenario_streamtooslow,
    'streammaxrun': scenario_streammaxrun,
    'streamstarved': scenario_streamstarved,
    'liveoverlap': scenario_liveoverlap,
    'livegate': scenario_livegate,
    'livelastfile': scenario_livelastfile,
    'repost': scenario_repost,
    'repostrenamed': scenario_repostrenamed,
    'repostobfuscated': scenario_repostobfuscated,
    'repostdonorgaps': scenario_repostdonorgaps,
    'xpackbare': scenario_xpackbare,
    'xpackrar': scenario_xpackrar,
    'xpackgapdonor': scenario_xpackgapdonor,
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
    'dupefailoverraw': scenario_dupefailoverraw,
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
    'streamgrouped': scenario_streamgrouped,
    'retrykeepsjobs': scenario_retrykeepsjobs,
    'retryparkedname': scenario_retryparkedname,
    'healthlastarticle': scenario_healthlastarticle,
    'wholefilerestart': scenario_wholefilerestart,
    'wholefilestale': scenario_wholefilestale,
    'wholefilenfoproof': scenario_wholefilenfoproof,
    'wholefilesampleproof': scenario_wholefilesampleproof,
    'dupefailoverchain': scenario_dupefailoverchain,
    'xpacklatency': scenario_xpacklatency,
    'xpackflaky': scenario_xpackflaky,
    'deadpickprobe': scenario_deadpickprobe,
    'deadpickafterreload': scenario_deadpickafterreload,
    'deadpickpartial': scenario_deadpickpartial,
    'deadpickstray': scenario_deadpickstray,
    'forcefailover': scenario_forcefailover,
    'copybackup': scenario_copybackup,
    'copyoffailed': scenario_copyoffailed,
    'resendbackup': scenario_resendbackup,
    'resendfailed': scenario_resendfailed,
    'projectedswap': scenario_projectedswap,
    'runswap': scenario_runswap,
    'runnobackup': scenario_runnobackup,
    'runscattered': scenario_runscattered,
    'runreset': scenario_runreset,
    'projectednobackup': scenario_projectednobackup,
    'projectedhalfnobackup': scenario_projectedhalfnobackup,
    'projectedbelownobackup': scenario_projectedbelownobackup,
    'keepreturned': scenario_keepreturned,
    'restartfailover': scenario_restartfailover,
    'slowprobe': scenario_slowprobe,
    'runparcovers': scenario_runparcovers,
    'ondiskskip': scenario_ondiskskip,
    'ondiskgone': scenario_ondiskgone,
    'ondiskstop': scenario_ondiskstop,
    'repairlast': scenario_repairlast,
    'recheckwholefail': scenario_recheckwholefail,
    'samepostingrepair': scenario_samepostingrepair,
    'samepostinglate': scenario_samepostinglate,
    'projectedhealthy': scenario_projectedhealthy,
    'projectedworsebackup': scenario_projectedworsebackup,
    'projectededge': scenario_projectededge,
    'projectededgelead': scenario_projectededgelead,
    'parprojection': scenario_parprojection,
    'deaddownloadstray2': scenario_deaddownloadstray2,
    'deaddownloadrestart': scenario_deaddownloadrestart,
    'deadbackupsonly': scenario_deadbackupsonly,
    'backuporder': scenario_backuporder,
    'deaddownloadlate': scenario_deaddownloadlate,
    'dupesearchtrigger': scenario_dupesearchtrigger,
    'dupesearchkey': scenario_dupesearchkey,
    'dupesearchsearch': scenario_dupesearchsearch,
    'dupesearchfetch': scenario_dupesearchfetch,
    'dupesearchfilters': scenario_dupesearchfilters,
    'dupesearchdonors': scenario_dupesearchdonors,
    'dupesearchfastdead': scenario_dupesearchfastdead,
    'dupesearchgroup': scenario_dupesearchgroup,
    'dupesearchwarnings': scenario_dupesearchwarnings,
    'dupesearchwarnnocheck': scenario_dupesearchwarnnocheck,
    'dupesearchambiguous': scenario_dupesearchambiguous,
    'dupesearchrestartcheck': scenario_dupesearchrestartcheck,
    'dupesearchalldead': scenario_dupesearchalldead,
    'dupesearchtwinmember': scenario_dupesearchtwinmember,
    'dupesearchrerank': scenario_dupesearchrerank,
    'dupesearchfailedfirst': scenario_dupesearchfailedfirst,
    'dupesearchfailedrestart': scenario_dupesearchfailedrestart,
    'dupesearchtwopicks': scenario_dupesearchtwopicks,
    'dupesearchindexerdown': scenario_dupesearchindexerdown,
    'dupesearchquerydrop': scenario_dupesearchquerydrop,
    'fleetdeadfirst': scenario_fleetdeadfirst,
    'fleettwins': scenario_fleettwins,
    'fleettimeout': scenario_fleettimeout,
    'fleetone': scenario_fleetone,
    'fleetalldead': scenario_fleetalldead,
    'fleetshutdown': scenario_fleetshutdown,
    'fleetallerror': scenario_fleetallerror,
    'fleetotherkey': scenario_fleetotherkey,
    'fleetnokey': scenario_fleetnokey,
    'fleetresend': scenario_fleetresend,
    'fleetresendslow': scenario_fleetresendslow,
    'fleetaddbackup': scenario_fleetaddbackup,
    'fleetfailover': scenario_fleetfailover,
    'fleetbusy': scenario_fleetbusy,
    'fleetdeadtwins': scenario_fleetdeadtwins,
    'fleetlarge': scenario_fleetlarge,
    'fleetcopy': scenario_fleetcopy,
    'fleetmostlydead': scenario_fleetmostlydead,
    'fleetpaused': scenario_fleetpaused,
    'fleetparallel': scenario_fleetparallel,
    'fleetscoremax': scenario_fleetscoremax,
    'fleetsamekey': scenario_fleetsamekey,
    'fleetwide': scenario_fleetwide,
    'fleetslowurlfirst': scenario_fleetslowurlfirst,
    'fleetmerged': scenario_fleetmerged,
    'appendconcurrent': scenario_appendconcurrent,
    'dupesearchgzip': scenario_dupesearchgzip,
    'dupesearchbareurl': scenario_dupesearchbareurl,
    'appendurlgzip': scenario_appendurlgzip,
    'readdafterdelete': scenario_readdafterdelete,
    'appendscorerange': scenario_appendscorerange,
    'apiedges': scenario_apiedges,
    'apiformat': scenario_apiformat,
    'apiaccess': scenario_apiaccess,
    'queueedits': scenario_queueedits,
    'directunpackkeep': scenario_directunpackkeep,
    'jointwosets': scenario_jointwosets,
    'staleprogress': scenario_staleprogress,
    'authrejected': scenario_authrejected,
    'appendlongname': scenario_appendlongname,
    'editscorerange': scenario_editscorerange,
    'appendlarge': scenario_appendlarge,
    'appendurlodd': scenario_appendurlodd,
    'addstorm': scenario_addstorm,
    'longstateline': scenario_longstateline,
    'articledecoy': scenario_articledecoy,
    'articledecoypar': scenario_articledecoypar,
    'yencrangefar': scenario_yencrangefar,
    'yencrangeshort': scenario_yencrangeshort,
    'quotafutureday': scenario_quotafutureday,
    'connhold': scenario_connhold,
    'categoryscan': scenario_categoryscan,
    'scanlongcommand': scenario_scanlongcommand,
    'innerarchivekeep': scenario_innerarchivekeep,
    'innerarchivekeepdirect': scenario_innerarchivekeepdirect,
    'feedpreviewparallel': scenario_feedpreviewparallel,
    'tasktypo': scenario_tasktypo,
    'scriptdirlist': scenario_scriptdirlist,
    'urlredirects': scenario_urlredirects,
    'urlclosed': scenario_urlclosed,
    'urlretrywait': scenario_urlretrywait,
    'historyeditlist': scenario_historyeditlist,
    'speedtestnohistory': scenario_speedtestnohistory,
    'newlinestate': scenario_newlinestate,
    'idsafterunreadable': scenario_idsafterunreadable,
    'fleetduringpost': scenario_fleetduringpost,
    'fleetslowurl': scenario_fleetslowurl,
    'dupesearchpickdeleted': scenario_dupesearchpickdeleted,
    'dupesearchpickgoneadd': scenario_dupesearchpickgoneadd,
    'dupesearchresubmit': scenario_dupesearchresubmit,
    'dupesearchquickstop': scenario_dupesearchquickstop,
    'dupesearchkeychanged': scenario_dupesearchkeychanged,
    'dupesearchresume': scenario_dupesearchresume,
    'dupesearchresumedeleted': scenario_dupesearchresumedeleted,
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
    'truncatedstate': ['ContinuePartial=yes', 'FlushQueue=yes', 'Server1.Connections=1'],
    'streamrestartcredit': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'streamdeaddonor': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'rejectnextserver': ['DupeArticleFallback=article', 'ArticleRetries=0'],
    'nzbgapborrow': ['DupeArticleFallback=article'],
    'sidefilenorepair': ['DupeArticleFallback=stream'],
    'finaldeletemidway': ['DupeArticleFallback=live', 'DirectRename=yes', 'HealthCheck=dupe', 'ArticleCache=8192', 'ContinuePartial=yes', 'DirectUnpack=yes'],
    'finaldeleterestart': ['DupeArticleFallback=live', 'DirectRename=yes', 'HealthCheck=dupe', 'ArticleCache=8192', 'ContinuePartial=yes', 'DirectUnpack=yes'],
    'restartmidway': ['DupeArticleFallback=live', 'DirectRename=yes', 'HealthCheck=dupe'],
    'declaredcountnopair': ['DupeArticleFallback=article'],
    'recheckfailed': ['DupeArticleFallback=article', 'ArticleRetries=0', 'Server1.Connections=2'],
    'streamgrouped': ['DupeArticleFallback=stream', 'ParCheck=auto', 'ArticleTimeout=20', 'Server1.Group=1'],
    'retrykeepsjobs': ['DupeArticleFallback=stream', 'ParCheck=auto', 'HealthCheck=park'],
    'retryparkedname': ['DupeArticleFallback=stream', 'ParCheck=auto', 'HealthCheck=park'],
    'healthlastarticle': ['DupeArticleFallback=stream', 'ParCheck=auto', 'HealthCheck=park', 'Server1.Connections=1'],
    'streamslowprogress': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DupeStreamTimeout=5'],
    'streamtooslow': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DupeStreamTimeout=5'],
    'streammaxrun': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DupeStreamTimeout=8'],
    'streamstarved': ['DupeArticleFallback=stream', 'ParCheck=auto', 'DupeStreamTimeout=5', 'Server1.Connections=2'],
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
    'xpackgapdonor': ['DupeArticleFallback=stream', 'ParCheck=auto'],
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
    'dupefailoverraw': ['DupeArticleFallback=article', 'HealthCheck=dupe', 'RawArticle=yes'],
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
    'wholefilestale': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilenfoproof': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'wholefilesampleproof': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'dupefailoverchain': ['DupeArticleFallback=article', 'HealthCheck=dupe'],
    'xpacklatency': ['DupeArticleFallback=stream', 'ParCheck=auto', 'Server1.Connections=8'],
    'xpackflaky': ['DupeArticleFallback=stream', 'ParCheck=auto', 'Server1.Connections=4'],
    'deadpickprobe': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'deadpickafterreload': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'deadpickpartial': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'deadpickstray': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'deaddownloadlate': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'forcefailover': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'copybackup': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'copyoffailed': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'resendbackup': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'resendfailed': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'projectedswap': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'runswap': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=1'],
    'runnobackup': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=1'],
    'runscattered': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=1'],
    'runreset': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=1'],
    'projectednobackup': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'projectedhalfnobackup': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'projectedbelownobackup': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'keepreturned': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'restartfailover': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'slowprobe': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=4'],
    'runparcovers': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'ParCheck=manual'],
    'ondiskskip': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'ondiskgone': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'ondiskstop': ['DupeArticleFallback=stream', 'ParCheck=auto', 'HealthCheck=dupe', 'DupeStreamTimeout=60', 'PostStrategy=rocket'],
    'repairlast': ['DupeArticleFallback=stream', 'ParCheck=auto', 'HealthCheck=dupe'],
    'recheckwholefail': ['DupeArticleFallback=article', 'HealthCheck=dupe', 'ArticleRetries=0'],
    'samepostingrepair': ['DupeArticleFallback=stream', 'ParCheck=auto', 'HealthCheck=dupe'],
    'samepostinglate': ['DupeArticleFallback=stream', 'ParCheck=auto', 'HealthCheck=dupe'],
    'projectedhealthy': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'projectedworsebackup': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'projectededge': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'projectededgelead': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'parprojection': ['DupeArticleFallback=stream', 'ParCheck=manual'],
    'deaddownloadstray2': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'deaddownloadrestart': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'ContinuePartial=yes'],
    'deadbackupsonly': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'backuporder': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'dupesearchtrigger': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchUrl=http://127.0.0.1:9/api', 'DupeSearchDelay=2'],
    'dupesearchkey': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchUrl=http://127.0.0.1:9/api', 'DupeSearchDelay=2'],
    'dupesearchdonor': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchUrl=http://127.0.0.1:9/api', 'DupeSearchDelay=2'],
    'dupesearchsearch': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=1', 'DupeSearchApiKey=SECRETKEY123'],
    'dupesearchbareurl': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=1', 'DupeSearchApiKey=SECRETKEY123'],
    'dupesearchgzip': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=1', 'DupeSearchApiKey=SECRETKEY123'],
    'dupesearchfetch': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=1', 'DupeSearchApiKey=k'],
    'dupesearchfetcherror': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=1', 'DupeSearchApiKey=k'],
    'dupesearchfilters': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k'],
    'dupesearchdonors': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchresumedeleted': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchresume': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchresubmit': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchpickdeleted': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchquickstop': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k',
                            'DupeFastDonors=0', 'DupeHealthBudget=120'],
    'dupesearchpickgoneadd': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2',
                              'Extensions=deletepick'],
    'dupesearchkeychanged': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchgroup': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchwarnings': ['DupeSearch=yes', 'DupeSearchUrl=http://127.0.0.1:9/api', 'HealthCheck=none', 'Server1.Connections=2'],
    'dupesearchwarnnocheck': ['DupeSearch=yes', 'DupeSearchUrl=http://127.0.0.1:9/api', 'DupeCheck=no'],
    'dupesearchambiguous': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchtwopicks': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k'],
    'dupesearchindexerdown': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k'],
    'dupesearchquerydrop': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k'],
    'fleetdeadfirst': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleettwins': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleettimeout': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetone': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetalldead': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetshutdown': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetallerror': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetotherkey': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetnokey': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetresend': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetresendslow': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetaddbackup': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetfailover': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetbusy': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'fleetdeadtwins': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetlarge': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetcopy': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetmostlydead': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetpaused': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'jointwosets': ['Unpack=yes', 'UnrarCmd=/usr/bin/unrar', 'SevenZipCmd=/usr/bin/7z'],
    'directunpackkeep': ['Unpack=yes', 'DirectUnpack=yes', 'UseTempUnpackDir=no', 'UnrarCmd=/usr/bin/unrar', 'UnpackCleanupDisk=yes', 'ParCheck=auto'],
    'apiaccess': ['ControlPassword=ctlpass', 'RestrictedUsername=ro', 'RestrictedPassword=ropass'],
    'articledecoy': ['DupeArticleFallback=article', 'HealthCheck=dupe', 'ParCheck=auto'],
    'articledecoypar': ['DupeArticleFallback=article', 'HealthCheck=dupe', 'ParCheck=auto'],
    'fleetslowurlfirst': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetwide': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetsamekey': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'yencrangefar': ['DupeArticleFallback=no', 'DirectWrite=no', 'ArticleRetries=0'],
    'yencrangeshort': ['DupeArticleFallback=no', 'ArticleCache=64', 'ArticleRetries=0'],
    'quotafutureday': ['DailyQuota=100000'],
    'categoryscan': ['Category1.Name=test', 'Category1.Extensions=catscan'],
    'tasktypo': lambda: _next_minute_task_options(),
    'scriptdirlist': ['ScriptDir=scripts;scripts2', 'Extensions=catscan'],
    'urlclosed': ['UrlRetries=1', 'UrlInterval=1'],
    'urlretrywait': ['UrlTimeout=2', 'UrlInterval=20', 'UrlRetries=2'],
    'historyeditlist': ['DupeCheck=yes'],
    'speedtestnohistory': ['KeepHistory=0'],
    'scanlongcommand': ['Extensions=longscan'],
    'innerarchivekeep': ['InterDir=', 'Unpack=yes', 'UseTempUnpackDir=no', 'UnrarCmd=/usr/bin/unrar', 'SevenZipCmd=/usr/bin/7z', 'UnpackCleanupDisk=yes'],
    'innerarchivekeepdirect': ['InterDir=', 'Unpack=yes', 'DirectUnpack=yes', 'UseTempUnpackDir=no', 'UnrarCmd=/usr/bin/unrar', 'SevenZipCmd=/usr/bin/7z', 'UnpackCleanupDisk=yes'],
    'fleetscoremax': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetmerged': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'fleetduringpost': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Extensions=slowpost'],
    'addstorm': ['NzbDirInterval=1', 'NzbDirFileAge=60', 'UrlConnections=4', 'UrlForce=yes', 'HealthCheck=dupe', 'DupeArticleFallback=live'],
    'fleetparallel': ['DupeArticleFallback=no', 'HealthCheck=dupe', 'Server1.Connections=2'],
    'fleetslowurl': ['DupeArticleFallback=no', 'HealthCheck=dupe'],
    'dupesearchrestartcheck': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=0', 'DupeHealthBudget=120'],
    'dupesearchrerank': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchfailedfirst': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=15', 'DupeSearchApiKey=k', 'HealthCheck=dupe'],
    'dupesearchfailedrestart': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=15', 'DupeSearchApiKey=k', 'HealthCheck=dupe'],
    'dupesearchalldead': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchtwinmember': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchfastdead': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchdryrun': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDryRun=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    'dupesearchrescorefail': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchDelay=2', 'DupeSearchApiKey=k', 'DupeFastDonors=2'],
    # an indexer that answers (with no results): a search that learned nothing is repeated
    'dupesearchrestart': ['DupeArticleFallback=no', 'DupeSearch=yes', 'DupeSearchApiKey=k', 'DupeSearchDelay=2'],
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
SCENARIO_NSERV_ARGS = {'finaldeletemidway': ['-w', '300'], 'finaldeleterestart': ['-w', '300'], 'restartmidway': ['-w', '300'], 'xpacklatency': ['-w', '1000'], 'deadpickprobe': ['-w', '500'], 'deadpickafterreload': ['-w', '500'],
                       'deadpickpartial': ['-w', '100'], 'deadpickstray': ['-w', '500'], 'forcefailover': ['-w', '500'], 'copybackup': ['-w', '500'], 'copyoffailed': ['-w', '500'], 'deaddownloadstray2': ['-w', '300'], 'deaddownloadrestart': ['-w', '300'], 'deadbackupsonly': ['-w', '500'], 'backuporder': ['-w', '500'], 'deaddownloadlate': ['-w', '300'], 'deadpickservers': ['-w', '200'],
                       'deadpickfewservers': ['-w', '200']}

# extra news servers behind the same nserv: (servers answering, unreachable
# optional servers); Server1 counts among the answering ones
SCENARIO_EXTRA_SERVERS = {'deadpickservers': (5, 1), 'deadpickfewservers': (4, 2), 'rejectnextserver': (2, 0)}

# scenarios whose extra servers join Server1's group (the same account): Group=1
SCENARIO_GROUPED_SERVERS = {'streamgrouped': 2}

# scenarios with a FlakyNntpProxy in front of a news server:
# (server number, message-id trigger, window in s or None for good)
SCENARIO_FLAKY_PROXY = {'xpackflaky': (1, 'xfB/', 4.0), 'xpackdeadserver': (2, 'xdB/', None)}

# scenarios with a CorruptingNntpProxy in front of Server1: message-id marker
SCENARIO_CORRUPT_PROXY = {'xpackcorrupt': 'xcB/'}

# scenarios with a RewritingNntpProxy in front of Server1: (old, new) reply bytes
SCENARIO_REWRITE_PROXY = {'notfound451': (b'430 ', b'451 '),
                          'yencrangefar': (b'begin=1000001 end=1500000', b'begin=9000001 end=9500000'),
                          'yencrangeshort': (b'begin=1000001 end=1500000', b'begin=1000001 end=1000100'),
                          'connhold': (b'\x00no-such\x00', b'\x00no-such\x00'), 'rejectnextserver': (b'=ypart begin=', b'=ypart begxn=')}

# scenarios with a DelayingNntpProxy in front of Server1: [(message-id marker, delay in s)]
SCENARIO_DELAY_PROXY = {'slowprobe': [(b'STAT ', 5.0), (b'spA/', 0.2)],
                        'directunpackkeep': [(b'rel.part03', 2.0)],
                        'truncatedstate': [(b'tsA/', 0.5)],
                        'streamrestartcredit': [(b'srSlow/', 3.0)],
                        'restartfailover': [(b'rfA/', 0.4)],
                        'ondiskstop': [(b'odB/', 1.0)],
                        'streamstarved': [(b'busy/', 2.0)],
                        'keepreturned': [(b'krA/', 0.4)],
                        'streamtimeout': [(b'slowB/', 8.0)],
                        'streamslowprogress': [(b'slowA/', 2.0), (b'slowB/', 0.5)],
                        'streamtooslow': [(b'slowB/', 1.0), (b'warm/', 0.2)],
                        'streammaxrun': [(b'capA/', 2.0), (b'capB/', 1.0)]}

# scenarios with a FirstFailNntpProxy in front of Server1: (message-id markers,)
# extensions a scenario installs into ScriptDir before the daemon starts
SCENARIO_EXTENSIONS = {'dupesearchpickgoneadd': {'deletepick.py': DELETE_PICK_EXTENSION},
                       'fleetduringpost': {'slowpost.py': SLOW_POST_EXTENSION},
                       'categoryscan': {'catscan.py': CATEGORY_SCAN_EXTENSION},
                       'scriptdirlist': {'catscan.py': CATEGORY_SCAN_EXTENSION},
                       'scanlongcommand': {'longscan.py': LONG_COMMAND_EXTENSION}}

SCENARIO_FIRST_FAIL_PROXY = {'recheckfailed': ((b'?5=', b'?10=', b'?15='),)}

# scenarios with a FakeNewznab indexer (DupeSearchUrl points to it)
SCENARIO_NEWZNAB = {'dupesearchbareurl', 'dupesearchgzip', 'dupesearchrestart', 'dupesearchsearch', 'dupesearchfetch', 'dupesearchfetcherror', 'dupesearchfilters', 'dupesearchdonors',
                   'dupesearchfastdead', 'dupesearchrescorefail', 'dupesearchdryrun',
                   'dupesearchgroup', 'dupesearchrerank', 'dupesearchfailedfirst', 'dupesearchfailedrestart', 'dupesearchtwopicks', 'dupesearchindexerdown', 'dupesearchquerydrop', 'dupesearchalldead', 'dupesearchtwinmember', 'dupesearchambiguous', 'dupesearchrestartcheck', 'dupesearchresume', 'dupesearchpickdeleted', 'dupesearchpickgoneadd', 'dupesearchquickstop', 'dupesearchresubmit', 'dupesearchkeychanged',
                   'dupesearchresumedeleted'}

# scenarios with a FakeNntp news server in place of nserv
SCENARIO_FAKE_NNTP = {'fleetparallel', 'fleetduringpost', 'fleetslowurlfirst', 'fleetwide', 'fleetsamekey', 'fleetscoremax', 'fleetmerged', 'fleetslowurl', 'fleetpaused', 'fleetmostlydead', 'fleetlarge', 'fleetcopy', 'fleetbusy', 'fleetdeadtwins', 'fleetfailover', 'fleetaddbackup', 'fleetresendslow', 'fleetresend', 'fleetallerror', 'fleetotherkey', 'fleetnokey', 'fleetdeadfirst', 'fleettwins', 'fleettimeout', 'fleetone', 'fleetalldead', 'fleetshutdown', 'dupesearchresumedeleted', 'dupesearchpickdeleted', 'dupesearchpickgoneadd', 'dupesearchquickstop', 'dupesearchresubmit', 'dupesearchkeychanged', 'dupesearchresume', 'dupesearchgroup', 'dupesearchrerank', 'dupesearchfailedfirst', 'dupesearchfailedrestart', 'dupesearchtwopicks', 'dupesearchindexerdown', 'dupesearchquerydrop', 'dupesearchalldead', 'dupesearchtwinmember', 'dupesearchambiguous', 'dupesearchrestartcheck', 'dupesearchdonors', 'dupesearchfastdead', 'dupesearchrescorefail', 'dupesearchdryrun'}


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #

def main():
    ap = argparse.ArgumentParser(description='DupeArticleFallback functional harness')
    ap.add_argument('--nzbget', required=True,
                    help='path to nzbget binary (local) or ON-DEVICE path (adb)')
    ap.add_argument('--target', choices=['local', 'adb'], default='local')
    ap.add_argument('--scenario', default='all',
                    help="'all', a scenario name, or several separated by commas")
    ap.add_argument('--serial', help='adb device serial (adb target)')
    ap.add_argument('--keep', action='store_true', help='keep the workdir')
    args = ap.parse_args()

    # 'all', one name, or several separated by commas
    scenarios = list(SCENARIOS) if args.scenario == 'all' else args.scenario.split(',')
    unknown = [name for name in scenarios if name not in SCENARIOS]
    if unknown:
        ap.error('unknown scenario(s): %s' % ', '.join(unknown))
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
            first_fail = SCENARIO_FIRST_FAIL_PROXY.get(name)
            options = SCENARIO_OPTIONS.get(name, DEFAULT_OPTIONS)
            # a function gives options that depend on the start time
            options = list(options() if callable(options) else options)
            nserv_port = nntp
            proxy_port = None
            if flaky or corrupt or rewrite or delay or first_fail:
                # Server1 always is nntp: a proxy for it listens there and
                # nserv moves to a port of its own; a proxy for Server2 gets a
                # port of its own
                server = flaky[0] if flaky else 1
                proxy_port = nntp if server == 1 else free_port()
                if server == 1:
                    nserv_port = free_port()
                else:
                    options.append('Server%d.Port=%d' % (server, proxy_port))
            if name in SCENARIO_GROUPED_SERVERS:
                for number in range(2, SCENARIO_GROUPED_SERVERS[name] + 1):
                    options += ['Server%d.Host=127.0.0.1' % number, 'Server%d.Port=%d' % (number, nserv_port),
                                'Server%d.Connections=2' % number, 'Server%d.Level=0' % number,
                                'Server%d.Encryption=no' % number, 'Server%d.Group=1' % number]
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
            if name == 'authrejected':
                daemon.reject_nntp = RejectingNntp()
                options += ['Server2.Active=yes', 'Server2.Host=127.0.0.1', 'Server2.Port=%d' % daemon.reject_nntp.port,
                            'Server2.Username=u', 'Server2.Password=p', 'Server2.Level=0', 'Server2.Connections=2',
                            'Server2.Encryption=no', 'ArticleRetries=0', 'ArticleInterval=0']
            if name in SCENARIO_NEWZNAB:
                daemon.newznab = FakeNewznab()
                # (dupesearchbareurl: the address without "/api", as users write it)
                options.append('DupeSearchUrl=http://127.0.0.1:%d%s' % (daemon.newznab.port,
                                                                       '' if name == 'dupesearchbareurl' else '/api'))
            daemon.write_config(options)
            for file_name, text in SCENARIO_EXTENSIONS.get(name, {}).items():
                target.write_file(os.path.join('main', 'scripts', file_name), text.encode())
                os.chmod(target.path('main', 'scripts', file_name), 0o755)
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
            elif first_fail:
                daemon.proxy = FirstFailNntpProxy(proxy_port, nserv_port, *first_fail)
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
            failed = not results or results[-1][1] is False
            if daemon.proxy:
                daemon.proxy.close()
            if daemon.newznab:
                daemon.newznab.close()
                if failed:
                    # what the indexer was asked, for reading a failed run
                    try:
                        target.write_file('newznab-requests.log', '\n'.join(
                            '%.3f %s' % (r.get('_time', 0), {k: v for k, v in r.items() if k != '_time'})
                            for r in daemon.newznab.requests).encode())
                    except Exception:
                        pass
            if daemon.fake_nntp:
                daemon.fake_nntp.close()
            # a failed scenario keeps its work directory
            if failed and not args.keep and args.target == 'local':
                print('       kept: %s' % target.work)
            target.teardown(args.keep or failed)

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
