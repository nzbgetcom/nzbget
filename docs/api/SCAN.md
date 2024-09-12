## API-method `scan`

### Signature
``` c++
bool scan(bool SyncMode);
```

### Description
Request rescanning of incoming directory for nzb-files (option `NzbDir`).

### Arguments
- **SyncMode** `(bool)` - `Optional`. waits for completing of scan
  before reporting the status.

### Return value
Always `true`
