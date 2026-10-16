## RSS and duplicate check

NZBGet can monitor RSS feeds, usually provided by an indexer, and add matching nzb-files to the download queue automatically. Each feed is checked at a configurable interval, and every item the feed returns is passed through the feed filter, which decides whether the item is accepted and which options it gets.

Feeds are configured in the web interface on the `Feeds` page, or in the configuration file with the options `Feed1.Name`, `Feed1.URL`, `Feed1.Filter` and so on, where the number is the feed number.

### Feed options

A feed is only complete with `Name` and `URL`. If one of them is missing, NZBGet reports `Feed definition not complete` for that feed on startup.

| Option | Default | Description |
|--------|---------|-------------|
| `Name` | | Name of the feed. Used in the web interface and in the log. |
| `URL` | | Address of the RSS feed. |
| `Filter` | | Filter rules applied to the items of this feed. See [Feed filter](#feed-filter). |
| `Interval` | `15` | How often the feed is checked, in minutes. `0` disables the automatic check of this feed. |
| `Backlog` | `yes` | Treat all items on the first fetch as backlog. With `yes`, the items already present the first time the feed is fetched (or after the URL or filter was changed) are ignored; only items found on later fetches are processed. With `no`, all items are processed immediately. |
| `PauseNzb` | `no` | Add nzb-files of this feed as paused. |
| `Category` | | Category for added nzb-files. |
| `CategorySource` | `NzbFile` | Where to take the category from: `auto` (NZB file first, then the feed file), `NzbFile` (only the NZB file) or `FeedFile` (only the feed file). If the category is not found, it is left empty. This option works only when the filter did not set a category and `Category` is empty. |
| `Priority` | `0` | Priority for added nzb-files. Any integer is allowed; the web interface offers -100 (very low), -50 (low), 0 (normal), 50 (high), 100 (very high) and 900 (force). Downloads with priority 900 or higher are processed even while the program is paused. |
| `CertVerification` | `strict` | Certificate verification level: `strict` (signing and hostname check), `minimal` (signing check only), `none` (no checks). |
| `Extensions` | | List of feed extension scripts to execute for the content of this feed, separated by commas or semicolons. The scripts must be located in `ScriptDir` and paths are relative to it. |

### Feed filter

The simplest filter is a space separated list of words that must all be present in the item title:

```
linux debian dvd
```

A filter consists of rules — one rule per line. Each rule defines a search string and a command that is executed when the search string matches. There are five commands:

| Command | Description |
|---------|-------------|
| `A:` | Accept-rule. The item is good and no further rules are checked. Rules are accept-rules by default, so `A:` may be omitted. |
| `R:` | Reject-rule. The item is bad and no further rules are checked. |
| `Q:` | Require-rule. The item is bad if it does **not** match, and no further rules are checked. |
| `O(options):` | Options-rule. The options in the rule are set for the item. The item is neither accepted nor rejected by this rule, but it can be accepted later by an accept-rule, keeping the options unless the accept-rule overrides them. |
| `#` | Comment. Lines starting with `#` are ignored. |

Because options in the configuration file cannot span multiple lines, rules of a single feed must be separated with the `%` (percent) character when they are written on one line.

A rule has the form:

```
[A:|A(options):|R:|Q:|O(options):|#] search-string
```

The search string is a list of terms. A term has the form `[+|-][field:][command]param`:

- `+` — positive term (default, may be omitted).
- `-` — negative term: if the term matches, the feed item is ignored.
- `field` — the field to search in; `title` by default.
- `command` — how to compare: `@` (search for the string, default), `$` (regular expression, POSIX Extended syntax), `=`, `<`, `<=`, `>`, `>=`.
- `param` — the value to compare with.

`@` and `$` are used for text fields (`title`, `filename`, `category`, `link`, `description`, `dupekey`), `=`, `<`, `<=`, `>` and `>=` for numeric fields (`size`, `age`, `imdbid`, `rageid`, `season`, `episode`, `priority`, `dupescore`). Only `title`, `filename` and `age` are always present; other fields depend on the RSS provider. Any newznab attribute (`newznab:attr`) can be used as a search field with the prefix `attr-`, for example `attr-genre`.

Text search supports the wildcards `*` (any number of any characters), `?` (any single character) and `#` (a single digit). The search is performed word by word by default; if the pattern starts and ends with `*`, or contains word separator characters, the whole field is searched. For `size` the suffixes `K`/`KB`, `M`/`MB` and `G`/`GB` are accepted; for `age` the suffixes `m` (minutes), `h` (hours) and `d` (days, the default).

Examples:

```
A: s01* -category:anime;
my show WEB-DL;
*my?show* WEB-DL size:<1.8GB age:>2h;
R: size:>9GB;
Q: HDTV
```

The trailing `;` and `.` characters in the examples are not part of the filter.

### Options set by filter rules

An options-rule contains a comma separated list of `name:value` pairs. Long and short names are accepted:

| Option | Description |
|--------|-------------|
| `category` (`cat`, `c`) | Set the category of the item. |
| `pause` (`p`) | Set the pause flag: `yes`/`y` or `no`/`n`. |
| `priority` (`pr`, `r`) | Set the priority. |
| `priority+` (`pr+`, `r+`) | Increase the priority. |
| `dupescore` (`ds`, `s`) | Set the dupe score. |
| `dupescore+` (`ds+`, `s+`) | Increase the dupe score. |
| `dupekey` (`dk`, `k`) | Set the dupe key. |
| `dupekey+` (`dk+`, `k+`) | Add to the dupe key. |
| `dupemode` (`dm`, `m`) | Set the dupe mode: `score`/`s`, `all`/`a` or `force`/`f`. |
| `rageid` | Generate the dupe key from this rageid (integer) and the season/episode numbers of the title. |
| `series` | Generate the dupe key from this series identifier and the season/episode numbers of the title. |

Rule options override the values set in the feed options:

```
Accept(category:my series, pause:yes, priority:100): my show 1080p;
Options(c:my series, p:y, r:100): 1080p;
Options(s:1000): 1080p;
Options(k+:1080p): 1080p;
Options(dupemode:force): BluRay
```

### Duplicate check

The duplicate check prevents the same content from being downloaded twice, which matters most for feeds: a feed may return the same item again, or an item may already be in the queue or in the history.

With `DupeCheck=yes` (default) NZBGet checks a new nzb-file against:

1. the history — if it contains the same title with success status, the nzb-file is not added to the queue;
2. the download queue — if it contains the same title, the nzb-file is added as a backup, in case the first file fails;
3. the nzb-file itself — duplicate entries are reported, which helps to find errors in bad nzb-files.

"Same title" means that the nzb file name is the same or the dupe key is the same. Dupe keys are set when fetching from RSS feeds, using the title identifier fields provided by the RSS provider (imdbid or rageid/season/episode), and can be set by filter rules or by API calls.

The behaviour is controlled by two values of every item:

- **dupe key** — identifies the content. Items with the same dupe key are duplicates.
- **dupe score** — how good a duplicate is; the higher the score, the better.

and by the dupe mode:

| Mode | Behaviour |
|------|-----------|
| `score` (default) | The dupe score decides. Duplicates found in the queue or in the history that are not better are not added; a lower-scored duplicate that is still needed as a backup is moved to the history as a dupe-backup, so it can be tried if the download fails. |
| `all` | Duplicates are always added to the queue, regardless of their dupe score. Only items with exactly the same content, or items marked as *good* in the history, are still skipped. |
| `force` | The duplicate check is not performed for this item. |

If duplicates were detected, only one of them is downloaded. If the download fails, another duplicate is tried. If the download succeeds, all remaining duplicates are deleted from the queue. For this automatic handling, `HealthCheck` must be set to `Delete`, `Park` or `None`; with `Pause` you need to unpause another duplicate manually.

Related options:

- `FeedHistory` — for how many days the information about the items of a feed is kept. After a feed is fetched, its items are remembered, so that only new items are detected on the next fetch. Because a feed is limited in length (usually 100 items or less), old items are pushed out of it and their records are dropped, immediately if the option is `0` or otherwise after the given number of days. Keeping the records for a few days also helps when the provider has technical issues and returns empty feeds or misses items — otherwise such items would be downloaded again.
- `KeepHistory` — how many days the history is kept. With `0` the history is disabled and the duplicate check does not work.

Dupe keys and scores can also be changed later, in the web interface or with the API actions described in [Edit queue](../api/EDITQUEUE.md) and [History](../api/HISTORY.md). New downloads can be added with these values via [Append](../api/APPEND.md).

### Fetching feeds

The `Feeds` page in the web interface lists all configured feeds and provides two actions for each of them:

- **Fetch** — fetch the feed now and add the accepted items to the queue. Fetching all feeds at once is available as well.
- **Preview** — fetch the feed and show the items with the reason why each of them would be accepted or rejected, before anything is added.

Feed fetching can also be triggered by the scheduler with the command `FetchFeed`, whose parameter is a comma separated list of feed ids or feed names (`0` fetches all feeds).

The following RPC methods are used by the web interface: [fetchfeed](../api/FETCHFEED.md) (feed id, `0` for all feeds), [viewfeed](../api/VIEWFEED.md) and [previewfeed](../api/PREVIEWFEED.md). Whether a feed is currently being fetched is reported by `FeedActive` in [Status](../api/STATUS.md); the configuration of the feeds is checked by the `Feeds` section of the system health check, see [System health](../api/SYSTEMHEALTH.md).

### Related documentation

- [Feed extensions](../extensions/FEED.md)
- [Append](../api/APPEND.md), [Edit queue](../api/EDITQUEUE.md), [History](../api/HISTORY.md)
- [System health](../api/SYSTEMHEALTH.md), [Status](../api/STATUS.md)
- https://nzbget.com/documentation/rss/
