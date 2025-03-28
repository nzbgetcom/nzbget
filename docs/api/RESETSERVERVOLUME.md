## API-method `resetservervolume`

### Signature
``` c++
bool resetservervolume(int ServerId, string Counter);
```

### Description
Resets download volume statistics for a specified news-server. This method allows for selective resetting of counters.

### Arguments
- **ServerId** `(int)` - Server ID to reset.
- **Counter** `(string)` - Specifies which counter to reset. The behavior depends on the value of this argument:

  - **"CUSTOM"** (case-sensitive) - Resets only the custom counter associated with the server. Use this option to clear custom statistics while preserving overall download volume data.
  - **""** (empty string) - Resets all counters associated with the server, including the overall download volume and any custom counters.

### Return value
`true` on success or `false` on failure.
