## API-method `append`

### Signature
``` c++
int append(
  string Filename, 
  string Content, 
  string Category,
  int Priority, 
  bool AddToTop, 
  bool AddPaused, 
  string DupeKey,
  int DupeScore, 
  string DupeMode, 
  bool AutoCategory,
  struct[] PPParameters
);
```

_Add an NZB, archive file, or URL to the download queue_

### Arguments
- **Filename** `(string)` - name of the file with extension which determines how it's handled (e.g., my-file.nzb, project.zip, data.rar). This parameter can be an empty string if **Content** contains a URL; in that case, the filename is read from HTTP headers. If `Filename` is provided it must have correct extension (usually `.nzb`, `.zip`, `.rar`) according to file content. Files without a recognized extension are not added to the queue directly but are passed to scan-scripts for further processing.
- **Category** `(string)` - category for nzb-file. Can be empty string.
- **Priority** `(int)` - priority for nzb-file. 0 for `normal priority`, positive values for high priority and negative values for low priority. Downloads with priorities equal to or greater than 900 are downloaded and post-processed even if the program is in paused state (force mode). Default priorities are: -100 (very low), -50 (low), 0 (normal), 50 (high), 100 (very high), 900 (force).
- **AddToTop** `(bool)` - `true` if the file should be added to the top of the download queue or `false` if to the end.
- **AddPaused** `(bool)` - `true` if the file should be added in paused state.
DupeKey (string) - duplicate key for nzb-file. See [RSS](../usage/RSS.md).
- **DupeScore** `(int)` - duplicate score for nzb-file. See [RSS](../usage/RSS.md).
- **DupeMode** `(string)` - duplicate mode for nzb-file. See [RSS](../usage/RSS.md).
- **AutoCategory** `(bool)` - If true, the category will be automatically detected from the NZB file (if available).
- **PPParameters** `(array)` - `v16.0` post-processing parameters. The array consists of structures with following fields:
  - **Name** `(string)` - name of post-processing parameter.
  - **Value** `(string)` - value of post-processing parameter.

### Return value
Positive number representing `NZBID` of the queue item. `0` and negative numbers represent error codes. Current version uses only error code `0`, newer versions may use other error codes for detailed information about error.

### Example

```python
from xmlrpc.client import ServerProxy
from base64 import standard_b64encode

server = ServerProxy("http://nzbget:tegbzn6789@localhost:6789/xmlrpc")
filename = "/tmp/test.nzb"
with open(filename, "rb") as f:
    nzb_content = f.read()
base64_nzb_content = standard_b64encode(nzb_content).decode()
server.append(
    filename,
    base64_nzb_content,
    "software",
    0,
    False,
    False,
    "",
    0,
    "SCORE",
    [
      ("*unpack:", "yes"), 
      ("EMail.py:", "yes")
    ],
)
```
