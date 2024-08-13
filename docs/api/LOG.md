## API-method `log`

### Signature
``` c++
struct[] log(int IDFrom, int NumberOfEntries);
```

### Description
This method returns entries from screen’s log-buffer. The size of this buffer is limited and can be set via option LogBufferSize. Which messages should be saved to screen-log and which should be saved to log-file can be set via options `DetailTarget`, `InfoTarget`, `WarningTarget`, `ErrorTarget` and `DebugTarget`.

### Arguments
- **IDFrom** `(int)` - The first ID to be returned.
- **NumberOfEntries** `(int)` - Number of entries, which should be returned.

**NOTE**: only one parameter - either `IDFrom` or `NumberOfEntries` - can be specified. The other parameter must be `0`.

**TIP**: If your application stores log-entries between sub-sequential calls to method log(), the usage of parameter IDFrom is recommended, since it reduces the amount of transferred data.

### Return value
This method returns array of structures with following fields:

- **ID** `(int)` - ID of log-entry.
- **Kind** `(string)` - Class of log-entry, one of the following strings: `INFO`, `WARNING`, `ERROR`, `DEBUG`.
- **Time** `(int)` - Time in C/Unix format (number of seconds since 00:00:00 UTC, January 1, 1970).
- **Text** `(string)` - Log-message.

**NOTE**: if there are no entries for requested criteria, an empty array is returned.
