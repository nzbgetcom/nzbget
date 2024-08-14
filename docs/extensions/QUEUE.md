## About 
Queue extensions are called after the download queue was changed. 
In the current version the queue extensions are called on defined events, described later. 
In the future they can be called on other events too.

## Writing queue extensions

> Please read [Extensions](EXTENSIONS.md) for general information about extensions first.

## Queue event information

 - **NZBNA_DIRECTORY** - Destination directory for downloaded files.
 - **NZBNA_FILENAME** - Filename of the nzb-file. If the file was added from nzb-directory this is the fullname with path. 
 If the file was added via web-interface it contains only filename without path.
 - **NZBNA_NZBNAME** - Nzb-name as displayed in web-interface.
 - **NZBNA_URL** - URL if the nzb-file was fetched from an URL.
 - **NZBNA_CATEGORY** - Category of nzb-file.
 - **NZBNA_PRIORITY** - Priority of nzb-file.
 - **NZBNA_NZBID** - ID of queue entry, can be used in RPC-calls.
 - **NZBNA_EVENT** - Describes why the extension was called. See below.
 - **NZBNA_URLSTATUS** - Details for event type URL_COMPLETED. One of `FAILURE`, `SCAN_SKIPPED`, `SCAN_FAILURE`.
 - **NZBNA_DELETESTATUS** - Details for event type NZB_DELETED. One of `MANUAL`, `DUPE`, `BAD`, `GOOD`, `COPY`, `SCAN`.

## Queue event type

The event type is passed with env. var NZBNA_EVENT and can have following values:
 - **NZB_ADDED** - after adding of nzb-file to queue;
 - **FILE_DOWNLOADED** - after a file included in nzb is downloaded;
 - **NZB_DOWNLOADED** - after all files in nzb are downloaded (before post-processing);
 - **NZB_DELETED** - after nzb-file is deleted from queue, by duplicate check or manually by user;
 - **URL_COMPLETED** - after an nzb-file queued with URL is fetched but could no be added for download.
In the future the list of supported events may be extended. To avoid conflicts with future NZBGet versions the extension 
must exit if the parameter has a value unknown to the extension:
```python
if os.environ.get('NZBNA_EVENT') not in ['NZB_ADDED', 'NZB_DOWNLOADED']:
    sys.exit(0)
```

In addition to checking the parameter NZBNA_EVENT the extension can tell NZBGet what events it is interested in in `manifest.json`:
```json
"queueEvents": "NZB_ADDED, NZB_DOWNLOADED",
```
That has an advantage that the extension isn’t called on events it can’t process anyway.

## Control commands

Queue extensions can change properties of nzb-file by printing special messages into standard output (which is processed by NZBGet).
To assign post-processing parameters:
```sh
echo "[NZB] NZBPR_myvar=my value"
```

The prefix `NZBPR_` will be removed. In this example a post-processing parameter 
with name `myvar` and value `my value` will be associated with nzb-file.

To cancel downloading and mark nzb as bad:
```sh
echo "[NZB] MARK=BAD"
```
When processing event NZB_DOWNLOADED, which is fired after all files are downloaded and 
just before unpack starts the extension can change final directory for nzb:
```sh
echo "[NZB] DIRECTORY=/mnt/movies"
```

## Synchronous/asynchronous and event queue

Only one queue extension is executed at a given time. If other events have occurred during executing of the 
extension the events are stored in a special queue and are processed later, when the extension terminates.

There are two kinds of events: synchronous and asynchronous. When a synchronous event must be processed NZBGet executes 
queue extension and waits for its termination before processing the nzb-file further. 
Only one event is of this kind - it’s `NZB_DOWNLOADED`.

All other events are asynchronous. When a extension is executed for an asynchronous event 
it’s possible that at the time a certain part of extension is being processed the state of 
nzb-file was changed or the file was even deleted. 
Extensions must take this possibility into account.

Although events `FILE_DOWNLOADED` are supposed to be executed after each downloaded file (part of nzb), 
no more than one event of this kind for a given nzb is preserved in the processing queue. 
This means that if during execution of queue extension multiple files were completed only one event `FILE_DOWNLOADED` 
will be processed. Therefore extension should not assume to be called for every downloaded file. 
Furthermore there is a program option EventInterval to reduce 
the number of extension executions (and system load) for event `FILE_DOWNLOADED`.
