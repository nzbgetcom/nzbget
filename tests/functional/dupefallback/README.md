# DupeArticleFallback functional harness

Cross-platform, fully-offline functional tests for the `DupeArticleFallback`
feature (see option `DupeArticleFallback` and PR #850). No real Usenet access
is needed: the harness drives nzbget's own test NNTP server (`nzbget --nserv`)
plus a scratch nzbget daemon, and uses the nserv `!serverlist` message-id
suffix to make chosen articles "missing" on the active server.

## Scenarios

| Scenario | What it proves |
|---|---|
| `complementary` | Two postings of the same content, each missing *different* articles; neither completes alone, together they do. Output is byte-identical. |
| `cutover` | Primary missing 10/20 articles → the file "cuts over" and leads with the duplicate (`Leading with duplicate collections`), completing byte-identical. |
| `manydonors` | 18 duplicates — more than the donor cache holds — to exercise the cache-eviction path. Regression test for the use-after-free crash; the daemon must survive and complete. |
| `stream` | Donor posted the SAME `.mkv` split into different article sizes (250 KB vs 500 KB). `DupeArticleFallback=stream` repairs the missing byte ranges in post-processing; output is byte-identical even though the history status stays non-SUCCESS (no par2 in the harness). |
| `repost` | A 4-member "rar+par2 release" (opaque random bytes — stands in for a passworded, compressed archive) reposted byte-identically under different segmentation. M1 same-bytes matching pairs each damaged member to its donor twin by name/suffix and repairs rar volume and par2 alike, byte-identically (final status is FAILURE/PAR by design — the stand-in par2 is not a real par2). |

Each scenario asserts the download reaches `SUCCESS`, the reassembled file is
**byte-identical** to the source (with `DirectWrite=yes`), and the
`DupeRecoveredArticles` counter reflects the recovery. The `stream` and
`repost` scenarios assert byte identity, the repair log lines and the counter
instead of the SUCCESS status.

## Running

The scenario logic is identical on every platform; only *where* the processes
and files live differs, isolated behind a `Target` abstraction in
[`harness.py`](harness.py) (`LocalTarget` for Linux/macOS, `AdbTarget` for
Android over adb).

### Linux / macOS (native)

```sh
./run-local.sh /path/to/nzbget           # all scenarios
./run-local.sh /path/to/nzbget cutover   # one scenario
```

### Linux (in Docker, x86_64)

```sh
./run-linux-docker.sh /path/to/linux-x86_64/nzbget
```

### Android (emulator or device, over adb)

Build the Android binary and start an emulator first:

```sh
bash linux/build-nzbget.sh android aarch64-ndk release
emulator -avd <name> -no-window -no-audio &   # or plug in a device
adb wait-for-device
./run-android.sh /path/to/android/nzbget
```

The binary is pushed to `/data/local/tmp`, nserv + the daemon run on the
device, and the RPC control port is forwarded back to the host.

## Direct invocation

```sh
python3 harness.py --nzbget <bin> --target {local|adb} \
    [--scenario all|complementary|cutover|manydonors|stream|repost] [--serial <adb-serial>] [--keep]
```

`--keep` leaves the scratch workdir in place for inspection. Exit code is 0
only if every selected scenario passes.
