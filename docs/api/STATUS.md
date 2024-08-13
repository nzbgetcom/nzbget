## API-method `status`

### Signature
``` c++
struct status();
```

### Description
Request for current status (summary) information.

### Return value
This method returns structure with following fields:

- **RemainingSizeLo** `(int)` - Remaining size of all entries in download queue, in bytes. This field contains the low 32-bits of 64-bit value
- **RemainingSizeHi** `(int)` - Remaining size of all entries in download queue, in bytes. This field contains the high 32-bits of 64-bit value
- **RemainingSizeMB** `(int)` - Remaining size of all entries in download queue, in MiB.
- **ForcedSizeLo** `(int)` - Remaining size of entries with FORCE priority, in bytes. This field contains the low 32-bits of 64-bit value
- **ForcedSizeHi** `(int)` - Remaining size of entries with FORCE priority, in bytes. This field contains the high 32-bits of 64-bit value
- **ForcedSizeMB** `(int)` - Remaining size of entries with FORCE priority, in MiB.
- **DownloadedSizeLo** `(int)` - Amount of data downloaded since server start, in bytes. This field contains the low 32-bits of 64-bit value
- **DownloadedSizeHi** `(int)` - Amount of data downloaded since server start, in bytes. This field contains the high 32-bits of 64-bit value
- **DownloadedSizeMB** `(int)` - Amount of data downloaded since server start, in MiB.
- **MonthSizeLo** `(int)` - Amount of data downloaded this month, in bytes. This field contains the low 32-bits of 64-bit value
- **MonthSizeHi** `(int)` - Amount of data downloaded this month, in bytes. This field contains the high 32-bits of 64-bit value
- **MonthSizeMB** `(int)` - Amount of data downloaded this month, in MiB.
- **DaySizeLo** `(int)` - Amount of data downloaded today, in bytes. This field contains the low 32-bits of 64-bit value.
- **DaySizeHi** `(int)` - Amount of data downloaded today, in bytes. This field contains the high 32-bits of 64-bit value.
- **DaySizeMB** `(int)` - Amount of data downloaded today, in MiB.
- **QuotaReached** `(bool)` - Indicates whether the quota has been reached.
- **ArticleCacheLo** `(int)` - `v14.0` Current usage of article cache, in bytes. This field contains the low 32-bits of 64-bit value.
- **ArticleCacheHi** `(int)` - `v14.0` Current usage of article cache, in bytes. This field contains the high 32-bits of 64-bit value.
- **ArticleCacheMB** `(int)` - `v14.0` Current usage of article cache, in MiB.
- **DownloadRate** `(int)` - ~~`24.2`~~ Deprecated. Current download speed, in Bytes per Second.
- **DownloadRateLo** `(int)` - `v24.2` Current download speed, in Bytes per Second. This field contains the low 32-bits of 64-bit value.
- **DownloadRateHi** `(int)` - `v24.2` Current download speed, in Bytes per Second. This field contains the high 32-bits of 64-bit value.
- **AverageDownloadRate** `(int)` - ~~`v24.2`~~ Deprecated. Average download speed since server start, in Bytes per Second.
- **AverageDownloadRateLo** `(int)` - `v24.2` Average download speed since server start, in Bytes per Second. This field contains the low 32-bits of 64-bit value.
- **AverageDownloadRateHi** `(int)` - `v24.2` Average download speed since server start, in Bytes per Second. This field contains the high 32-bits of 64-bit value.
- **DownloadLimit** `(int)` - Current download limit, in Bytes per Second. The limit can be changed via method “rate”. Be aware of different scales used by the method rate (Kilobytes) and this field (Bytes).
- **ThreadCount** `(int)` - Number of threads running. It includes all threads, created by the program, not only download-threads.
- **PostJobCount** `(int)` - Number of Par-Jobs or Post-processing script jobs in the post-processing queue (including current file).
- **ParJobCount** `(int)` - ~~`v12.0`~~ Deprecated, use PostJobCount instead.
- **UrlCount** `(int)` - Number of URLs in the URL-queue (including current file).
- **UpTimeSec** `(int)` - Server uptime in seconds.
- **DownloadTimeSec** `(int)` - Server download time in seconds.
- **ServerStandBy** `(bool)` - `false` - there are currently downloads running, `true` - no downloads in progress (server paused or all jobs completed).
- **DownloadPaused** `(bool)` - `true` if download queue is paused via first pause register (soft-pause).
- **Download2Paused** `(bool)` - ~~`v13.0`~~ Deprecated, use DownloadPaused instead.
- **ServerPaused** `(bool)` - ~~`v12.0`~~ Deprecated, use DownloadPaused instead.
- **PostPaused** `(bool)` - `true` if post-processor queue is currently in paused-state.
- **ScanPaused** `(bool)` - `true` if the scanning of incoming nzb-directory is currently in paused-state.
- **ServerTime** `(int)` - Current time on computer running NZBGet. Time is in C/Unix format (number of seconds since 00:00:00 UTC, January 1, 1970).
- **ResumeTime** `(int)` - Time to resume if set with method `scheduleresume`. Time is in C/Unix format.
- **FeedActive** `(bool)` - `true` if any RSS feed is being fetched right now.
- **FreeDiskSpaceLo** `(int)` - Free disk space on `DestDir`, in bytes. This field contains the low 32-bits of 64-bit value
- **FreeDiskSpaceHi** `(int)` - Free disk space on `DestDir`, in bytes. This field contains the high 32-bits of 64-bit value
- **FreeDiskSpaceMB** `(int)` - Free disk space on `DestDir`, in MiB.
- **TotalDiskSpaceLo** `(int)` - `v24.2` Total disk space on `DestDir`, in bytes. This field contains the low 32-bits of 64-bit value
- **TotalDiskSpaceHi** `(int)` - `v24.2` Total disk space on `DestDir`, in bytes. This field contains the high 32-bits of 64-bit value
- **TotalDiskSpaceMB** `(int)` - `v24.2` Total disk space on `DestDir`, in MiB.
- **QueueScriptCount** `(int)` - Indicates number of queue-scripts queued for execution including the currently running.
- **NewsServers** `(struct[])` - Status of news-servers, array of structures with following fields
  - **ID** `(int)` - Server number in the configuration file. For example `1` for server defined by options `Server1.Host`, `Server1.Port`, etc.
  - **Active** `(bool)` - `true` if server is in active state (enabled). `Active` doesn’t mean that the data is being downloaded from the server right now. This only means the server can be used for download (if there are any download jobs).
