## API-method `testserverspeed`

## Since 
`v24.2`

### Signature
``` c++
bool testserverspeed(string nzbFileUrl, int serverId);
```

### Description
Adds a test NZB file with the highest priority to be downloaded upon receiving download speed results.

### Arguments
- **nzbFileUrl** `(string)` - NZB file url to download.
- **serverId** `(int)` - Server ID.

### Return value
`true` on success or `failure` result on error.
