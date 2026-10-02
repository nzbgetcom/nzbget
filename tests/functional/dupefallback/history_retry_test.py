#!/usr/bin/env python3
"""Retry a naturally incomplete NZB without changing its source or queue state.

The unavailable segment is absent from the original NZB, so all listed articles
succeed. Retry Failed must finish post-processing again without downloading or
getting stuck on a zero-byte queue entry. Uses only scratch daemon/nserv paths.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile
import time

import harness


def file_hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_hashes(target):
    return {rel: file_hash(Path(target.path(rel)))
            for rel in target.find_files('main', 'nzb')}


def request_count(target):
    time.sleep(0.3)  # nserv flushes its captured frontend periodically.
    log = target.read_file('nserv.log').decode(errors='replace')
    return len(re.findall(r'Received: (?:BODY|ARTICLE) ', log))


def run(binary, keep):
    workdir = tempfile.mkdtemp(prefix='nzbget-history-retry-')
    target = harness.LocalTarget(binary, workdir)
    nntp_port = harness.free_port()
    rpc_port = harness.free_port()
    while rpc_port == nntp_port:
        rpc_port = harness.free_port()
    daemon = harness.Daemon(target, nntp_port, rpc_port)
    passed = False
    try:
        daemon.write_config(['DupeArticleFallback=no', 'DupeCheck=no',
                             'ParCheck=auto', 'HealthCheck=none',
                             'ContinuePartial=yes', 'ArticleRetries=0'])
        daemon.start_nserv(capture_requests=True)
        daemon.start()
        api = daemon.wait_ready()
        content = harness._payload(4 * 65536, 9102026)
        served = harness._place_copy(target, 'retry-primary', content,
                                     'partial.bin')
        nzb = harness.build_nzb(served, 'partial.bin', len(content),
                                65536, set())
        nzb, removed = re.subn(
            r'\s*<segment\b[^>]*\bnumber="2"[^>]*>[^<]*</segment>', '', nzb)
        assert removed == 1, 'fixture must omit exactly one internal NZB entry'
        name = 'MissingNzbEntry'
        nzb_id = daemon.append(api, name, nzb, False, '', 0)
        assert nzb_id > 0
        first = daemon.wait_history(api, name, timeout=30)
        assert first['Status'] == 'FAILURE/HEALTH', first['Status']
        assert first['SuccessArticles'] == 3, first['SuccessArticles']
        assert first['FailedArticles'] == 1, first['FailedArticles']
        outputs = [Path(target.path(rel)) for rel in target.find_files('main')
                   if os.path.basename(rel) == 'partial.bin']
        assert len(outputs) == 1, outputs
        output = outputs[0]
        original_hash = file_hash(output)
        original_sources = source_hashes(target)
        assert original_sources, 'original queued NZB source must exist'
        requests_before = request_count(target)
        assert requests_before >= 3, 'nserv capture must contain the initial fetches'

        # No source-NZB, payload, saved diskstate, or configuration changes here.
        assert api.editqueue('HistoryRetryFailed', 0, '', [nzb_id])
        second = None
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            second = next((item for item in api.history()
                           if item['NZBID'] == nzb_id), None)
            if second:
                break
            time.sleep(0.1)
        queue = [item for item in api.listgroups() if item['NZBID'] == nzb_id]
        requests_after = request_count(target)
        detail = {
            'initial_status': first['Status'],
            'retry_status': second['Status'] if second else None,
            'remaining_queue_entries': len(queue),
            'requests_before': requests_before,
            'requests_after': requests_after,
            'output_unchanged': output.exists() and file_hash(output) == original_hash,
            'source_nzb_unchanged': source_hashes(target) == original_sources,
            'article_counts_unchanged': bool(second and
                second['SuccessArticles'] == 3 and second['FailedArticles'] == 1),
        }
        passed = (detail['retry_status'] == 'FAILURE/HEALTH' and not queue
                  and requests_after == requests_before
                  and detail['output_unchanged'] and detail['source_nzb_unchanged']
                  and detail['article_counts_unchanged'])
        target.write_file('result.json', json.dumps(detail, indent=2).encode())
        target.write_file('history.json', json.dumps(api.history(), indent=2).encode())
        target.write_file('queue.json', json.dumps(queue, indent=2).encode())
        print('[%s] history_retry %s' % ('PASS' if passed else 'FAIL',
                                        json.dumps(detail, sort_keys=True)), flush=True)
    except Exception as exc:
        print('[FAIL] history_retry %s: %s' % (type(exc).__name__, exc), flush=True)
    finally:
        target.teardown(keep or not passed)
        if keep or not passed:
            print('Artifacts: %s' % workdir, flush=True)
    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--nzbget', required=True)
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    binary = str(Path(args.nzbget).resolve())
    if not os.access(binary, os.X_OK):
        parser.error('--nzbget must name an executable')
    return 0 if run(binary, args.keep) else 1


if __name__ == '__main__':
    raise SystemExit(main())
