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
| `leadswitch` | The top-scored duplicate shares the primary's hole (parts 2–12), a lower-scored duplicate covers it. After a few consecutive lead misses the lead rotates to the next duplicate (`Switching lead duplicate collection`, exactly once — the stale-snapshot guard must prevent cascade demotion), completing byte-identical from the second duplicate. |
| `manydonors` | 18 duplicates — more than the donor cache holds — to exercise the cache-eviction path. Regression test for the use-after-free crash; the daemon must survive and complete. |
| `stream` | Donor posted the SAME `.mkv` split into different article sizes (250 KB vs 500 KB). `DupeArticleFallback=stream` repairs the missing byte ranges in post-processing; every hole is filled and there is no par2 in the harness, so the byte-based health recount (see option `DupeArticleFallback`) takes the release all the way to `SUCCESS` (moved to its destination directory), asserted directly alongside byte identity. |
| `liveoverlap` | `DupeArticleFallback=live`: FileA completes with a hole while the big FileB still downloads (DownloadRate-throttled). The live pass repairs A DURING the download — proven by strict log order (`Starting live stream repair` before `completely downloaded`) — and the release still completes `SUCCESS` byte-identically via the unchanged post-processing accounting. |
| `livegate` | The same two-file fixture under plain `stream`: the live pass must NOT run (option gate); repair happens in post-processing as before. |
| `livelastfile` | Live mode, single-file collection: the live dispatch is skipped for the collection's last file (the post-processing stage starts moments later and repairs it there) — asserts the last-file guard. |
| `repost` | A 4-member "rar+par2 release" (opaque random bytes — stands in for a passworded, compressed archive) reposted byte-identically under different segmentation. M1 same-bytes matching pairs each damaged member to its donor twin by name/suffix and repairs rar volume and par2 alike, byte-identically (final status is FAILURE/PAR by design — the stand-in par2 is not a real par2). |
| `repostrenamed` | A 3-member rar-volume repost whose members were RENAMED (different release base name, same volume suffixes), reposted byte-identically. Exact-name pairing cannot fire, so M1's unique-suffix-key tier must pair the damaged member with its donor twin — proving tier-2 pairing end-to-end. No par2 and the hole is fully filled, so the release also completes `SUCCESS`. |
| `xpackbare` | A bare `.mkv` completed with a hole, repaired from a duplicate that posted the SAME movie packed into store-mode RAR3 volumes (different framing, offsets and segmentation). M1 cannot pair bare against rar volumes, so the M2 cross-packing `ContentMap` pass must locate the missing bytes inside the donor's volumes and patch them byte-identically. No par2 and the hole is fully filled, so the release also completes `SUCCESS`. |
| `xpackrar` | A store-rar target repaired from a bare donor, with a degraded volume (a header hole) that must be excluded from the map and stays damaged (no par2) — the PARTIAL-repair proof for the health recount: the still-damaged volume means the release stays `FAILURE/HEALTH`, asserted directly (never a false `SUCCESS`). |
| `xpackrar2rar` | rar-to-rar cross-packing where target and donor use *different* volume sizes (2 MB vs 1.5 MB); member-wise M1 cannot window these, but the inner content stream matches exactly. No par2 and the hole is fully filled, so the release also completes `SUCCESS`. |
| `xpackzip` | A bare target repaired from a SPANNED STORED ZIP donor (`z01`+`z02`+`zip`); the repair write itself crosses a donor volume boundary. No par2 and the hole is fully filled, so this is the cross-packing scenario that asserts `SUCCESS` directly. |
| `xpack7z` | A bare target repaired from a 7z-COPY donor posted as `.7z.001`/`.002` splits. |
| `xpacksplit` | A store-rar target repaired from RAW SPLITS (`movie.mkv.001`/`.002`/`.003`). |
| `xpackcompressed` | The mechanism ladder on a COMPRESSED archive (method byte forged to 0x33): M2's method gate must never map it, but a byte-identical repost still repairs it via M1 — the ladder proof for the docs' compressed/encrypted promise. |
| `xpackneg` | The negative: a donor set with the right inner size but the WRONG bytes must be rejected by the identity probes (`content identity not confirmed`); nothing is written and both files stay unrecovered. |
| `xcrypt_encplain` | M3 password-assisted cross-packing: a password-ENCRYPTED store-rar target (password known via its own NZB) with a data hole in one volume, repaired from a BARE unencrypted donor. Asserts byte-identical ciphertext volumes after the decrypt/patch/re-encrypt round trip. |
| `xcrypt_plainenc` | Reverse direction: a BARE unencrypted target repaired from a password-ENCRYPTED store-rar donor whose password travels via the donor's own NZB (the M3 retry ladder: plain `BuildMap` fails with "encrypted archive data", then retries with the donor's password). |
| `xcrypt_diffpass` | Both sides encrypted under DIFFERENT passwords and different volume sizes: proves the donor's and target's crypto contexts never mix (each side decrypts/re-encrypts with its own key). |
| `xcrypt_wrongpass` | The negative: an encrypted donor whose supplied password does NOT match the one it was encrypted with. RAR3 has no stored password-check value, so `BuildMap` succeeds with a wrong key; the mismatch is caught downstream by the content-identity probe (`content identity not confirmed`) - nothing is written. |
| `xdecomp_zip` | M4 decompression-assisted donor extraction (option `DupeStreamDecompress`): a bare `movie.mkv` target with holes, repaired from a REAL DEFLATE-compressed zip donor of the identical file. M2 never maps a compressed zip entry, so the donor's articles are materialized and extracted via the configured `SevenZipCmd` before the recovered plaintext patches the target's holes. No par2 and the hole is fully filled, so this is the decompression scenario that asserts `SUCCESS` directly. |
| `xdecomp_7z` | Same shape as `xdecomp_zip`, but the donor is a REAL LZMA2-compressed 7z archive. |
| `xdecomp_storetarget` | The M4 decompression path against a non-bare TARGET: a store-mode rar3 target (same generator `xpackrar` uses) with a data hole, repaired from a compressed-7z donor of the same inner file - proves the extracted-donor path composes with the M2 plain target map. |
| `xdecomp_enc7z` | The POSIX password-quoting proof: a bare target repaired from a HEADER-ENCRYPTED 7z donor (`-mhe=on`), its password threaded via the donor's own NZB exactly like `xcrypt_plainenc`. Locks in the Task 2 fix where a quote-wrapped password would otherwise break extraction on Linux/macOS. |
| `xdecomp_enctarget` | The M3+M4 composition: a password-ENCRYPTED store-rar TARGET with a data hole, repaired from a COMPRESSED 7z donor of the same movie. The donor is materialized and extracted to plaintext, then re-encrypted under the target's own AES-CBC stream context (the M3 write core) and the ciphertext written into the hole. Byte-identical encrypted volumes prove the extract → re-encrypt → patch round trip. Needs both a crypto module and a 7z binary. |
| `xdecomp_neg` | The negative: a compressed donor with the right inner size but the WRONG bytes; rejected by the identity probe (`content identity not confirmed`) before any write - nothing is written and the target stays unrecovered. |
| `xdecomp_off` | The opt-in gate: the identical compressed-7z-donor setup as `xdecomp_7z`, but `DupeStreamDecompress` is OMITTED (default `no`) - the decompression path must never run and the item stays unrepaired. |

Each scenario asserts byte identity of the reassembled file (with
`DirectWrite=yes`) and the `DupeRecoveredArticles` counter reflecting the
recovery. Article-level scenarios (`complementary`, `cutover`, `leadswitch`,
`manydonors`) assert `SUCCESS` directly. Stream-repair recounts byte-based
health after repair (see option `DupeArticleFallback`): a release whose
holes are ALL filled and ships no par2 now also completes `SUCCESS`, moved
to its destination directory - `stream`, one cross-packing (`xpackzip`)
and one decompression (`xdecomp_zip`) scenario assert that status flip
directly, and every other fully-repaired no-par2 scenario (`repostrenamed`,
`xpackbare`, `xpackrar2rar`, `xpack7z`, `xpacksplit`, `xpackcompressed`,
`xcrypt_encplain`, `xcrypt_plainenc`, `xcrypt_diffpass`, `xdecomp_7z`,
`xdecomp_storetarget`, `xdecomp_enc7z`) reaches it too. A PARTIALLY-repaired release still stays
`FAILURE/HEALTH` - `xpackrar`'s header-holed volume asserts the status does
NOT contain `SUCCESS`, proving the recount can never falsely complete a
partial repair. `repost` ends `FAILURE/PAR` (its stand-in par2 is opaque
random bytes and fails par-check by design). The negative scenarios
(`xpackneg`, `xcrypt_wrongpass`, `xdecomp_neg`, `xdecomp_off`) recover
nothing and stay `FAILURE/HEALTH`. The four `xcrypt_*` scenarios require the
Python `cryptography` package; the six `xdecomp_*` scenarios require a local
`7z`/`7za`/`7zr` binary on `PATH`. Without them, the respective scenarios
SKIP gracefully (reported separately from PASS/FAIL).

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
    [--scenario all|complementary|cutover|leadswitch|manydonors|stream|liveoverlap|livegate|livelastfile|repost|repostrenamed|xpackbare|xpackrar|xpackrar2rar|xpackzip|xpack7z|xpacksplit|xpackcompressed|xpackneg|xcrypt_encplain|xcrypt_plainenc|xcrypt_diffpass|xcrypt_wrongpass|xdecomp_zip|xdecomp_7z|xdecomp_storetarget|xdecomp_enc7z|xdecomp_enctarget|xdecomp_neg|xdecomp_off] [--serial <adb-serial>] [--keep]
```

`--keep` leaves the scratch workdir in place for inspection. Exit code is 0
only if every selected scenario passes.
