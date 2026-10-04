# `DupeArticleFallback` functional harness

Cross-platform, fully offline functional tests for the `DupeArticleFallback`
feature (see option `DupeArticleFallback` and PR #850). No real Usenet access
is needed: the harness drives nzbget's own test NNTP server (`nzbget --nserv`)
plus a scratch nzbget daemon, and uses the nserv `!serverlist` message-id
suffix to make chosen articles "missing" on the active server.

## Scenarios

| Scenario | What it proves |
|---|---|
| `complementary` | Two postings of the same content, each missing *different* articles; neither completes alone, together they do. Output is byte-identical. |
| `cutover` | Primary missing 10/20 articles → the file "cuts over" and leads with the duplicate (`Leading with duplicate collections`), completing byte-identical. |
| `leadswitch` | The top-scored duplicate shares the primary's hole (parts 2–12), a lower-scored duplicate covers it. After a few consecutive lead misses the lead rotates to the next duplicate (`Switching lead duplicate collection`, exactly once—the stale-snapshot guard must prevent cascade demotion), completing byte-identical from the second duplicate. |
| `cutovertruth` | Counter honesty under cutover: the lead duplicate misses articles the primary HAS, so after cutover a second duplicate serves them proactively. Those proactive successes must NOT count as recovered—`DupeRecoveredArticles` stays bounded by the primary's own missing articles, with at most one lead switch. |
| `manydonors` | 18 duplicates—more than the donor cache holds—to exercise the cache-eviction path. Regression test for the use-after-free crash; the daemon must survive and complete. |
| `stream` | Donor posted the SAME `.mkv` split into different article sizes (250 KB vs 500 KB). `DupeArticleFallback=stream` repairs the missing byte ranges in post-processing; every hole is filled and there is no par2 in the harness, so the byte-based health recount (see option `DupeArticleFallback`) takes the release all the way to `SUCCESS` (moved to its destination directory), asserted directly alongside byte identity. |
| `liveoverlap` | `DupeArticleFallback=live`: file `FileA` completes with a hole while the big FileB still downloads (DownloadRate-throttled). The live pass repairs A DURING the download—proven by strict log order (`Starting live stream repair` before `completely downloaded`)—and the release still completes `SUCCESS` byte-identically via the unchanged post-processing accounting. |
| `livegate` | The same two-file fixture under plain `stream`: the live pass must NOT run (option gate); repair happens in post-processing as before. |
| `livelastfile` | Live mode, single-file collection: the live dispatch is skipped for the collection's last file (the post-processing stage starts moments later and repairs it there)—asserts the last-file guard. |
| `repost` | A 4-member "rar+par2 release" (opaque random bytes standing in for a passworded, compressed archive) reposted byte-identically under different segmentation. The damaged archive volume is repaired byte-identically after PAR fails. The PAR2 file retains its exact original hole, and captured NNTP commands prove zero donor PAR2 fetches alongside successful archive donor fetches. Final status is FAILURE/PAR by design because the stand-in parity is not a real PAR2 set. |
| `repostrenamed` | A 3-member rar-volume repost whose members were RENAMED (different release base name, same volume suffixes), reposted byte-identically. Exact-name pairing cannot fire, so M1's unique-suffix-key tier must pair the damaged member with its donor twin—proving tier-2 pairing end-to-end. No par2 and the hole is fully filled, so the release also completes `SUCCESS`. |
| `repostobfuscated` | An 8-member repost of equal-size volumes with obfuscated names on both sides, a shuffled donor order, and different article sizes. No name, suffix, position, or size-step tier identifies the twins, so stream repair probes the donor's members until one is byte-identical. Every damaged member must be repaired; the release completes `SUCCESS`. |
| `repostdonorgaps` | A byte-identical repost that misses articles of its own exactly where the identity probes land first. Missing probes are inconclusive, so verification draws replacement probes until it can compare the twin; the hole is repaired, nothing is rejected, and the release completes `SUCCESS`. |
| `xpackbare` | A bare `.mkv` completed with a hole, repaired from a duplicate that posted the SAME movie packed into store-mode RAR3 volumes (different framing, offsets and segmentation). M1 cannot pair bare against rar volumes, so the M2 cross-packing `ContentMap` pass must locate the missing bytes inside the donor's volumes and patch them byte-identically. No par2 and the hole is fully filled, so the release also completes `SUCCESS`. |
| `xpackrar` | A store-rar target repaired from a bare donor, with a degraded volume (a header hole) that must be excluded from the map and stays damaged (no par2)—the PARTIAL-repair proof for the health recount: the still-damaged volume means the release stays `FAILURE/HEALTH`, asserted directly (never a false `SUCCESS`). |
| `xpackrar2rar` | rar-to-rar cross-packing where target and donor use *different* volume sizes (2 MB vs 1.5 MB); member-wise M1 cannot window these, but the inner content stream matches exactly. No par2 and the hole is fully filled, so the release also completes `SUCCESS`. |
| `xpack2sets` | TWO damaged store-rar sets in one item, repaired from ONE duplicate carrying both sets at different volume sizes. Locks the per-duplicate donor-map reuse (the map is built once per duplicate and shared across repair sets) with the per-pair identity gate still routing each repair set to its own donor set; both sets repaired byte-identically. |
| `xpackzip` | A bare target repaired from a SPANNED STORED ZIP donor (`z01`+`z02`+`zip`); the repair write itself crosses a donor volume boundary. No par2 and the hole is fully filled, so this is the cross-packing scenario that asserts `SUCCESS` directly. |
| `xpack7z` | A bare target repaired from a 7z-COPY donor posted as `.7z.001`/`.002` splits. |
| `xpacksplit` | A store-rar target repaired from RAW SPLITS (`movie.mkv.001`/`.002`/`.003`). |
| `xpackcompressed` | The mechanism ladder on a COMPRESSED archive (method byte forged to 0x33); the `M2` method gate must never map it, but a byte-identical repost still repairs it via M1—the ladder proof for the docs' compressed/encrypted promise. |
| `xpackneg` | The negative: a donor set with the right inner size but the WRONG bytes must be rejected by the identity probes (`content identity not confirmed`); nothing is written and both files stay unrecovered. |
| `xcrypt_encplain` | M3 password-assisted cross-packing: a password-ENCRYPTED store-rar target (password known via its own NZB) with a data hole in one volume, repaired from a BARE unencrypted donor. Asserts byte-identical ciphertext volumes after the decrypt/patch/re-encrypt round trip. |
| `xcrypt_plainenc` | Reverse direction: a BARE unencrypted target repaired from a password-ENCRYPTED store-rar donor whose password travels via the donor's own NZB (the M3 retry ladder: plain `BuildMap` fails with `encrypted archive data`, then retries with the donor's password). |
| `xcrypt_diffpass` | Both sides encrypted under DIFFERENT passwords and different volume sizes: proves the donor's and target's crypto contexts never mix (each side decrypts/re-encrypts with its own key). |
| `xcrypt_wrongpass` | The negative: an encrypted donor whose supplied password does NOT match the one it was encrypted with. RAR3 has no stored password-check value, so `BuildMap` succeeds with a wrong key; the mismatch is caught downstream by the content-identity probe (`content identity not confirmed`) - nothing is written. |
| `xdecomp_zip` | M4 decompression-assisted donor extraction (option `DupeStreamDecompress`): a bare `movie.mkv` target with holes, repaired from a REAL DEFLATE-compressed zip donor of the identical file. M2 never maps a compressed zip entry, so the donor's articles are materialized and extracted via the configured `SevenZipCmd` before the recovered plaintext patches the target's holes. No par2 and the hole is fully filled, so this is the decompression scenario that asserts `SUCCESS` directly. |
| `xdecomp_7z` | Same shape as `xdecomp_zip`, but the donor is a REAL LZMA2-compressed 7z archive. |
| `xdecomp_storetarget` | The M4 decompression path against a non-bare TARGET: a store-mode rar3 target (same generator `xpackrar` uses) with a data hole, repaired from a compressed-7z donor of the same inner file - proves the extracted-donor path composes with the M2 plain target map. |
| `xdecomp_enc7z` | The POSIX password-quoting proof: a bare target repaired from a HEADER-ENCRYPTED 7z donor (`-mhe=on`), its password threaded via the donor's own NZB exactly like `xcrypt_plainenc`. Locks in the Task 2 fix where a quote-wrapped password would otherwise break extraction on Linux/macOS. |
| `xdecomp_enctarget` | The M3+M4 composition: a password-ENCRYPTED store-rar TARGET with a data hole, repaired from a COMPRESSED 7z donor of the same movie. The donor is materialized and extracted to plaintext, then re-encrypted under the target's own AES-CBC stream context (the M3 write core) and the ciphertext written into the hole. Byte-identical encrypted volumes prove the extract → re-encrypt → patch round trip. Needs both a crypto module and a 7z binary. |
| `xdecomp_neg` | The negative: a compressed donor with the right inner size but the WRONG bytes; rejected by the identity probe (`content identity not confirmed`) before any write - nothing is written and the target stays unrecovered. |
| `xdecomp_symlink` | The symlink fail-close: a compressed donor containing a valid movie plus a relative symlink to it must be rejected before selecting or patching anything (`archive contains link`). The link is relative and in-tree because 7-Zip 23.01+ refuses to create absolute or `..` link targets at extraction time (exit code 2), which would fail the extract step before the daemon's own link check ever ran. Also asserts cleanup unlinks the extracted symlink without following it (an outside sentinel file survives) and removes every scratch directory. POSIX-only. |
| `wholefile` | A volume none of whose articles exists anywhere. The renamed byte-identical repost is proven on the set's other damaged volume first, then the missing volume is recreated whole from the twin member the suffix pairs it with (`Recreating`), and the release completes `SUCCESS` byte-identically. |
| `dupefailover` | `HealthCheck=dupe`: a dead posting whose files no duplicate carries, with a healthy lower-scored backup of the same title in history. The download is abandoned well before all of its articles have failed (`Failing over`), and the backup is fetched and completes `SUCCESS`. |
| `dupehopeless` | `HealthCheck=dupe` without a usable backup: a dead posting whose one duplicate is dead too (and marked bad) is parked (`Parking`) once the duplicates were asked for a sample, a tenth of it was tried and fewer than one in ten of those articles existed, instead of failing every article; no `Failing over` line. |
| `dupedeadstart` | The guard for that rule: a posting that merely begins with a dead stretch (first 40 of 100 articles of each file missing, no par2, no usable duplicate) is below critical health with nothing downloaded yet, but is NOT parked - it runs to the end and ends `FAILURE/HEALTH` with exactly its missing articles failed. |
| `streamretry` | Retry after stream repair: one volume fully repaired, another keeps a hole no duplicate carries. "Retry failed articles" must leave the release `FAILURE/HEALTH` (before the fix the repaired volume's credit was subtracted twice and the damaged release turned `SUCCESS`). |
| `wholefileretry` / `wholefilefailretry` | "Retry failed articles" after a whole volume was recreated, on a release that succeeded and on one that still fails: the recreated volume is kept, never deleted as an empty failed file. |
| `wholefileonly` | The common shape of a missing volume: everything else is intact, so the duplicate is proven byte-identical on an intact volume (`verified on intact file`) and the missing one is recreated; `SUCCESS` byte-identically. |
| `wholefilewrongdonor` | A duplicate with the same volume names and sizes but other bytes (another packing) fails that proof; nothing is recreated or written. |
| `wholefilelive` | Whole-file recreation under `live` mode, when the live pass already repaired the collection's other damage. |
| `wholefilepar` | A real par2 index (generated, no recovery slices): the recreated volume passes a full par-check (`SUCCESS/PAR`). |
| `wholefilenofirst` | The donor's twin misses its first article: the file is sized from a later article and recreated except that first part. |
| `wholefilepartial` | The donor's twin has a hole: partial recreation credits nothing to health, and a retry keeps the recreated bytes. |
| `wholefiletwo` | Two volumes missing entirely, each recreated from its own twin member. |
| `wholefilenfoproof` / `wholefilesampleproof` | A different packing (same volume names and sizes, other bytes) that ships the same `.nfo` or sample: a byte match on those - even the sample's legitimate repair - must not prove the duplicate for recreating an archive volume, which only a sibling volume of the same set can. |
| `dupehopelessnodupecheck` | `HealthCheck=dupe` with `DupeCheck=no`: no failover is possible, but a dead posting is still parked after the sample. |
| `dupehopelessretry` | "Retry failed articles" on a download parked as hopeless is checked like a new download and parked again as early as the first attempt (28 tried articles, not 60). |
| `dupefailovernofallback` | `HealthCheck=dupe` with `DupeArticleFallback=no`: a posting missing only its first stretch is not abandoned for a lower-scored backup. |
| `failoverlive` | `HealthCheck=dupe` with `live` mode: the failover parks the download (detaching any live pass) and the backup completes. |
| `wholefilerestart` | nzbget restarts between download and post-processing while a whole-file job waits: the job is saved and loaded intact and the volume is still recreated. |
| `reloadpostqueue` | nzbget reloads (as saving settings does) three times while a download waits in post-processing behind 40 queued downloads: the reloaded post job must run. Before, the post-processor could sanitise the queue before it was loaded again, and the job stayed at `LOADING_PARS` for good (a race: about half the runs). |
| `prodwholefile` / `prodstream` | Whole-file recreation and stream repair under the production option set (`live`, direct rename and unpack, par-rename, quick par-check, unpack, article cache, `HealthCheck=dupe`) on a store-mode 7z split with a par2 index: par-check passes and unpack extracts a byte-identical movie. |
| `prodrarwhole` / `prodrarstream` | The same on a store-mode rar set with valid CRCs, so `unrar` and direct unpack really run (`generators.rar3_store_volumes_valid`). |
| `dupefailoverchain` | The first backup is dead too: the primary fails over to it, it fails over to the second, healthy backup, which completes; the parked primary is never brought back. |
| `xpacklatency` | Cross-packing against a news server answering after 1 s: requests that end without a server answer are retried on a fresh connection instead of counting the article as missing; the movie is repaired byte-identically (before the fix nothing was recovered). |
| `xpackflaky` | Cross-packing while the provider drops every connection and turns new ones away for 4 s (`FlakyNntpProxy`, a per-user connection limit): the repeated attempts for a duplicate article are spaced instead of used up within milliseconds, and the dropped pooled connections don't use up attempts of their own. Before, the article counted as missing, the content map couldn't be built and nothing was recovered. |
| `xpackdeadserver` | The preferred news server goes down for good at the first duplicate request: it delays the repair once (its spaced attempts), then gets one attempt per article, so cross-packing stays within 15 s instead of waiting for it on every article. |
| `wholefileunicode` / `wholefilepadding` / `wholefileoldstyle` | Whole-file recreation with spaces and non-ASCII names, with volume numbers padded differently on each side (`part003` vs `part03`), and with old-style volume names (`x.rar`, `x.r00`, ...). |
| `wholefilecontinued` | An old-style set of more than 101 volumes continues `x.r99` with `x.s00`, `x.s01`, ...: the missing `x.s00` belongs to the `x.rNN` set, is proven on its siblings and recreated. |
| `wholefiletwosets` | Two archive sets in one release, the duplicate carrying both under other names: the missing volume of the second set is recreated from the duplicate's second set, never from the same-numbered volume of its first set. |
| `*_nodirect` | `wholefile`, `wholefileonly`, `stream`, `xpackbare` and `streamretry` with `DirectWrite=no` (files assembled from temporary article files). |
| `xpackendhole_nodirect` | `DirectWrite=no` with holes up to the end of a bare movie, repaired from rar volumes: the file joined from article files ends with its last downloaded article, so cross-packing must size the target by its decoded size. Before, the holes past the end of the file were "outside the mappable inner stream". |
| `dupefailovernonzb` | The best backup's source nzb-file is gone from `NzbDir`: the failover picks the next backup that can be downloaded again instead of parking the download for one that can't. |
| `wholefileproofcost` | Six volumes missing and a duplicate that is another packing: the failed intact-volume proof is not repeated for every missing volume of the set (16 duplicate article fetches instead of 96). |
| `xdecomp_off` | The opt-in gate: the identical compressed-7z-donor setup as `xdecomp_7z`, but `DupeStreamDecompress` is OMITTED (default `no`) - the decompression path must never run and the item stays unrepaired. |

Each scenario asserts byte identity of the reassembled file (with
`DirectWrite=yes`) and the `DupeRecoveredArticles` counter reflecting the
recovery. Article-level scenarios (`complementary`, `cutover`, `leadswitch`,
`manydonors`) assert `SUCCESS` directly. Stream-repair recounts byte-based
health after repair (see option `DupeArticleFallback`): a release whose
holes are ALL filled and ships no par2 now also completes `SUCCESS`, moved
to its destination directory - `stream`, one cross-packing (`xpackzip`)
and one decompression (`xdecomp_zip`) scenario assert that status flip
directly, and every other fully repaired no-par2 scenario (`repostrenamed`, `repostobfuscated`,
`xpackbare`, `xpackrar2rar`, `xpack7z`, `xpacksplit`, `xpackcompressed`,
`xcrypt_encplain`, `xcrypt_plainenc`, `xcrypt_diffpass`, `xdecomp_7z`,
`xdecomp_storetarget`, `xdecomp_enc7z`) reaches it too. A PARTIALLY-repaired release still stays
`FAILURE/HEALTH` - `xpackrar`'s header-holed volume asserts the status does
NOT contain `SUCCESS`, proving the recount can never falsely complete a
partial repair. `repost` ends `FAILURE/PAR` (its stand-in par2 is opaque
random bytes and fails par-check by design). The negative scenarios
(`xpackneg`, `xcrypt_wrongpass`, `xdecomp_neg`, `xdecomp_off`) recover
nothing and stay `FAILURE/HEALTH`. The four `xcrypt_*` scenarios require the
Python `cryptography` package; the eight `xdecomp_*` scenarios require a local
`7z`/`7za`/`7zr`/`7zz` binary on `PATH`. Without them, the respective scenarios
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
    [--scenario all|complementary|cutover|leadswitch|cutovertruth|manydonors|stream|liveoverlap|livegate|livelastfile|repost|repostrenamed|repostobfuscated|repostdonorgaps|xpackbare|xpackrar|xpackrar2rar|xpack2sets|xpackzip|xpack7z|xpacksplit|xpackcompressed|xpackneg|xcrypt_encplain|xcrypt_plainenc|xcrypt_diffpass|xcrypt_wrongpass|xdecomp_zip|xdecomp_7z|xdecomp_storetarget|xdecomp_enc7z|xdecomp_enctarget|xdecomp_neg|xdecomp_symlink|xdecomp_off|wholefile|dupefailover|dupehopeless|dupedeadstart|streamretry|wholefileretry|wholefilefailretry|wholefileonly|wholefilewrongdonor|wholefilelive|wholefilepar|wholefilenofirst|wholefilepartial|wholefiletwo|dupehopelessnodupecheck|dupefailovernofallback|failoverlive|wholefilenfoproof|wholefilesampleproof|wholefilerestart|prodwholefile|prodstream|prodrarwhole|prodrarstream|dupefailoverchain|xpacklatency|xpackflaky|xpackdeadserver|wholefileunicode|wholefilepadding|wholefileoldstyle|wholefiletwosets|wholefile_nodirect|wholefileonly_nodirect|stream_nodirect|xpackbare_nodirect|streamretry_nodirect|dupefailovernonzb|wholefileproofcost] [--serial <adb-serial>] [--keep]
```

`--keep` leaves the scratch workdir in place for inspection. Exit code is 0
only if every selected scenario passes.

## Unavailable archive donors

```sh
python3 archive_stall_test.py --nzbget /path/to/nzbget --keep
```

This separate regression suite requires a local `7z`/`7za`/`7zr`/`7zz`. It
creates real compressed archives split over three volumes and hundreds of
NNTP articles, and captures actual `BODY` requests from an isolated nserv.
The four scenarios cover an entirely unavailable donor, a donor with an
internal missing article, repeated entries for the same unavailable posting
followed by a healthy donor, and the same encrypted posting retried with a
corrected password. Use `--scenario <name>` to select a case:
`all_missing`, `missing_middle`, `repeated_then_healthy`, or `password_retry`.

The failed-donor cases bound requests well below a complete first volume,
preserve every target byte and zero recovery counters, and require cleanup
of temporary archive directories. Successful recovery must reproduce the
original payload byte for byte. Public RPC progress labels must say
`Downloading duplicate` while retrieving articles; incomplete archives must
never reach `Decompressing duplicate`. Results, request captures, progress
labels and history are saved with `--keep` or whenever a case fails.
