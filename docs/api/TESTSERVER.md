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
  int timeout,
  int certVerificationLevel
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
- **certVerificationLevel** `(int)` - Certificate verification level:
  This is an enumerated type where the following integer values are recognized:
  -   **0:** None - NO certificate signing check, NO certificate hostname check
  -   **1:** Minimal - certificate signing check, NO certificate hostname check
  -   **2:** Strict - certificate signing check, certificate hostname check

### Return value
`string` result.
