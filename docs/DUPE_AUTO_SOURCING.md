# Auto-sourcing donors for DupeArticleFallback

The `DupeArticleFallback` feature (see option `DupeArticleFallback`) repairs a
damaged download by borrowing articles — or, with value `stream`/`live`, byte
ranges — from a **duplicate collection** of the same release that is already in
your download queue or history. This document describes how to make those
duplicates appear *automatically* when a download is damaged, so the repair has
something to draw from without you queuing an alternate NZB by hand.

The feature itself does not fetch alternate NZBs from an indexer. It only
*uses* duplicates that are already present. Auto-sourcing is therefore a thin
layer you add around it, using nzbget's existing extension and RPC facilities.

## What counts as a usable donor

A queue or history item is a donor for a target when it has the **same
duplicate key** (`DupeKey`) or the same name, and is not marked `dmForce`.
Crucially, the donor's source `.nzb` must still exist in `NzbDir` — so
`NzbCleanupDisk` must be **disabled** for the machinery to re-parse it. See the
`DupeArticleFallback` option help for the full donor rules.

Two facts make auto-sourcing practical:

1. **RSS duplicate handling already queues donors.** With `DupeCheck=yes`, when
   your RSS feeds pull in several postings of the same release (same
   `imdbid`/`rageid` dupe key), nzbget downloads one and marks the rest
   `DELETED/DUPE` without downloading them — but their `.nzb` files are retained
   in `NzbDir`. Those retained duplicates are exactly the donors the repair
   machinery re-parses. So for RSS-driven grabbing, you often already have
   donors with no extra work — just keep `NzbCleanupDisk=no`.

2. **A script can queue a donor on demand.** When a download finishes damaged,
   a post-processing extension can query your indexer for another NZB of the
   same release and `append` it under the **same `DupeKey`**, then send the
   damaged item back through the download/repair path. The appended NZB becomes
   a donor and the repair borrows from it.

## Prerequisites

```
DupeArticleFallback=stream      # or "live" for download-concurrent repair
DupeCheck=yes
NzbCleanupDisk=no               # donors must keep their .nzb in NzbDir
HealthCheck=none                # do NOT delete/park a damaged item before
                                # post-processing, or the repair never runs
```

## What a post-processing script receives

nzbget runs post-processing extension scripts with these environment variables
(among others — see `docs/extensions`):

| Variable | Meaning |
|---|---|
| `NZBPP_TOTALSTATUS` | `SUCCESS` / `WARNING` / `FAILURE` — the overall outcome |
| `NZBPP_HEALTH` | health in permille (1000 = 100.0%) |
| `NZBPP_DUPEKEY` | the item's duplicate key (empty if none) |
| `NZBPP_NZBNAME` | the release name |

A script keys off `NZBPP_TOTALSTATUS=FAILURE` (or a low `NZBPP_HEALTH`) to
decide the download needs a donor.

## Recipe

A post-processing script that, on failure, fetches an alternate NZB of the same
release and re-queues both so the repair can run:

```sh
#!/bin/sh
# Auto-source a donor when a download fails, then retry the original so
# DupeArticleFallback can repair it from the freshly-queued alternate.
[ "$NZBPP_TOTALSTATUS" = "FAILURE" ] || exit 93   # 93 = SUCCESS/skip

NAME="$NZBPP_NZBNAME"
KEY="$NZBPP_DUPEKEY"

# 1. Ask YOUR indexer for another NZB of the same release. This part is
#    indexer-specific: use its API to search by NAME (or by the imdbid/tvdbid
#    behind KEY) and download the .nzb to $ALT.
ALT="$(mktemp).nzb"
your_indexer_search "$NAME" > "$ALT" || exit 94   # 94 = FAILURE (no alternate)

# 2. Queue the alternate as a PAUSED donor under the SAME dupe key, so the
#    repair machinery treats it as a duplicate of the failed release. The
#    append RPC's DupeKey/DupeMode arguments carry the key (see docs/api/APPEND.md).
B64="$(base64 -w0 "$ALT")"
curl -s -u "$NZBOP_CONTROLUSERNAME:$NZBOP_CONTROLPASSWORD" \
  "http://127.0.0.1:$NZBOP_CONTROLPORT/jsonrpc" -d '{
    "method":"append",
    "params":["'"$NAME"'-donor.nzb","'"$B64"'","",0,true,false,"'"$KEY"'",0,"score",[]]
  }' >/dev/null

# 3. Send the failed item back to the download queue; the repair runs against
#    the donor queued in step 2 (editqueue HistoryRedownload, see docs/api/EDITQUEUE.md).
curl -s -u "$NZBOP_CONTROLUSERNAME:$NZBOP_CONTROLPASSWORD" \
  "http://127.0.0.1:$NZBOP_CONTROLPORT/jsonrpc" -d '{
    "method":"editqueue",
    "params":["HistoryRedownload","",['"$NZBPP_NZBID"']]
  }' >/dev/null

exit 93
```

The two `curl` calls use the JSON-RPC methods
[`append`](api/APPEND.md) (queue the donor, paused, with `DupeKey`) and
[`editqueue`](api/EDITQUEUE.md) (`HistoryRedownload` to retry the original).
The control credentials/port are passed to scripts as `NZBOP_CONTROLUSERNAME`,
`NZBOP_CONTROLPASSWORD` and `NZBOP_CONTROLPORT`.

## Notes and caveats

- **Only the indexer-query step is yours to fill in.** Everything else —
  matching the donor to the target, verifying byte identity before writing,
  the repair itself — is done by `DupeArticleFallback`.
- **The donor must be genuinely the same content.** A different encode
  (different resolution/codec/group) will not donate: the repair verifies byte
  identity against already-downloaded bytes and rejects a mismatch. Prefer an
  alternate posting of the *identical* release.
- **Loop protection.** Guard your script against re-fetching forever (e.g. skip
  if a donor was already queued for this `DupeKey`, or cap retries) — otherwise
  a release no indexer can complete will retry indefinitely.
- **`live` mode** repairs each damaged file as soon as it completes, so with an
  already-present donor there is no wait for the whole download to finish; the
  auto-sourcing script above still applies for the case where NO donor was
  present at download time.
