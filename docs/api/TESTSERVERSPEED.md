## API-method `testserverspeed`

## Since 
`v24.2`

### Signature
``` c++
bool testserverspeed(string nzbFileUrl, int serverId);
```

_Adds a test NZB file with the highest priority to be downloaded upon receiving download speed results_

### Arguments
- `nzbFileUrl (string)` - NZB file url to download.
- `serverId (int)` - Server ID.

### Return value
Always `true`.
