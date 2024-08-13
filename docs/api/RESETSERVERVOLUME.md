## API-method `resetservervolume`

### Signature
``` c++
bool resetservervolume(int ServerId, string Sounter);
```

### Description
Reset download volume statistics for a specified news-server.

### Arguments
- **ServerId** `(int)` - Server ID to reset.
- **Counter** `(string)` - The custom counter.

### Return value
`true` on success or `false` on failure.
