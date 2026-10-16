## API-method `viewfeed`

### Signature

``` c++
struct[] viewfeed(int ID, bool IncludeNonMatching);
```

### Description

Returns the items of the last fetch of the RSS feed with the given ID. Nothing is added to the download queue.

### Arguments

- **ID** `(int)` - ID of the feed. The ID is the number in the option name, so `Feed1.URL` belongs to the feed with ID `1`.
- **IncludeNonMatching** `(bool)` - Return the items which were not accepted by the feed filter as well.

### Return value

An array of structures with the following fields:

- **Title** `(String)` - Title of the item.
- **Filename** `(String)` - Name of the nzb-file as provided by the feed.
- **URL** `(String)` - Download URL of the nzb-file.
- **SizeHi** `(int)` - High 32 bits of the size of the nzb-file in bytes.
- **SizeLo** `(int)` - Low 32 bits of the size of the nzb-file in bytes.
- **SizeMB** `(int)` - Size of the nzb-file in megabytes.
- **Category** `(String)` - Category of the item as provided by the feed.
- **AddCategory** `(String)` - Category which would be set for the added nzb-file.
- **PauseNzb** `(bool)` - `true` if the nzb-file would be added as paused.
- **Priority** `(int)` - Priority which would be set for the added nzb-file.
- **Time** `(int)` - Publish time of the item (Unix timestamp).
- **Match** `(String)` - Result of the feed filter: **IGNORED**, **ACCEPTED** or **REJECTED**.
- **Rule** `(int)` - Number of the filter rule which matched the item.
- **DupeKey** `(String)` - Duplicate key which would be set for the added nzb-file.
- **DupeScore** `(int)` - Duplicate score which would be set for the added nzb-file.
- **DupeMode** `(String)` - Duplicate check mode which would be set for the added nzb-file: **SCORE**, **ALL** or **FORCE**.
- **Status** `(String)` - Status of the item within the feed: **UNKNOWN**, **BACKLOG**, **FETCHED** or **NEW**.

### Errors

- `2` - `Invalid parameter`.
- `3` - `Could not read feed`.

### Example

Request:

``` json
{"method": "viewfeed", "params": [1, false], "id": 1}
```

Response:

``` json
{"result": [
  {
    "Title" : "My Show S01E01 1080p WEB-DL",
    "Filename" : "my.show.s01e01.1080p.web-dl.nzb",
    "URL" : "https://indexer.example.com/api?t=get&id=1a2b3c",
    "SizeHi" : 0,
    "SizeLo" : 1234567,
    "SizeMB" : 1234,
    "Category" : "my series",
    "AddCategory" : "my series",
    "PauseNzb" : false,
    "Priority" : 100,
    "Time" : 1760000000,
    "Match" : "ACCEPTED",
    "Rule" : 1,
    "DupeKey" : "my show",
    "DupeScore" : 1000,
    "DupeMode" : "SCORE",
    "Status" : "NEW"
  }
], "id": 1}
```
