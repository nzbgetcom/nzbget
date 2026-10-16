## About

Feed extensions are called after an RSS feed is read and before it is processed by NZBGet.

The extension receives the downloaded feed file and can check or modify it before NZBGet parses it.

## Writing feed extensions

> Please read [Extensions](EXTENSIONS.md) for general information about extensions first.

An extension is called for feeds when its `kind` is set to `FEED` in `manifest.json` (see [`"kind"`](EXTENSIONS.md#kind)). Extensions of the old format (without `manifest.json`) declare this with a `### NZBGET FEED SCRIPT` line in the script file.

The extension is called once for each feed, after the feed file has been downloaded and before it is parsed. The path of the feed file is passed in the environment variable `NZBFP_FILENAME`; the extension may rewrite that file — the items of the resulting file are the ones NZBGet processes afterwards.

Which extensions are called is set with the option `FeedN.Extensions` for a specific feed. If that option is empty, the extensions from the global option `Extensions` are used.

The extension must exit with the code `93` on success. With any other exit code the feed is marked as failed, its items are not processed and the log shows `Feed-script <name> for Feed<id> failed`.

Feed extensions are also executed when a feed is previewed in the web interface.

## Feed information

### The information about the feed which is currently processed:

- **NZBFP_FILENAME** - Path to the downloaded feed file.
- **NZBFP_FEEDID** - ID of the feed (`1` for `Feed1`).

The program options are passed in `NZBOP_*` variables (for example `NZBOP_VERSION`), the options of the extension itself in `NZBPO_*` variables (see [Extensions](EXTENSIONS.md)).
