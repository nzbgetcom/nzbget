## API-method `listgroups`

### Signature
``` c++
struct[] listgroups(int NumberOfLogEntries);
```

### Description
Request for list of downloads (nzb-files). This method returns summary information for each group (nzb-file).

### Arguments
- **NumberOfLogEntries** `(int)` - ~~`v15.0`~~ Number of post-processing log-entries (field `Log`), which should be returned for the top (currently processing) item in post-processing state. Deprecated, must be 0.

### Return value
This method returns array of structures with following fields:

- **NZBID** `(int)` - ID of NZB-file.
- **FirstID** `(int)` - ~~`v13.0`~~ Deprecated, use `NZBID` instead.
- **LastID** `(int)` - ~~`v13.0`~~ Deprecated, use `NZBID` instead.
- **NZBFilename** `(string)` - Name of nzb-file, this file was added to queue from. The filename could include fullpath (if client sent it by adding the file to queue).
- **NZBName** `(string)` - The name of nzb-file without path and extension. Ready for user-friendly output.
- **NZBNicename** `(string)` - ~~`v15.0`~~ Deprecated, use `NZBName` instead.
- **Kind** `(string)` - Kind of queue entry: NZB or URL.
- **URL** `(string)` - URL where the NZB-file was fetched `(Kind=NZB)` or should be fetched `(Kind=URL)`.
- **DestDir** `(string)` - Destination directory for output file.
- **FinalDir** `(string)` - Final destination if set by one of post-processing scripts. Can be set only for items in post-processing state.
- **Category** `(string)` - Category for group or empty string if none category is assigned.
- **FileSizeLo** `(int)` - Initial size of all files in group in bytes, Low 32-bits of 64-bit value.
- **FileSizeHi** `(int)` - Initial size of all files in group in bytes, High 32-bits of 64-bit value.
- **FileSizeMB** `(int)` - Initial size of all files in group in MiB.
- **RemainingSizeLo** `(int)` - Remaining size of all (remaining) files in group in bytes, Low 32-bits of 64-bit value.
- **RemainingSizeHi** `(int)` - Remaining size of all (remaining) files in group in bytes, High 32-bits of 64-bit value.
- **RemainingSizeMB** `(int)` - Remaining size of all (remaining) files in group in MiB.
- **PausedSizeLo** `(int)` - Size of all paused files in group in bytes, Low 32-bits of 64-bit value.
- **PausedSizeHi** `(int)` - Size of all paused files in group in bytes, High 32-bits of 64-bit value.
- **PausedSizeMB** `(int)` - Size of all paused files in group in MiB.
- **FileCount** `(int)` - Initial number of files in group.
- **RemainingFileCount** `(int)` - Remaining (current) number of files in group.
- **RemainingParCount** `(int)` - Remaining (current) number of par-files in group.
- **MinPostTime** `(int)` - Date/time when the oldest file in the group was posted to newsgroup (Time is in C/Unix format).
- **MaxPostTime** `(int)` - Date/time when the newest file in the group was posted to newsgroup (Time is in C/Unix format).
- **MinPriority** `(int)` - ~~`v13.0`~~ Deprecated, use `MaxPriority` instead.
- **MaxPriority** `(int)` - Priority of the group. “Max” in the field name has historical reasons.
- **ActiveDownloads** `(int)` - Number of active downloads in the group. With this filed can be determined what group(s) is (are) being currently downloaded. In most cases only one group is downloaded at a time however more that one group can be downloaded simultaneously when the first group is almost completely downloaded.
- **Status** `(string)`- Status of the group:
  - **QUEUED** - queued for download;
  - **PAUSED** - paused;
  - **DOWNLOADING** - item is being downloaded;
  - **FETCHING** - nzb-file is being fetched from URL `(Kind=URL)`;
  - **PP_QUEUED** - queued for post-processing (completely downloaded);
  - **LOADING_PARS** - stage of par-check;
  - **VERIFYING_SOURCES** - stage of par-check;
  - **REPAIRING** - stage of par-check;
  - **VERIFYING_REPAIRED** - stage of par-check;
  - **RENAMING** - processed by par-renamer;
  - **UNPACKING** - being unpacked;
  - **MOVING** - moving files from intermediate directory into destination directory;
  - **EXECUTING_SCRIPT** - executing post-processing script;
  - **PP_FINISHED** - post-processing is finished, the item is about to be moved to history.
- **TotalArticles** `(int)` - Total number of articles in all files of the group.
- **SuccessArticles** `(int)` - Number of successfully downloaded articles.
- **FailedArticles** `(int)` - Number of failed article downloads.
- **Health** `(int)` - Current health of the group, in permille. 1000 means 100.0%. The health can go down below this valued during download if more article fails. It can never increase (unless merging of groups). Higher values are better.
- **CriticalHealth** `(int)` - Calculated critical health of the group, in permille. 1000 means 100.0%. The critical health is calculated based on the number and size of par-files. Lower values are better.
- **DownloadedSizeLo** `(int)` - `v14.0` Amount of downloaded data for group in bytes, Low 32-bits of 64-bit value.
- **DownloadedSizeHi** `(int)` - `v14.0` Amount of downloaded data for group in bytes, High 32-bits of 64-bit value.
- **DownloadedSizeMB** `(int)` - `v14.0` Amount of downloaded data for group in MiB.
- **DownloadTimeSec** `(int)` - `v14.0` Download time in seconds.
- **MessageCount** `(int)` - `v15.0` Number of messages stored in the item log. Messages can be retrieved with method [loadlog](LOADLOG.md).
- **DupeKey** `(string)` - Duplicate key. [RSS](../usage/RSS.md).
- **DupeScore** `(int)` - Duplicate score. [RSS](../usage/RSS.md).
- **DupeMode** `(string)` - Duplicate mode. One of SCORE, ALL, FORCE. [RSS](../usage/RSS.md).
- **Parameters** `(struct[])` - Post-processing parameters for group. An array of structures with following fields:
  - **Name** `(string)`- Name of post-processing parameter.
  - **Value** `(string)` - Value of post-processing parameter.
- **Deleted** `(bool)` - ~~`v12.0`~~ Deprecated, use DeleteStatus instead.
- **ServerStats** `(struct[])` - Per news-server download statistics. For description see method [history](HISTORY.md).
- **ParStatus**, **UnpackStatus**, **ExParStatus**, **MoveStatus**, **ScriptStatus**, **DeleteStatus**, **UrlStatus**, **MarkStatus**, **ScriptStatuses**, **PostTotalTimeSec**, **ParTimeSec**, **RepairTimeSec**, **UnpackTimeSec** - These fields have meaning only for a group which is being currently post-processed. For description see method [history](HISTORY.md).
- **PostInfoText** `(string)` - Text with short description of current action in post processor. For example: `Verifying file myfile.rar`. Only for a group which is being currently post-processed.
- **PostStageProgress** `(int)` - Completing of current stage, in permille. 1000 means 100.0%. Only for a group which is being currently post-processed.
- **PostTotalTimeSec** `(int)` - Number of seconds this post-job is being processed (after it first changed the state from PP-QUEUED). Only for a group which is being currently post-processed.
- **PostStageTimeSec** `(int)` - Number of seconds the current stage is being processed. Only for a group which is being currently post-processed.
- **Log** `(struct[])` - ~~`v15.0`~~ Array of structs with log-messages. For description of struct see method [log](LOG.md). Only for a group which is being currently post-processed. The number of returned entries is limited by parameter `NumberOfLogEntries`. Deprecated, use method [loadlog](LOADLOG.md) instead.
