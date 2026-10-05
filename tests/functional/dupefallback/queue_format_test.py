#!/usr/bin/env python3
"""Queue format compatibility between this branch and upstream nzbget (B24, B28).

This branch writes the queue, history and file states in upstream's formats (64
and 7) and keeps its own data in a trailer upstream ignores, so that going back
to an nzbget without duplicate repair keeps the queue and the history.

  roundtrip  this build makes a history item with recovered articles, a queued
             duplicate and a paused, half-downloaded item; upstream loads all of
             it without an error; this build loads it again afterwards
  migrate    a queue directory an earlier build of the branch wrote (formats
             66/9, e.g. a backup of a production queue) is loaded by this build
             and rewritten; upstream then reads the same history
  newer      a queue file of a format above 64 beside the marker of this build is
             a newer upstream format: refused, never read as the branch's 65/66

Usage: queue_format_test.py --nzbget BUILD --upstream UPSTREAM_BUILD [--legacy-queue TAR]
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness  # noqa: E402

# options only this branch knows: upstream would reject them (and pause downloads)
PR_OPTIONS = re.compile(r'^(Dupe(ArticleFallback|StreamDecompress|StreamTimeout|Search\w*|Health\w*|'
                        r'BodyChecks|MinAlive|FastDonors))=', re.M)


def header(path):
    with open(path, 'rb') as f:
        return f.readline().decode(errors='replace').strip()


def version(path):
    m = re.search(r'(\d+)$', header(path))
    return int(m.group(1)) if m else -1


def upstream_config(conf_path):
    with open(conf_path) as f:
        text = f.read()
    text = PR_OPTIONS.sub(lambda m: '#' + m.group(0), text)
    text = text.replace('HealthCheck=dupe', 'HealthCheck=park')
    with open(conf_path, 'w') as f:
        f.write(text)


def start(target, binary, conf_rel):
    target.procs.append(subprocess.Popen([binary, '-c', target.path(conf_rel), '-s'],
                                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))


def stop(daemon, target):
    api = daemon.wait_ready()
    try:
        api.shutdown()
    except Exception:
        pass
    deadline = time.time() + 30
    while time.time() < deadline and target.procs[-1].poll() is None:
        time.sleep(0.2)
    return target.procs[-1].poll() is not None


def errors_in_log(target, since):
    log = target.read_file('nzbget.log').decode(errors='replace')[since:]
    return [line for line in log.splitlines() if re.search(
        r'Error reading diskstate|version mismatch|could not load diskstate|Failed to read', line, re.I)]


def log_size(target):
    try:
        return len(target.read_file('nzbget.log'))
    except Exception:
        return 0


def roundtrip(build, upstream):
    stage = tempfile.mkdtemp(prefix='queueformat-roundtrip-')
    target = harness.LocalTarget(build, stage)
    nntp, rpc = harness.free_port(), harness.free_port()
    daemon = harness.Daemon(target, nntp, rpc)
    detail = {}
    try:
        daemon.write_config(['DupeArticleFallback=stream', 'ParCheck=auto', 'HealthCheck=dupe',
                             'Server1.Connections=1', 'ContinuePartial=yes'])
        daemon.start_nserv(extra_args=['-w', '200'], port=nntp)
        time.sleep(1)
        start(target, build, daemon.conf_rel)
        api = daemon.wait_ready()

        # a release repaired from a duplicate (recovered articles in its history entry)
        size, seg_primary, seg_donor = 3_000_000, 500_000, 250_000
        data = harness._payload(size, 4242)
        pp = harness._place_copy(target, 'rtA', data, 'file.mkv')
        dp = harness._place_copy(target, 'rtB', data, 'file.mkv')
        daemon.append(api, 'Donor', harness.build_nzb(dp, 'obf.mkv', size, seg_donor, set()), True, 'rt-key', 50)
        daemon.append(api, 'Done', harness.build_nzb(pp, 'Done.mkv', size, seg_primary, {3}), False, 'rt-key', 100)
        done = daemon.wait_history(api, 'Done')
        detail['done_recovered'] = int(done.get('DupeRecoveredArticles', 0))

        # a half-downloaded item, paused (its file state has a decoded size)
        big = 8_000_000
        bp = harness._place_copy(target, 'rtC', harness._payload(big, 77), 'big.bin')
        daemon.append(api, 'Partial', harness.build_nzb(bp, 'Big.bin', big, 200_000, set()), False, 'rt-big', 10)
        deadline = time.time() + 60
        while time.time() < deadline:
            g = [x for x in api.listgroups() if x['NZBName'] == 'Partial']
            if g and g[0]['RemainingSizeMB'] < (big >> 20) - 2:
                api.editqueue('GroupPause', 0, '', [g[0]['NZBID']])
                break
            time.sleep(0.2)
        time.sleep(2)
        detail['stopped_ours'] = stop(daemon, target)

        queue_dir = target.path('main', 'queue')
        detail['queue_version'] = version(os.path.join(queue_dir, 'queue'))
        detail['history_version'] = version(os.path.join(queue_dir, 'history'))
        states = [f for f in os.listdir(queue_dir) if re.match(r'^\d+[sc]$', f)]
        detail['state_versions'] = sorted({version(os.path.join(queue_dir, f)) for f in states})
        detail['marker'] = os.path.exists(os.path.join(queue_dir, 'dupestate'))
        with open(os.path.join(queue_dir, 'history')) as f:
            detail['history_trailer'] = '#dupestate 1' in f.read()

        # this build again first: its own data comes back from the trailer
        start(target, build, daemon.conf_rel)
        api = daemon.wait_ready()
        time.sleep(1)
        again = [h for h in api.history(True) if h['NZBName'] == 'Done']
        detail['restart_recovered'] = int(again[0].get('DupeRecoveredArticles', 0)) if again else -1
        stop(daemon, target)

        # upstream on the same queue
        upstream_config(target.path(daemon.conf_rel))
        mark = log_size(target)
        start(target, upstream, daemon.conf_rel)
        api = daemon.wait_ready()
        time.sleep(2)
        names_up = sorted(g['NZBName'] for g in api.listgroups())
        hist_up = sorted((h['NZBName'], h['Status']) for h in api.history(True))
        detail['upstream_queue'] = names_up
        detail['upstream_history'] = hist_up
        detail['upstream_errors'] = errors_in_log(target, mark)
        detail['stopped_upstream'] = stop(daemon, target)

        # this build again
        daemon.write_config(['DupeArticleFallback=stream', 'ParCheck=auto', 'HealthCheck=dupe',
                             'Server1.Connections=1', 'ContinuePartial=yes'])
        mark = log_size(target)
        start(target, build, daemon.conf_rel)
        api = daemon.wait_ready()
        time.sleep(2)
        detail['back_queue'] = sorted(g['NZBName'] for g in api.listgroups())
        detail['back_history'] = sorted((h['NZBName'], h['Status']) for h in api.history(True))
        detail['back_errors'] = errors_in_log(target, mark)
        stop(daemon, target)

        ok = (detail['done_recovered'] >= 1 and detail['restart_recovered'] == detail['done_recovered'] and
              detail['queue_version'] == 64 and
              detail['history_version'] == 64 and detail['state_versions'] == [7] and
              detail['marker'] and detail['history_trailer'] and
              names_up == ['Partial'] and ('Done', 'SUCCESS/HEALTH') in hist_up and
              ('Donor', 'DELETED/DUPE') in hist_up and
              not detail['upstream_errors'] and detail['back_queue'] == names_up and
              detail['back_history'] == hist_up and not detail['back_errors'])
        return ok, detail
    finally:
        target.teardown(False)


def migrate(build, upstream, legacy_tar):
    stage = tempfile.mkdtemp(prefix='queueformat-migrate-')
    target = harness.LocalTarget(build, stage)
    rpc = harness.free_port()
    daemon = harness.Daemon(target, harness.free_port(), rpc)
    detail = {}
    try:
        queue_dir = target.path('main', 'queue')
        shutil.rmtree(queue_dir)
        with tarfile.open(legacy_tar) as tar:
            tar.extractall(target.path('main'), filter='data')
        # no news server: nothing downloads, the queue is only loaded and saved
        daemon.write_config(['DupeArticleFallback=stream', 'HealthCheck=dupe', 'Server1.Active=no'])
        detail['legacy_versions'] = (version(os.path.join(queue_dir, 'queue')) if os.path.exists(os.path.join(queue_dir, 'queue')) else None,
                                     version(os.path.join(queue_dir, 'history')))
        with open(os.path.join(queue_dir, 'history'), errors='replace') as f:
            f.readline()
            legacy_count = int(f.readline())
        start(target, build, daemon.conf_rel)
        api = daemon.wait_ready(timeout=120)
        time.sleep(3)
        ours = len(api.history(True))
        first = api.history()[0]
        api.editqueue('HistorySetDupeScore', 0, str(first['DupeScore']), [first['ID']])	# a save, nothing changed
        time.sleep(2)
        detail['ours_history'] = ours
        detail['ours_errors'] = errors_in_log(target, 0)
        stop(daemon, target)
        detail['history_version_after'] = version(os.path.join(queue_dir, 'history'))
        detail['marker'] = os.path.exists(os.path.join(queue_dir, 'dupestate'))

        upstream_config(target.path(daemon.conf_rel))
        mark = log_size(target)
        start(target, upstream, daemon.conf_rel)
        api = daemon.wait_ready(timeout=120)
        time.sleep(3)
        detail['upstream_history'] = len(api.history(True))
        detail['upstream_errors'] = errors_in_log(target, mark)
        stop(daemon, target)
        detail['legacy_count'] = legacy_count
        ok = (ours == legacy_count and detail['history_version_after'] == 64 and detail['marker'] and
              detail['upstream_history'] == legacy_count and not detail['ours_errors'] and
              not detail['upstream_errors'])
        return ok, detail
    finally:
        target.teardown(False)


def newer(build):
    stage = tempfile.mkdtemp(prefix='queueformat-newer-')
    target = harness.LocalTarget(build, stage)
    daemon = harness.Daemon(target, harness.free_port(), harness.free_port())
    detail = {}
    try:
        daemon.write_config(['Server1.Active=no'])
        queue_dir = target.path('main', 'queue')
        with open(os.path.join(queue_dir, 'dupestate'), 'w') as f:
            f.write('1\n')
        with open(os.path.join(queue_dir, 'queue'), 'w') as f:
            f.write('nzbget diskstate file version 65\n0\n')
        start(target, build, daemon.conf_rel)
        daemon.wait_ready()
        time.sleep(1)
        detail['refused'] = bool(re.search(r'version mismatch', target.read_file('nzbget.log').decode(errors='replace')))
        stop(daemon, target)
        return detail['refused'], detail
    finally:
        target.teardown(False)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--nzbget', required=True)
    ap.add_argument('--upstream', required=True)
    ap.add_argument('--legacy-queue', help='a tar of a queue directory an earlier build of the branch wrote')
    args = ap.parse_args()
    results = [('roundtrip',) + roundtrip(args.nzbget, args.upstream), ('newer',) + newer(args.nzbget)]
    if args.legacy_queue:
        results.append(('migrate',) + migrate(args.nzbget, args.upstream, args.legacy_queue))
    for name, ok, detail in results:
        print('[%s] %s  %s' % ('PASS' if ok else 'FAIL', name, detail))
    print('%d/%d queue format checks passed' % (sum(r[1] for r in results), len(results)))
    return 0 if all(r[1] for r in results) else 1


if __name__ == '__main__':
    sys.exit(main())
