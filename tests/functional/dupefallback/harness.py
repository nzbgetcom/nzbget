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
import time
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

    def start_nserv(self, capture_requests=False, extra_args=()):
        # A single instance (-i 1) binds only nntp_port. Instance 1 already
        # returns "430 not found" for "!2" message-ids (its id 1 is not in the
        # server-list [2]), which is how a "missing" article is simulated on the
        # only server the daemon uses. A second instance would just bind
        # nntp_port+1 and risk colliding with the control port.
        self.t.spawn([self.t.nzbget, '--nserv', '-d', self.datadir,
                      '-p', str(self.nntp_port), '-i', '1',
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
# extra nserv arguments per scenario (-w: response latency in ms)
SCENARIO_NSERV_ARGS = {'xpacklatency': ['-w', '1000']}


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
            daemon.write_config(SCENARIO_OPTIONS.get(name, DEFAULT_OPTIONS))
            daemon.start_nserv(capture_requests=(name == 'repost'),
                               extra_args=SCENARIO_NSERV_ARGS.get(name, ()))
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
