#!/usr/bin/env python3
"""Offline PAR-first regressions using real checked-in PAR2 recovery packets.

Each scenario runs a scratch NZBGet daemon and its own nserv. Repairable
downloads must use their matching parity without requesting a donor article.
Insufficient parity must still permit duplicate stream recovery, followed by a
successful PAR verification. The existing no-PAR scenarios cover compatibility.

Usage:
    python3 par_first_test.py --nzbget /path/to/nzbget [--keep]

No production configuration, download, NNTP server, or queue is accessed.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time

import harness


FIXTURES = Path(__file__).resolve().parents[2] / 'testdata' / 'parchecker'
SCENARIOS = (
    'repairable_live', 'repairable_stream',
    'insufficient_live', 'insufficient_stream',
    'unprotected_live', 'unprotected_stream',
    'no_par_live', 'no_par_stream',
)


class LoggedDaemon(harness.Daemon):
    """Capture nserv's received commands, including unsuccessful donor probes."""

    def start_nserv(self):
        with open(self.t.path('nserv.log'), 'wb') as output:
            proc = subprocess.Popen(
                [self.t.nzbget, '--nserv', '-d', self.datadir,
                 '-p', str(self.nntp_port), '-i', '1', '-v', '2'],
                stdout=output, stderr=subprocess.STDOUT,
            )
        self.t.procs.append(proc)
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                raise RuntimeError('nserv exited before accepting connections')
            try:
                with socket.create_connection(('127.0.0.1', self.nntp_port),
                                              timeout=0.2):
                    return
            except OSError:
                time.sleep(0.05)
        raise RuntimeError('nserv did not accept connections')


def inspect_parity(paths):
    """Reject a broken fixture instead of mistaking it for insufficient parity."""
    recovery = set()
    sets = set()
    block_sizes = set()
    for path in paths:
        data = path.read_bytes()
        offset = 0
        while offset < len(data):
            assert data[offset:offset + 8] == b'PAR2\0PKT', path
            length = struct.unpack_from('<Q', data, offset + 8)[0]
            assert length >= 64 and length % 4 == 0, path
            packet = data[offset:offset + length]
            assert len(packet) == length, path
            assert hashlib.md5(packet[32:]).digest() == packet[16:32], path
            sets.add(packet[32:48])
            packet_type = packet[48:64]
            if packet_type == b'PAR 2.0\0Main\0\0\0\0':
                block_sizes.add(struct.unpack_from('<Q', packet, 64)[0])
            if packet_type == b'PAR 2.0\0RecvSlic':
                recovery.add(struct.unpack_from('<I', packet, 64)[0])
            offset += length
    assert len(sets) == len(block_sizes) == 1, 'fixture must use one PAR set'
    return block_sizes.pop(), len(recovery)


def recovered_bytes(history):
    return ((int(history.get('DupeRecoveredBytesHi', 0)) & 0xffffffff) << 32
            | (int(history.get('DupeRecoveredBytesLo', 0)) & 0xffffffff))


def donor_requests(target):
    # nserv flushes its frontend every 100ms. Waiting two periods captures the
    # final command even when the history RPC sees completion immediately.
    time.sleep(0.25)
    log = target.read_file('nserv.log').decode(errors='replace')
    requests = re.findall(r'Received: (?:BODY|ARTICLE) [^\r\n]*', log)
    assert requests, 'nserv command capture is empty; cannot prove zero fetches'
    return [line for line in requests if 'par-first-donor/' in line]


def verify_named_outputs(target, expected):
    """Only final-destination bytes count as successful completed output."""
    result = {}
    outputs = target.find_files('main', 'dst')
    for name, data in expected.items():
        matches = [rel for rel in outputs if os.path.basename(rel) == name]
        result[name] = len(matches) == 1 and target.read_file(matches[0]) == data
    return result


def scenario_parity(daemon, target, name):
    insufficient = name.startswith('insufficient_')
    unprotected = name.startswith('unprotected_')
    parity_names = ['testfile.par2', 'testfile.vol00+1.PAR2']
    if not insufficient:
        parity_names += ['testfile.vol01+2.PAR2', 'testfile.vol03+3.PAR2']
    block_size, recovery_blocks = inspect_parity(
        [FIXTURES / filename for filename in parity_names])
    assert recovery_blocks == (1 if insufficient else 6)

    # One failed article covers exactly three aligned PAR blocks. Donor
    # segmentation differs, so article fallback cannot hide the PAR/stream
    # ordering under test. The hole is internal and leaves ample identity data.
    segment_size = block_size * 3
    missing_parts = {10}
    data = (FIXTURES / 'testfile.dat').read_bytes()
    nfo = (FIXTURES / 'testfile.nfo').read_bytes()
    assert len(data) > segment_size * 20
    primary_members = []
    for filename, content, holes, segment in (
            ('testfile.dat', data, missing_parts, segment_size),
            ('testfile.nfo', nfo, set(), 4096)):
        served = harness._place_copy(target, 'par-first-primary', content,
                                     filename)
        primary_members.append((served, filename, len(content), segment, holes))

    # Keep another data file downloading long enough for the live dispatcher
    # to see the damaged file. The unprotected scenarios also damage this file:
    # success of the two-file PAR set must not discard its unrelated donor job.
    tail_name = 'zz-keepalive.bin'
    tail = harness._payload(4 * 1024 * 1024, 9102026)
    tail_path = harness._place_copy(target, 'par-first-primary', tail, tail_name)
    tail_segment = 128 * 1024
    primary_members.append((tail_path, tail_name, len(tail), tail_segment,
                            {10} if unprotected else set()))

    for filename in parity_names:
        content = (FIXTURES / filename).read_bytes()
        served = harness._place_copy(target, 'par-first-primary', content,
                                     filename)
        primary_members.append((served, filename, len(content), 64 * 1024, set()))

    donor_path = harness._place_copy(target, 'par-first-donor', data,
                                     'testfile.dat')
    donor_members = [(donor_path, 'testfile.dat', len(data),
                      block_size * 5, set())]
    if unprotected:
        donor_tail = harness._place_copy(target, 'par-first-donor', tail,
                                         tail_name)
        donor_members.append((donor_tail, tail_name, len(tail),
                              96 * 1024, set()))
    donor = harness.build_multi_nzb(donor_members)
    primary = harness.build_multi_nzb(primary_members)
    api = daemon.wait_ready()
    key = 'par-first-' + name
    assert daemon.append(api, 'Donor-' + name, donor, True, key, 50) > 0
    assert daemon.append(api, 'Primary-' + name, primary, False, key, 100) > 0
    history = daemon.wait_history(api, 'Primary-' + name, timeout=90)

    integrity = verify_named_outputs(target, {
        'testfile.dat': data, 'testfile.nfo': nfo, tail_name: tail,
    })
    fetched = donor_requests(target)
    articles = int(history.get('DupeRecoveredArticles', 0))
    byte_count = recovered_bytes(history)
    log = target.read_file('nzbget.log').decode(errors='replace')
    live_runs = log.count('Starting live stream repair')
    donor_at = log.rfind('donor article(s)')
    after_donor = log[donor_at:] if donor_at >= 0 else ''
    final_full_par = ('Checking pars for' in after_donor
                      and 'Verifying file testfile.dat' in after_donor
                      and 'Quickly verified' not in after_donor)
    detail = {
        'status': history.get('Status'),
        'par_status': history.get('ParStatus'),
        'valid_recovery_blocks': recovery_blocks,
        'damaged_blocks': 3,
        'recovered_articles': articles,
        'recovered_bytes': byte_count,
        'donor_fetches': len(fetched),
        'protected_donor_fetches': sum('testfile.dat?' in req for req in fetched),
        'unprotected_donor_fetches': sum(tail_name + '?' in req for req in fetched),
        'live_stream_runs': live_runs,
        'full_par_after_donor': final_full_par,
        'integrity': integrity,
    }
    target.write_file('history.json', json.dumps(history, indent=2).encode())
    target.write_file('result.json', json.dumps(detail, indent=2).encode())

    common = (history.get('Status') == 'SUCCESS/PAR'
              and history.get('ParStatus') == 'SUCCESS'
              and all(integrity.values()))
    if unprotected:
        # Matching PAR repairs testfile.dat; only the file absent from the
        # manifest may use duplicate data. Global psSuccess is insufficient
        # evidence to throw away this second job.
        passed = (common and detail['protected_donor_fetches'] == 0
                  and detail['unprotected_donor_fetches'] > 0
                  and articles == 1 and byte_count == tail_segment)
    elif insufficient:
        # A successful final PAR result is required even after duplicate data
        # supplies the bytes which the available recovery packet cannot cover.
        passed = (common and len(fetched) > 0 and articles == 1
                  and byte_count == segment_size and final_full_par)
    else:
        passed = (common and not fetched and articles == 0 and byte_count == 0
                  and live_runs == 0)
    return passed, detail


def run_one(binary, name, keep, par_quick='no'):
    workdir = tempfile.mkdtemp(prefix='nzbget-par-first-%s-' % name)
    target = harness.LocalTarget(binary, workdir)
    nntp_port = harness.free_port()
    rpc_port = harness.free_port()
    while rpc_port == nntp_port:
        rpc_port = harness.free_port()
    daemon = LoggedDaemon(target, nntp_port, rpc_port)
    passed = False
    try:
        mode = 'live' if name.endswith('_live') else 'stream'
        options = ['DupeArticleFallback=' + mode, 'ParCheck=auto',
                   'ParQuick=' + par_quick, 'ParScan=full', 'ParRepair=yes',
                   'ParCleanupQueue=no', 'DownloadRate=1024']
        daemon.write_config(options)
        daemon.start_nserv()
        daemon.start()
        if name.startswith('no_par_'):
            scenario = (harness.scenario_liveoverlap if mode == 'live'
                        else harness.scenario_stream)
            _, passed, detail = scenario(daemon, target)
        else:
            passed, detail = scenario_parity(daemon, target, name)
        print('[%s] %s %s' % ('PASS' if passed else 'FAIL', name,
                              json.dumps(detail, sort_keys=True)), flush=True)
    except Exception as exc:
        print('[FAIL] %s %s: %s' % (name, type(exc).__name__, exc), flush=True)
    finally:
        preserve = keep or not passed
        target.teardown(preserve)
        if preserve:
            print('Artifacts: %s' % workdir, flush=True)
    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--nzbget', required=True)
    parser.add_argument('--scenario', choices=('all',) + SCENARIOS,
                        default='all')
    parser.add_argument('--keep', action='store_true')
    parser.add_argument('--par-quick', choices=('yes', 'no'), default='no',
                        help='also exercise download-time CRC verification')
    args = parser.parse_args()
    binary = str(Path(args.nzbget).resolve())
    if not os.access(binary, os.X_OK):
        parser.error('--nzbget must name an executable')
    names = SCENARIOS if args.scenario == 'all' else (args.scenario,)
    results = [run_one(binary, name, args.keep, args.par_quick) for name in names]
    print('%d/%d PAR-first scenarios passed (ParQuick=%s)' %
          (sum(results), len(results), args.par_quick), flush=True)
    return 0 if all(results) else 1


if __name__ == '__main__':
    sys.exit(main())
