## About

Scan extensions are called when a new file is found in the incoming nzb directory (option NzbDir). 
If a file is being added via web-interface or via RPC-API from a third-party app the file is saved into nzb directory and then processed. NZBGet loads only files with nzb-extension but it calls the scan extensions for every file found in the nzb directory. 
This allows for example for scan extensions which unpack zip-files containing nzb-files.

## Writing scan extensions

> Please read [Extensions](EXTENSIONS.md) for general information about extensions first.

## File information
 - NZBNP_DIRECTORY - Path to directory, where file is located. It is a directory specified by the option NzbDir or a subdirectory.
 - NZBNP_FILENAME - Name of file to be processed.
 - NZBNP_NZBNAME - Nzb-name (without path but with extension). The extension can rename the file if needed.
 - NZBNP_URL - URL this nzb-file was downloaded from.
 - NZBNP_CATEGORY - Category of nzb-file. The extension can change this setting (see later).
 - NZBNP_PRIORITY - Priority of nzb-file. The extension can change this setting (see later).
 - NZBNP_TOP - Flag indicating that the file will be added to the top of queue: 0 or 1. The extension can change this setting (see later).
 - NZBNP_PAUSED - Flag indicating that the file will be added as paused: 0 or 1. The extension can change this setting (see later).
 - NZBNP_DUPEKEY - Duplicate key (see [RSS](../usage/RSS.md)). The extension can change this setting (see later).
 - NZBNP_DUPESCORE - Duplicate score (see [RSS](../usage/RSS.md)). The extension can change this setting (see later).
 - NZBNP_DUPEMODE - Duplicate mode, one of SCORE, ALL, FORCE (see [RSS](../usage/RSS.md)). The extension can change this setting (see later).


## Control commands

Scan extensions can change nzb-name, category, priority, post-processing parameters and top-/paused-flags 
of the nzb-file by printing special messages into standard output (which is processed by NZBGet).

To change nzb-name use following syntax:
```sh
echo "[NZB] NZBNAME=my download"
```
To change category:
```sh
echo "[NZB] CATEGORY=my category"
```
To change priority:
```sh
echo "[NZB] PRIORITY=signed_integer_value"
```
For example: to set priority higher than normal:
```sh
echo "[NZB] PRIORITY=50"
```
Another example: use a negative value for "lower than normal" priority:
```sh
echo "[NZB] PRIORITY=-100"
```

Although priority can be any integer value, the web-interface 
operates with six predefined priorities: `*-100` - very low priority; `*-50` - low priority; `*0` - normal priority (default); 
`*50` - high priority; `*100` - very high priority; `*900` - force priority.

Downloads with priorities equal to or greater than `900` are downloaded and post-processed 
even if the program is in paused state (force mode).

To change duplicate key:
```sh
echo "[NZB] DUPEKEY=tv show s01e02"
```
To change duplicate score:
```sh
echo "[NZB] DUPESCORE=integer_value"
```
To change duplicate mode:
```sh
echo "[NZB] DUPEMODE=(SCORE|ALL|FORCE)"
```
To assign post-processing parameters:
```sh
echo "[NZB] NZBPR_myvar=my value"
```
The prefix `NZBPR_` will be removed. In this example a post-processing parameter with name `myvar` 
and value `my value` will be associated with nzb-file.

To change top-flag (nzb-file will be added to the top of queue):
To assign post-processing parameters:
```sh
echo "[NZB] TOP=1"
```
To change paused-flag (nzb-file will be added in paused state):
```sh
echo "[NZB] PAUSED=1"
```
Scan extensions can delete processed file, rename it or move somewhere. After the calling of the extension the file will be 
either added to 
queue (if it was an nzb-file) or renamed by adding the extension `.processed`.

> Files with extensions ".processed", ".queued" and ".error" are skipped during the directory scanning.

> Files with extension ".nzb_processed" are not passed to scan-extension before adding to queue. This feature allows scan-extension
to prevent the scanning of nzb-files extracted from archives, if they were already processed by the extension.

> Files added via RPC calls in particular from web-interface are saved into incoming nzb-directory and then processed by the extension.
