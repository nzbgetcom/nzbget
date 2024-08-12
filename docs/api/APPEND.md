## API-method `append`

### Signature
``` c++
int append(
  string NZBFilename, 
  string NZBContent, 
  string Category,
  int Priority, 
  bool AddToTop, 
  bool AddPaused, 
  string DupeKey,
  int DupeScore, 
  string DupeMode, 
  struct[] PPParameters
);
```

_Add nzb-file or URL to download queue_

### Arguments
- `NZBFilename (string)` - name of nzb-file (with extension). This parameter can be an empty string if parameter Content contains an URL; in that case the file name is read from http headers. If `NZBFilename` is provided it must have correct extension (usually `.nzb`) according to file content. Files without `.nzb`-extension are not added to queue directly; all files however are sent to scan-scripts.
- `Content (string)` - content of nzb-file encoded with Base64 or URL to fetch nzb-file from.
- `Category (string)` - category for nzb-file. Can be empty string.
- `Priority (int)` - priority for nzb-file. 0 for `normal priority`, positive values for high priority and negative values for low priority. Downloads with priorities equal to or greater than 900 are downloaded and post-processed even if the program is in paused state (force mode). Default priorities are: -100 (very low), -50 (low), 0 (normal), 50 (high), 100 (very high), 900 (force).
- `AddToTop (bool)` - `true` if the file should be added to the top of the download queue or `false` if to the end.
- `AddPaused (bool)` - `true` if the file should be added in paused state.
DupeKey (string) - duplicate key for nzb-file. See RSS _(comming soon)_.
- `DupeScore (int)` - duplicate score for nzb-file. See RSS _(comming soon)_.
- `DupeMode (string)` - duplicate mode for nzb-file. See RSS _(comming soon)_.
- `PPParameters (array)` - `v16.0` post-processing parameters. The array consists of structures with following fields:
  - `Name (string)` - name of post-processing parameter.
  - `Value (string)` - value of post-processing parameter.

### Return value
Positive number representing `NZBID` of the queue item. `0` and negative numbers represent error codes. Current version uses only error code `0`, newer versions may use other error codes for detailed information about error.
