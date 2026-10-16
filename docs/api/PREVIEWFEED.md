## API-method `previewfeed`

### Signature

``` c++
struct[] previewfeed(int ID, string Name, string URL, string Filter, bool Backlog, bool PauseNzb, string Category,
		int Priority, int Interval, string FeedScript, bool IncludeNonMatching, int CacheTimeSec, string CacheId,
		string CategorySource);
```

Older version of the method (before `v16.0`):

``` c++
struct[] previewfeed(string Name, string URL, string Filter, bool PauseNzb, string Category, int Priority,
		bool IncludeNonMatching, int CacheTimeSec, string CacheId);
```

### Description

Fetches the given RSS feed and returns its items without adding anything to the download queue. This is the method behind the preview dialog of the feeds page: it shows which items the filter would accept and which options they would get.

The version of the method is detected by the first parameter: if it is an integer, the current signature is used, otherwise the older one without `ID`, `Backlog`, `Interval` and `FeedScript`.

### Arguments

- **ID** `(int)` - ID of the feed being previewed. Use `0` for a feed which does not exist in the configuration yet.
- **Name** `(string)` - Name of the feed (option `FeedN.Name`).
- **URL** `(string)` - URL of the feed (option `FeedN.URL`).
- **Filter** `(string)` - Filter rules (option `FeedN.Filter`).
- **Backlog** `(bool)` - Treat all items of the first fetch as backlog (option `FeedN.Backlog`).
- **PauseNzb** `(bool)` - Add nzb-files as paused (option `FeedN.PauseNzb`).
- **Category** `(string)` - Category for added nzb-files (option `FeedN.Category`).
- **Priority** `(int)` - Priority of added downloads (option `FeedN.Priority`).
- **Interval** `(int)` - How often the feed is checked, in minutes (option `FeedN.Interval`).
- **FeedScript** `(string)` - Lists of feed extension scripts to execute for the feed (option `FeedN.Extensions`).
- **IncludeNonMatching** `(bool)` - Return the items which were not accepted by the filter as well.
- **CacheTimeSec** `(int)` - Time in seconds for which the fetched items are cached. `0` disables the cache.
- **CacheId** `(string)` - Identifier of the cached items; the cache is used only if this parameter is not empty and `CacheTimeSec` is greater than `0`.
- **CategorySource** `(string)` - `Optional`. Where to take the category from: `Auto`, `FeedFile` or `NZBFile` (option `FeedN.CategorySource`). The default is `NZBFile`.

### Return value

An array of structures with the same fields as returned by [`viewfeed`](VIEWFEED.md).

### Errors

- `2` - `Invalid parameter`.
- `3` - `Could not read feed`.

### Example

Request:

``` json
{"method": "previewfeed", "params": [1, "my feed", "https://indexer.example.com/api?t=search&q=my+show",
  "my show WEB-DL", true, false, "", 0, 15, "", false, 0, "", "NZBFile"], "id": 1}
```
