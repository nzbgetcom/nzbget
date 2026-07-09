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
| `repostrenamed` | A 3-member rar-volume repost whose members were RENAMED (different release base name, same volume suffixes), reposted byte-identically. Exact-name pairing cannot fire, so M1's unique-suffix-key tier must pair the damaged member with its donor twin — proving tier-2 pairing end-to-end (no par2 in the harness). |
| `xpackbare` | A bare `.mkv` completed with a hole, repaired from a duplicate that posted the SAME movie packed into store-mode RAR3 volumes (different framing, offsets and segmentation). M1 cannot pair bare against rar volumes, so the M2 cross-packing `ContentMap` pass must locate the missing bytes inside the donor's volumes and patch them byte-identically (no par2 in the harness). |
| `xpackrar` | A store-rar target repaired from a bare donor, with a degraded volume (a header hole) that must be excluded from the map and stays damaged (no par2). |
| `xpackrar2rar` | rar-to-rar cross-packing where target and donor use *different* volume sizes (2 MB vs 1.5 MB); member-wise M1 cannot window these, but the inner content stream matches exactly. |
| `xpackzip` | A bare target repaired from a SPANNED STORED ZIP donor (`z01`+`z02`+`zip`); the repair write itself crosses a donor volume boundary. |
| `xpack7z` | A bare target repaired from a 7z-COPY donor posted as `.7z.001`/`.002` splits. |
| `xpacksplit` | A store-rar target repaired from RAW SPLITS (`movie.mkv.001`/`.002`/`.003`). |
| `xpackcompressed` | The mechanism ladder on a COMPRESSED archive (method byte forged to 0x33): M2's method gate must never map it, but a byte-identical repost still repairs it via M1 — the ladder proof for the docs' compressed/encrypted promise. |
| `xpackneg` | The negative: a donor set with the right inner size but the WRONG bytes must be rejected by the identity probes (`content identity not confirmed`); nothing is written and both files stay unrecovered. |

Each scenario asserts the download reaches `SUCCESS`, the reassembled file is
**byte-identical** to the source (with `DirectWrite=yes`), and the
`DupeRecoveredArticles` counter reflects the recovery. The `stream`,
`repost`, `repostrenamed` and `xpack*` scenarios assert byte identity, the
repair log lines and the counter instead of the SUCCESS status.

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
    [--scenario all|complementary|cutover|manydonors|stream|repost|repostrenamed|xpackbare|xpackrar|xpackrar2rar|xpackzip|xpack7z|xpacksplit|xpackcompressed|xpackneg] [--serial <adb-serial>] [--keep]
```

`--keep` leaves the scratch workdir in place for inspection. Exit code is 0
only if every selected scenario passes.
