## API-method `writelog`

### Signature
``` c++
bool writelog(string Kind, string Text);
```

_Append log-entry into server’s log-file and on-screen log-buffer_

### Arguments
- `Kind (string)` - Kind of log-message. Must be one of the following strings: `INFO`, `WARNING`, `ERROR`, `DETAIL`, `DEBUG`. Debug-messages are available, only if the program was compiled in debug-mode.
- `Text (string)` - Text to be added into log.

### Return value
Always `true`.
