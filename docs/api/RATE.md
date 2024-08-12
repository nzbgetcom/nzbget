## API-method `rate`

### Signature
``` c++
bool rate(int Limit);
```

_Set download speed limit_

### Arguments
- `Limit (int)` - new download speed limit in KBytes/second. Value `0` disables speed throttling.

### Return value
Usually `true` but may return `false` if parameter Limit was out of range.
