## API-method `history`

### Signature
``` c++
struct[] history(bool Hidden);
```

### Description
Request for list of items in history-list.

### Arguments
- **Hidden** `(bool)` - Also return hidden history records. Use this only if you need to see the old (hidden) history records (Kind=DUP). Normal (unhidden) records are always returned.

### Return value
This method returns an array of structures with following fields:

- **NZBID** `(int)` - ID of NZB-file.
- **ID (int)** - ~~`v13.0`~~ Deprecated, use `NZBID` instead.
- **Kind** `(string)` - Kind of history item. One of the predefined text constants:
  - **NZB** for nzb-files;
  - **URL** for failed URL downloads. **NOTE**: successful URL-downloads result in adding files to download queue, a success-history-item is not created in this case;
  - **DUP** for hidden history items.
- **NZBFilename** `(string)` - Name of nzb-file, this file was added to queue from. The filename could include fullpath (if client sent it by adding the file to queue).
- **Name** `(string)` - The name of nzb-file or info-name of URL, without path and extension. Ready for user-friendly output.
- **NZBName** `(string)` - nzb-file name.
- **NZBNicename** `(string)` - ~~`v12.0`~~ Deprecated, use `NZBName` instead.
- **URL** `(string)` - URL.
- **RetryData** `(bool)` - Has completed files.
- **HistoryTime** `(int)` - Date/time when the file was added to history (Time is in C/Unix format).
- **DestDir** `(string)` - Destination directory for output files.
- **FinalDir** `(string)` - Final destination if set by one of post-processing scripts.
- **Category** `(string)` - Category for group or empty string if none category is assigned.
- **FileSizeLo** `(int)` - Initial size of all files in group in bytes, Low 32-bits of 64-bit value.
- **FileSizeHi** `(int)` - Initial size of all files in group in bytes, High 32-bits of 64-bit value.
- **FileSizeMB** `(int)` - Initial size of all files in group in MiB.
- **FileCount** `(int)` - Initial number of files in group.
- **RemainingFileCount** `(int)` - Number of parked files in group. If this number is greater than `0`, the history item can be returned to download queue using command `HistoryReturn` of method [editqueue](EDITQUEUE.md).
- **MinPostTime** `(int)` - Date/time when the oldest file in the item was posted to newsgroup (Time is in C/Unix format).
- **MaxPostTime** `(int)` - Date/time when the newest file in the item was posted to newsgroup (Time is in C/Unix format).
- **TotalArticles** `(int)` - Total number of articles in all files of the group.
- **SuccessArticles** `(int)` - Number of successfully downloaded articles.
- **FailedArticles** `(int)` - Number of failed article downloads.
- **Health `(int)` - Final health of the group, in permille. 1000 means 100.0%. Higher values are better.
- **CriticalHealth** `(int)` - Calculated critical health of the group, in permille. 1000 means 100.0%. The critical health is calculated based on the number and size of par-files. Lower values are better.
- **Deleted** `(bool)` - ~~`v12.0`~~ Deprecated, use DeleteStatus instead.
- **DownloadedSizeLo** `(int)` - `v14.0` Amount of downloaded data for group in bytes, Low 32-bits of 64-bit value.
- **DownloadedSizeHi `(int)` - v14.0 Amount of downloaded data for group in bytes, High 32-bits of 64-bit value.
- **DownloadedSizeMB** `(int)` - `v14.0` Amount of downloaded data for group in MiB.
- **DownloadTimeSec** `(int)` - `v14.0` Download time in seconds.
- **PostTotalTimeSec** `(int)` - `v14.0` Total post-processing time in seconds.
- **ParTimeSec** `(int)` - `v14.0` Par-check time in seconds (incl. verification and repair).
- **RepairTimeSec** `(int)` - `v14.0` Par-repair time in seconds.
- **UnpackTimeSec** `(int)` - `v14.0` Unpack time in seconds.
- **MessageCount** `(int)` - `v15.0` Number of messages stored in the item log. Messages can be retrieved with method loadlog.
- **DupeKey** `(string)` - Duplicate key. See RSS __(comming soon)__.
- **DupeScore** `(int)` - Duplicate score. See RSS __(comming soon)__.
- **DupeMode** `(string)` - Duplicate mode. One of **SCORE**, **ALL**, **FORCE**. See RSS __(comming soon)__.
- **Status** `(string)` - Total status of the download. One of the predefined text constants such as `SUCCESS/ALL` or `FAILURE/UNPACK`, etc. For the complete list see below.
- **ParStatus** `(string)` - Result of par-check/repair:
  - **NONE** - par-check wasn’t performed;
  - **FAILURE** - par-check has failed;
  - **REPAIR_POSSIBLE** - download is damaged, additional par-files were downloaded but the download was not repaired. Either the option ParRepair is disabled or the par-repair was cancelled by option ParTimeLimit;
  - **SUCCESS** - par-check was successful;
  - **MANUAL** - download is damaged but was not checked/repaired because option `ParCheck` is set to `Manual`.
- **ExParStatus** `(string)` - `v16.0` Indicates if the download was repaired using duplicate par-scan mode (option `ParScan=dupe`):
  - **RECIPIENT** - repaired using blocks from other duplicates;
  - **DONOR** - has donated blocks to repair another duplicate;
- **UnpackStatus** `(string)` - Result of unpack:
  - **NONE** - unpack wasn’t performed, either no archive files were found or the unpack is disabled for that download or globally;
  - **FAILURE** - unpack has failed;
  - **SPACE** - unpack has failed due to not enough disk space;
  - **PASSWORD** - unpack has failed because the password was not provided or was wrong. Only for rar5-archives;
  - **SUCCESS** - unpack was successful.
- **UrlStatus** `(string)` - Result of URL-download:
  - **NONE** - that nzb-file were not fetched from an URL;
  - **SUCCESS** - that nzb-file was fetched from an URL;
  - **FAILURE** - the fetching of the URL has failed.
  - **SCAN_SKIPPED** - The URL was fetched successfully but downloaded file was not nzb-file and was skipped by the scanner;
  - **SCAN_FAILURE** - The URL was fetched successfully but an error occurred during scanning of the downloaded file. The downloaded file isn’t a proper nzb-file. This status usually means the web-server has returned an error page (HTML page) instead of the nzb-file.
- **ScriptStatus `(string)` - Accumulated result of all post-processing scripts. One of the predefined text constants: `NONE`, `FAILURE`, `SUCCESS`. Also see field `ScriptStatuses`.
- **ScriptStatuses** `(struct[])` - Status info of each post-processing script. See below.
- **MoveStatus** `(string)` - Result of moving files from intermediate directory into final directory:
  - **NONE** - the moving wasn’t made because either the option `InterDir` is not in use or the par-check or unpack have failed;
  - **SUCCESS** - files were moved successfully;
  - **FAILURE** - the moving has failed.
- **DeleteStatus** `(string)` - Indicates if the download was deleted:
  - **NONE** - not deleted;
  - **MANUAL** - the download was manually deleted by user;
  - **HEALTH** - the download was deleted by health check;
  - **DUPE** - the download was deleted by duplicate check;
  - **BAD** - `v14.0` the download was marked as `BAD` by a queue-script during download;
  - **SCAN** - `v16.0` the download was deleted because the nzb-file could not be parsed (malformed nzb-file);
  - **COPY** - `v16.0` the download was deleted by duplicate check because an nzb-file with exactly same content exists in download queue or in history.
- **MarkStatus** `(string)` - Indicates if the download was marked by user:
  - **NONE** - not marked;
  - **GOOD** - the download was marked as good by user using command `Mark as good` in history dialog;
  - **BAD** - the download was marked as bad by user using command `Mark as bad` in history dialog;
- **ExtraParBlocks** `(int)` - `v16.0` amount of extra par-blocks received from other duplicates or donated to other duplicates, when duplicate par-scan mode was used (option `ParScan`=`dupe`):
  - **> 0** - has received extra blocks;
  - **< 0** - has donated extra blocks;
- **Parameters** `(struct[])` - Post-processing parameters for group. For description of struct see method [listgroups](LISTGROUPS.md).
- **ServerStats** `(struct[])` - Per-server article completion statistics.
- **Log** `(struct[])` - ~~`v13.0`~~ Deprecated, was never really used.

## Field Status
Field `Status` can be set to one of the following values:

**For history items with `Kind=NZB`**

- **SUCCESS/ALL** - Downloaded and par-checked or unpacked successfully. All post-processing scripts were successful. The download is completely OK.
- **SUCCESS/UNPACK** - Similar to `SUCCESS/ALL` but no post-processing scripts were executed. Downloaded and unpacked successfully. Par-check was successful or was not necessary.
- **SUCCESS/PAR** - Similar to `SUCCESS/ALL` but no post-processing scripts were executed. Downloaded and par-checked successfully. No unpack was made (there are no archive files or unpack was disabled for that download or globally). At least one of the post-processing scripts has failed.
- **SUCCESS/HEALTH** - Download was successful, download health is 100.0%. No par-check was made (there are no par-files). No unpack was made (there are no archive files or unpack was disabled for that download or globally).
- **SUCCESS/GOOD** - The download was marked as good by user using command Mark as good in history dialog.
- **SUCCESS/MARK** - `v15.0` The download was marked as success by user using command `Mark as success` in history dialog.
- **WARNING/SCRIPT** - Downloaded successfully. Par-check and unpack were either successful or were not performed. At least one post-processing script has failed.
- **WARNING/SPACE** - Unpack has failed due to not enough space on the drive.
- **WARNING/PASSWORD** - Unpack has failed because the password was not provided or was wrong.
- **WARNING/DAMAGED** - Par-check is required but is disabled in settings (option `ParCheck=Manual`).
- **WARNING/REPAIRABLE** - Par-check has detected a damage and has downloaded additional par-files but the repair is disabled in settings (option `ParRepair=no`).
- **WARNING/HEALTH** - Download health is below 100.0%. No par-check was made (there are no par-files). No unpack was made (there are no archive files or unpack was disabled for that download or globally).
- **DELETED/MANUAL** - The download was manually deleted by user.
- **DELETED/DUPE** - The download was deleted by duplicate check.
- **DELETED/COPY** - `v16.0` The download was deleted by duplicate check because this nzb-file already exists in download queue or in history.
- **DELETED/GOOD** - `v16.0` The download was deleted by duplicate check because there is a duplicate history item with status `GOOD` or a duplicate hidden history item with status`SUCCESS` which do not have any visible duplicates.
- **FAILURE/PAR** - The par-check has failed.
- **FAILURE/UNPACK** - The unpack has failed and there are no par-files.
- **FAILURE/MOVE** - An error has occurred when moving files from intermediate directory into the final destination directory.
- **FAILURE/SCAN** - `v16.0` nzb-file could not be parsed (malformed nzb-file).
- **FAILURE/BAD** - The download was marked as bad by user using command `Mark as bad` in history dialog.
- **FAILURE/HEALTH** - Download health is below critical health. No par-check was made (there are no par-files or the download was aborted by health check). No unpack was made (there are no archive files or unpack was disabled for that download or globally or the download was aborted by health check).

**For history items with `Kind=URL`**

- **DELETED/MANUAL** - The download was manually deleted by user.
- **DELETED/DUPE - The download was deleted by duplicate check.
- **WARNING/SKIPPED** - The URL was fetched successfully but downloaded file was not nzb-file and was skipped by the scanner.
- **FAILURE/FETCH** - Fetching of the URL has failed.
- **FAILURE/SCAN** - The URL was fetched successfully but an error occurred during scanning of the downloaded file. The downloaded file isn’t a proper nzb-file. This status usually means the web-server has returned an error page (HTML page) instead of the nzb-file.

**For history items with `Kind=DUP`**

- **SUCCESS/HIDDEN** - The hidden history item has status `SUCCESS`.
- **SUCCESS/GOOD** - The download was marked as good by user using command Mark as good in history dialog.
- **FAILURE/HIDDEN** - The hidden history item has status `FAILURE`.
- **DELETED/MANUAL** - The download was manually deleted by user.
- **DELETED/DUPE** - The download was deleted by duplicate check.
- **FAILURE/BAD** - The download was marked as bad by user using command `Mark as bad` in history dialog.

### Field ScriptStatuses
Contains an array of structs with following fields:

- **Name** `(string)` - Script name.
- **Status** `(string)` - Result of post-processing script exection. One of the predefined text constants: `NONE`, `FAILURE`, `SUCCESS`.

### Field ServerStats
Contains an array of structs with following fields:

- **ServerID** `(int)` - Server number as defined in section `news servers` of the configuration file.
- **SuccessArticles** `(int)` - Number of successfully downloaded articles.
- **FailedArticles** `(int)` - Number of failed articles.
