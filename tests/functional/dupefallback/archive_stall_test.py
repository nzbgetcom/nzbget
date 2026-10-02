#!/usr/bin/env python3
"""Offline regressions for stalled duplicate-archive recovery.

Real LZMA2 archives are split over three volumes and hundreds of articles.
nserv supplies immediate, authoritative missing-article responses, with a
small response delay so the public progress label can also be observed.
Request counts, output bytes and final history are the assertions; elapsed
time is only a timeout, never the proof that an unavailable donor was skipped.

Usage: python3 archive_stall_test.py --nzbget /path/to/nzbget [--keep]
Only scratch configuration, downloads, queues and loopback NNTP are used.
"""

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import tempfile
import time

import generators
import harness


SCENARIOS = ('all_missing', 'missing_middle', 'repeated_then_healthy',
             'password_retry')
PRIMARY_SEGMENT = 512 * 1024
DONOR_SEGMENT = 16 * 1024
VOLUME_SIZE = 1536 * 1024
PRIMARY_HOLE = 4


class ObservedDaemon(harness.Daemon):
    def start_nserv(self):
        process = self.t.spawn(
            [self.t.nzbget, '--nserv', '-d', self.datadir,
             '-b', '127.0.0.1', '-p', str(self.nntp_port), '-i', '1',
             '-v', '2', '-w', '80'], output_rel='nserv.log')
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError('nserv exited before accepting connections')
            try:
                with socket.create_connection(('127.0.0.1', self.nntp_port),
                                              timeout=0.2):
                    return
            except OSError:
                time.sleep(0.05)
        raise RuntimeError('nserv did not accept connections')

    def wait_observed_history(self, api, name, timeout=40):
        labels = []
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for item in api.history():
                if item.get('NZBName') == name:
                    return item, labels
            for item in api.listgroups():
                if item.get('NZBName') == name:
                    label = item.get('PostInfoText', '')
                    if label and (not labels or labels[-1] != label):
                        labels.append(label)
            time.sleep(0.02)
        raise RuntimeError('timeout waiting for %s; labels: %s' % (name, labels))


def donor_nzb(target, pieces, directory, missing='none', password=None):
    members = []
    for index, piece in enumerate(pieces, 1):
        filename = 'release.7z.%03d' % index
        served = harness._place_copy(target, directory, piece, filename)
        count = (len(piece) + DONOR_SEGMENT - 1) // DONOR_SEGMENT
        holes = (set(range(1, count + 1)) if missing == 'all' else
                 {4} if missing == 'middle' and index == 1 else set())
        members.append((served, filename, len(piece), DONOR_SEGMENT, holes))
    return harness.build_multi_nzb(members, password=password)


def requests(target):
    time.sleep(0.3)  # nserv flushes its captured frontend every 100ms.
    log = target.read_file('nserv.log').decode(errors='replace')
    result = re.findall(r'Received: (?:BODY|ARTICLE) ([^\r\n]+)', log)
    assert any('stall-primary/' in item for item in result), \
        'empty request capture cannot establish a download bound'
    return result


def recovered_bytes(history):
    return ((int(history.get('DupeRecoveredBytesHi', 0)) & 0xffffffff) << 32
            | (int(history.get('DupeRecoveredBytesLo', 0)) & 0xffffffff))


def run_scenario(daemon, target, name):
    data = harness._payload(8 * PRIMARY_SEGMENT, 9122026)
    password_test = name == 'password_retry'
    with tempfile.TemporaryDirectory(prefix='archive-stall-fixture-') as work:
        archive = (generators.seven_zip_lzma_encrypted(
            [('movie.mkv', data)], work, 'fixture-correct-password')
            if password_test else generators.seven_zip_lzma(
                [('movie.mkv', data)], work))
    pieces = generators.split_bytes(archive, [VOLUME_SIZE, VOLUME_SIZE])
    assert len(pieces) == 3 and len(pieces[0]) // DONOR_SEGMENT >= 64
    primary_path = harness._place_copy(target, 'stall-primary', data, 'movie.mkv')
    primary = harness.build_nzb(primary_path, 'movie.mkv', len(data),
                                PRIMARY_SEGMENT, {PRIMARY_HOLE})
    api = daemon.wait_ready()
    key = 'archive-stall-' + name
    donor_ids = []

    def append_donor(label, nzb, score):
        nzb_id = daemon.append(api, label, nzb, True, key, score)
        assert nzb_id > 0, (label, nzb_id)
        donor_ids.append(nzb_id)

    if password_test:
        append_donor('WrongPassword', donor_nzb(target, pieces, 'stall-password',
                     password='fixture-wrong-password'), 80)
        append_donor('CorrectPassword', donor_nzb(target, pieces, 'stall-password',
                     password='fixture-correct-password'), 70)
    else:
        bad = donor_nzb(target, pieces, 'stall-unavailable',
                       missing='middle' if name == 'missing_middle' else 'all')
        append_donor('UnavailableFirst', bad, 80)
        if name == 'repeated_then_healthy':
            # Two independently saved NZBs with exactly the same posting. With
            # DupeCheck=no both remain eligible paused donors; intake dedup and
            # automatic redownload cannot hide the repair-stage regression.
            append_donor('UnavailableAgain', bad, 70)
            append_donor('HealthyLater', donor_nzb(target, pieces,
                         'stall-healthy'), 60)

    queued = api.listgroups()
    assert all(any(item['NZBID'] == nzb_id for item in queued)
               for nzb_id in donor_ids), 'all donor entries must exist before repair'
    primary_name = 'Primary-' + name
    assert daemon.append(api, primary_name, primary, False, key, 100) > 0
    history, labels = daemon.wait_observed_history(api, primary_name)
    fetched = requests(target)
    bad_requests = [item for item in fetched if 'stall-unavailable/' in item]
    good_requests = [item for item in fetched if 'stall-healthy/' in item]
    first_bad_requests = [item for item in bad_requests
                          if 'release.7z.001?1=' in item]
    volume_counts = Counter(re.search(r'release\.7z\.(\d{3})', item).group(1)
                            for item in bad_requests)
    outputs = [rel for rel in target.find_files('main')
               if os.path.basename(rel) == 'movie.mkv']
    output = target.read_file(outputs[0]) if len(outputs) == 1 else None
    hole_start = (PRIMARY_HOLE - 1) * PRIMARY_SEGMENT
    damaged = (data[:hole_start] + bytes(PRIMARY_SEGMENT)
               + data[hole_start + PRIMARY_SEGMENT:])
    scratch = list(Path(target.path('main')).rglob('.stream-decompress.*'))
    log = target.read_file('nzbget.log').decode(errors='replace')
    downloading = [label for label in labels if label.startswith('Downloading duplicate ')]
    unavailable_decompression = [label for label in labels
                                if label.startswith('Decompressing duplicate Unavailable')]
    detail = {
        'status': history.get('Status'),
        'unavailable_body_requests': len(bad_requests),
        'unavailable_first_article_requests': len(first_bad_requests),
        'unavailable_volume_requests': dict(volume_counts),
        'healthy_body_requests': len(good_requests),
        'donor_total_articles': sum((len(piece) + DONOR_SEGMENT - 1)
                                    // DONOR_SEGMENT for piece in pieces),
        'recovered_articles': int(history.get('DupeRecoveredArticles', 0)),
        'recovered_bytes': recovered_bytes(history),
        'output_count': len(outputs),
        'output_byte_identical': output == data,
        'damaged_target_unchanged': output == damaged,
        'output_sha256': hashlib.sha256(output).hexdigest() if output else None,
        'scratch_directories_remaining': len(scratch),
        'observed_download_labels': len(downloading),
        'unavailable_decompression_labels': unavailable_decompression,
    }
    checks = {
        'single_output': len(outputs) == 1,
        'scratch_cleaned': not scratch,
        'download_label_observed': bool(downloading),
        'incomplete_donor_never_decompresses': not unavailable_decompression,
    }
    if name in ('all_missing', 'repeated_then_healthy'):
        checks.update({
            'missing_donor_aborted_early': 0 < len(bad_requests) <= 16,
            'same_posting_not_retried': len(first_bad_requests) <= 2,
            'later_missing_volumes_not_downloaded': set(volume_counts) == {'001'},
        })
    elif name == 'missing_middle':
        checks.update({
            'internal_hole_aborts_remaining_archive': 0 < len(bad_requests) <= 24,
            'internal_missing_article_was_requested': any(
                'release.7z.001?4=' in item for item in bad_requests),
        })
    if name in ('all_missing', 'missing_middle'):
        checks.update({
            'failure_preserved': history.get('Status') == 'FAILURE/HEALTH',
            'target_not_patched': output == damaged,
            'zero_recovered_articles': detail['recovered_articles'] == 0,
            'zero_recovered_bytes': detail['recovered_bytes'] == 0,
        })
    else:
        checks.update({
            'success_preserved': history.get('Status') == 'SUCCESS/HEALTH',
            'output_byte_identical': output == data,
            'one_hole_recovered': detail['recovered_articles'] == 1,
            'exact_recovered_bytes': detail['recovered_bytes'] == PRIMARY_SEGMENT,
        })
        if password_test:
            checks['different_password_still_tried'] = (
                'duplicate WrongPassword: extraction failed' in log
                and 'from duplicate CorrectPassword (decompressed)' in log)
        else:
            checks['healthy_later_posting_downloaded'] = bool(good_requests)

    detail['checks'] = checks
    target.write_file('history.json', json.dumps(history, indent=2).encode())
    target.write_file('progress.json', json.dumps(labels, indent=2).encode())
    target.write_file('requests.json', json.dumps(fetched, indent=2).encode())
    target.write_file('result.json', json.dumps(detail, indent=2).encode())
    return all(checks.values()), detail


def run_one(binary, name, keep):
    work = tempfile.mkdtemp(prefix='nzbget-archive-stall-%s-' % name)
    target = harness.LocalTarget(binary, work)
    nntp_port, rpc_port = harness.free_port(), harness.free_port()
    while rpc_port == nntp_port:
        rpc_port = harness.free_port()
    daemon = ObservedDaemon(target, nntp_port, rpc_port)
    passed = False
    try:
        daemon.write_config([
            'DupeArticleFallback=stream', 'DupeStreamDecompress=yes',
            'DupeCheck=no', 'ParCheck=auto', 'ArticleRetries=0',
            'SevenZipCmd=' + generators.SEVENZIP_PATH,
        ])
        daemon.start_nserv()
        daemon.start()
        passed, detail = run_scenario(daemon, target, name)
        print('[%s] %s %s' % ('PASS' if passed else 'FAIL', name,
                              json.dumps(detail, sort_keys=True)), flush=True)
    except Exception as exc:
        print('[FAIL] %s %s: %s' % (name, type(exc).__name__, exc), flush=True)
    finally:
        target.teardown(keep or not passed)
        if keep or not passed:
            print('Artifacts: %s' % work, flush=True)
    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--nzbget', required=True)
    parser.add_argument('--scenario', action='append', choices=SCENARIOS)
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    binary = str(Path(args.nzbget).resolve())
    if not os.access(binary, os.X_OK):
        parser.error('--nzbget must name an executable')
    if not generators.HAVE_7Z:
        parser.error('7z/7za/7zr/7zz is required for real archive fixtures and extraction')
    results = [run_one(binary, name, args.keep)
               for name in (args.scenario or SCENARIOS)]
    return 0 if all(results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
