## API-method `writelog`

### Signature
``` c++
bool writelog(string Kind, string Text);
```

### Description
Append log-entry into server’s log-file and on-screen log-buffer.

### Arguments
- **Kind** `(string)` - Kind of log-message. Must be one of the following strings: `INFO`, `WARNING`, `ERROR`, `DETAIL`, `DEBUG`. Debug-messages are available, only if the program was compiled in debug-mode.
- **Text** `(string)` - Text to be added into log.

### Return value
`true` on success or `failure` result on error.
