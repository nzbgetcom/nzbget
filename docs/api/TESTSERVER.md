## API-method `testserver`

### Signature
``` c++
string testserver(
  string host, 
  int port, 
  string username, 
  string password, 
  bool encryption, 
  string cipher, 
  int timeout
);
```

### Description
Tries to connect to a server.

### Arguments
- **host** `(string)` - Server host.
- **port** `(int)` - Port.
- **username** `(string)` - User name.
- **password** `(string)` - User password.
- **encryption** `(bool)` - The inscription should be used.
- **cipher** `(string)` - Cipher for use.
- **timeout** `(int)` - Connection timeout.

### Return value
`string` result.
