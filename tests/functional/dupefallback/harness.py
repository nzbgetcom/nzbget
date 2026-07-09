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
* manydonors    - more duplicates than the donor cache holds, to exercise the
  cache-eviction path (regression for the use-after-free crash).
* stream        - the donor is segmented differently; missing byte ranges are
  repaired on stream level in post-processing.
* repost        - a 4-member opaque "rar+par2 release" reposted byte-identically
  under different segmentation; damaged volume and par2 are both repaired
  byte-identically (final status FAILURE/PAR by design - the stand-in par2 is
  random bytes).
* repostrenamed - a 3-member repost whose members were RENAMED (different
  release base name, same volume suffixes): exact-name pairing cannot fire,
  proving the unique-suffix-key tier pairs the damaged member with its donor
  twin end-to-end.
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

Usage:
    harness.py --nzbget /path/to/nzbget [--target local|adb]
               [--scenario all|complementary|cutover|manydonors|stream|repost|repostrenamed|xpackbare|xpackrar|xpackrar2rar|xpackzip|xpack7z|xpacksplit|xpackcompressed|xpackneg]
               [--serial NNN] [--keep]
"""

import argparse
import base64
import os
import random
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


def build_nzb(served_path, subject_name, file_size, seg_size, missing_parts):
    """Return NZB XML for a single file served by nserv.

    ``missing_parts`` is a set of 1-based part numbers to mark with ``!2`` so
    they are served only by nserv instance 2 (i.e. missing on the active
    instance-1 server, forcing a fallback)."""
    return build_multi_nzb([(served_path, subject_name, file_size, seg_size,
                             missing_parts)])


def build_multi_nzb(members):
    """Return NZB XML containing one <file> block per member tuple
    (served_path, subject_name, file_size, seg_size, missing_parts).
    Member subject names MUST be distinct: duplicate parsed filenames make
    nzbget fall back to raw subjects and set ManyDupeFiles."""
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<!DOCTYPE nzb PUBLIC "-//newzBin//DTD NZB 1.0//EN" '
        '"http://www.newzbin.com/DTD/nzb/nzb-1.0.dtd">',
        '<nzb xmlns="http://www.newzbin.com/DTD/2003/nzb">',
    ]
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

    def spawn(self, args):
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

    def spawn(self, args):
        # launch detached on device; ports are device-local
        remote = ' '.join(self._q(a) for a in args) + ' >/dev/null 2>&1 &'
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

    def start_nserv(self):
        # A single instance (-i 1) binds only nntp_port. Instance 1 already
        # returns "430 not found" for "!2" message-ids (its id 1 is not in the
        # server-list [2]), which is how a "missing" article is simulated on the
        # only server the daemon uses. A second instance would just bind
        # nntp_port+1 and risk colliding with the control port.
        self.t.spawn([self.t.nzbget, '--nserv', '-d', self.datadir,
                      '-p', str(self.nntp_port), '-i', '1', '-v', '0'])

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
    return ('complementary', ok and integ and recov >= 3,
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
    return ('cutover', ok and integ and recov >= 10 and cut >= 1,
            'status=%s recovered=%d cutover_logs=%d integrity=%s'
            % (h['Status'], recov, cut, integ))


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
    The history status stays non-SUCCESS (the failed-article statistics
    survive; there is no par2 here) - the proof of repair is byte integrity
    plus the repair logs and the DupeRecoveredArticles counter."""
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
    return ('stream',
            integ and recov >= 4 and queued >= 1 and repaired >= 1 and rejected >= 1,
            'status=%s recovered=%d queued_logs=%d repair_logs=%d rejected_logs=%d integrity=%s'
            % (h['Status'], recov, queued, repaired, rejected, integ))


def scenario_repost(daemon, t):
    """M1 same-bytes matching: a 4-member "release" (three equal-size rar
    volumes + a small par2) where the payloads are random bytes standing in
    for a PASSWORD-PROTECTED, COMPRESSED archive - stream repair never
    interprets them, which is the point. The primary posting is missing
    blocks in part01 and in the par2; the donor is a REPOST: byte-identical
    members under the same names, cut into different article sizes. Suffix/
    name pairing must match each damaged member to its donor twin and repair
    both byte-identically (equal-size volumes prove pairing; the par2 proves
    the small-file probe fallback and scaled floor). Expected history status
    is FAILURE/PAR: ParCheck=auto runs the par stage against the stand-in
    par2, which is opaque random bytes, so par-check fails by design -
    byte integrity and the counters are the pass criteria here."""
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
    integ_par = _verify_output(t, payloads['Rel.vol00+01.par2'], '.par2', dirs=both_dirs)
    intact_2 = _verify_output(t, payloads['Rel.part02.rar'], '.rar', dirs=both_dirs)
    intact_3 = _verify_output(t, payloads['Rel.part03.rar'], '.rar', dirs=both_dirs)
    return ('repost',
            integ_rar and integ_par and intact_2 and intact_3 and
            recov >= 4 and queued >= 2 and repaired >= 2,
            'status=%s recovered=%d queued_logs=%d repair_logs=%d '
            'rar=%s par2=%s intact=%s/%s'
            % (h['Status'], recov, queued, repaired,
               integ_rar, integ_par, intact_2, intact_3))


def scenario_repostrenamed(daemon, t):
    """M1 tier-2 pairing end-to-end: the donor is a byte-identical repost
    whose members were RENAMED (different release base name, same volume
    suffixes), so exact-name pairing cannot fire and the unique-suffix-key
    tier must pair the damaged member with its donor twin. No par2 members:
    the item ends FAILURE/HEALTH; byte integrity and the counters are the
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


def scenario_xpackbare(daemon, t):
    """M2 cross-packing smoke test: the primary posts movie.mkv BARE and
    completes with a hole; the only duplicate posts the SAME movie packed
    into store-mode RAR3 volumes (different framing, different offsets,
    different segmentation). M1 cannot pair bare against rar volumes -
    the ContentMap pass must locate the missing bytes inside the donor's
    volumes and patch them. No par2: FAILURE/HEALTH expected; integrity,
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


def _xpack_run(daemon, t, tag, primary_members, donor_members, payloads):
    """Append donor (paused) + primary under one dupe-key, wait for history,
    return (history, per-payload integrity dict, log counters)."""
    primary = build_multi_nzb(primary_members)
    donor = build_multi_nzb(donor_members)
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
    damaged for par2 - which doesn't exist here, so FAILURE/HEALTH)."""
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
    ok = (integ['Rel.part01.rar'] and integ['Rel.part02.rar'] and
          not integ['Rel.part03.rar'] and              # header-holed vol stays damaged
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


def scenario_xpackzip(daemon, t):
    """Bare target repaired from a SPANNED STORED ZIP donor (z01+z02+zip)."""
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
    ok = integ['movie.mkv'] and c['recov'] >= 1 and c['repaired'] >= 1
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


def _verify_output(t, expected, ext='.bin', dirs=(('main', 'dst'),)):
    """On SUCCESS the completed file lands at main/dst/<category>/<nzb>/
    <name><ext>, whose exact path depends on category and FileNaming. When
    the history status stays non-SUCCESS (e.g. failed-article statistics
    survive a byte-level stream repair, so cleanup/move-to-dst is skipped
    and nzbget "parks" the item instead - see HistoryCoordinator's
    cleanupParkedFiles logic), the same file instead sits under
    main/inter/<nzb>.#<id>/<name><ext>. Walk the directories specified in
    'dirs' (by default, main/dst only; scenarios whose item ends parked,
    like stream, pass both main/dst and main/inter) and byte-compare every
    produced file with the given extension against the source payload."""
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


SCENARIOS = {
    'complementary': scenario_complementary,
    'cutover': scenario_cutover,
    'manydonors': scenario_manydonors,
    'stream': scenario_stream,
    'repost': scenario_repost,
    'repostrenamed': scenario_repostrenamed,
    'xpackbare': scenario_xpackbare,
    'xpackrar': scenario_xpackrar,
    'xpackrar2rar': scenario_xpackrar2rar,
    'xpackzip': scenario_xpackzip,
    'xpack7z': scenario_xpack7z,
    'xpacksplit': scenario_xpacksplit,
    'xpackcompressed': scenario_xpackcompressed,
    'xpackneg': scenario_xpackneg,
}

# per-scenario daemon options; the article-level scenarios keep the legacy
# "yes" spelling on purpose - it must still parse as "article". The stream
# scenario overrides the base config's ParCheck=manual: under "manual" the
# RequestParCheck the repair stage issues would only flag the item instead
# of running the par stage, leaving that handoff untested (with no par2
# files present, "auto" ends in a harmless "Nothing to par-check").
SCENARIO_OPTIONS = {
    'stream': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # repost: ParCheck=auto runs par-check against a random-bytes stand-in
    # par2 after the repair - FAILURE/PAR is the EXPECTED final status
    'repost': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # repostrenamed: no par2 members; "auto" ends in a harmless
    # "Nothing to par-check" after the repair handoff
    'repostrenamed': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # xpackbare: no par2; "auto" ends in "Nothing to par-check" post-repair
    'xpackbare': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    # xpack*: no real par2 anywhere; ParCheck=auto ends in "Nothing to par-check"
    'xpackrar': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackrar2rar': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackzip': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpack7z': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpacksplit': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackcompressed': ['DupeArticleFallback=stream', 'ParCheck=auto'],
    'xpackneg': ['DupeArticleFallback=stream', 'ParCheck=auto'],
}
DEFAULT_OPTIONS = ['DupeArticleFallback=yes']


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
            daemon.start_nserv()
            time.sleep(1)
            daemon.start()
            if args.target == 'adb':
                target.forward_rpc(rpc)
            time.sleep(2)
            sc_name, passed, detail = SCENARIOS[name](daemon, target)
            results.append((sc_name, passed, detail))
            print('[%s] %s  (%s)' % ('PASS' if passed else 'FAIL', sc_name, detail))
        except Exception as e:
            results.append((name, False, 'ERROR: %s' % e))
            print('[FAIL] %s  (ERROR: %s)' % (name, e))
        finally:
            target.teardown(args.keep)

    print('\n=== %s / %s scenarios passed on target=%s ===' % (
        sum(1 for _, p, _ in results if p), len(results), args.target))
    return 0 if all(p for _, p, _ in results) else 1


if __name__ == '__main__':
    sys.exit(main())
