## API-method `rate`

### Signature
``` c++
bool rate(int Limit);
```

### Description
Set download speed limit.

### Arguments
- **Limit** `(int)` - new download speed limit in KBytes/second. Value `0` disables speed throttling.

### Return value
Usually `true` but may return `false` if parameter Limit was out of range.
