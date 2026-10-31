## Status of this document

The document describes methods available in NZBGet `version 13.0 and later`. The document is updated after a new stable version is out. Current testing version may have methods or fields not described here; these methods or fields may change before getting stable.

## Protocols

NZBGet supports `XML-RPC`, `JSON-RPC` and `JSON-P-RPC`. RPC-protocols allow to control the program from other applications. Many programming languages have libraries, which make the usage of `XML-RPC`, `JSON-RPC` and `JSON-P-RPC` very simple. The implementations of all three protocols in NZBGet are equal: all methods are identical. You may choose the protocol, which is better supported by your programming language.

## Authentication

NZBGet has three pairs of username/password:

- options `ControlUsername` and `ControlPassword` - Full access, usually used when connecting to web-interface;
- options `RestrictedUsername` and `RestrictedPassword` - `v15.0` Restricted user can control the program with few restrictions. He has access to web-interface and can see most program settings. He can not change program settings and can not view security related options or options provided by extension scripts. In terms of RPC-API see [Restricted user](#restricted-user).
- options `AddUsername` and `AddPassword` - `v15.0` This user has only two permissions:
  - add new downloads using RPC-methods `append` and `appendurl`;
  - check program version using RPC-method `version`.

### HTTP basic authentication

The RPC server uses HTTP basic authentication by default. The URL would be something like:

- for **XML-RPC**: `http://username:password@localhost:6789/xmlrpc`
- for **JSON-RPC**: `http://username:password@localhost:6789/jsonrpc`
- for **JSON-P-RPC**: `http://username:password@localhost:6789/jsonprp`
 
If HTTP basic authentication is somewhat problematic the username/password can also be passed in the URL as the first part of the path:

- `http://localhost:6789/username:password/xmlrpc`

It is also possible to send the credentials in the header `X-Authorization: Basic <base64(username:password)>` (used by the web-interface) or in the standard `Authorization: Basic ...` header.

With the option `FormAuth` enabled (the default) the web-interface shows its own login form instead of the browser's built-in dialog. This does not affect RPC clients: the server still replies to non-web-interface requests with `HTTP 401` and the `WWW-Authenticate: Basic realm="NZBGet"` challenge, so tools that do not send credentials preemptively (curl, wget, Sonarr/Radarr and other integrations) keep working as before. Only requests marked as coming from the web-interface (header `X-NZBGet-Client: webui`) receive the `401` response without the challenge, which prevents the browser from popping up its own dialog. With `FormAuth=no` the web-interface falls back to the browser's built-in dialog. The web-interface ends the session with `GET /logout`, which expires the session cookies.

### Restricted user

For scripts, automation and third-party applications (for example the *arr suite) use the separate user with restricted access instead of the administrator credentials. Set the options `RestrictedUsername` and `RestrictedPassword` and pass them with any of the methods described above (the user-name/password part in the URL, basic authentication or `X-Authorization: Basic ...`). Such a user can read the queue and history and control downloads, but has no access to the administrator-only methods listed below. Each call to them fails with `faultCode 401` and `faultString "Access denied"`; the same check is applied to every method called inside `system.multicall`.

Specifically, the restricted user:

- cannot change the configuration: `saveconfig`, `editserver`, `resetservervolume`. Methods `config` and `loadconfig` work, but return `***` for the protected options (passwords and feed URLs containing credentials);
- cannot control the daemon: `shutdown`, `reload`, `dump`, `clearlog`, `startupdate`, `logupdate`;
- cannot manage extensions or run scripts: `downloadextension`, `updateextension`, `deleteextension`, `testextension`, `startscript`, `logscript`. Listing the cached extensions via `loadextensions` is allowed, but reloading them from disk (`loadextensions` with `loadFromDisk=true`) is administrator-only;
- can view the configured feeds (`viewfeed`), but cannot preview a feed from an arbitrary client-supplied URL (`previewfeed`).

`Security warning`: HTTP authentication is not secure. Although the password is encoded using Base64 it is not encrypted. For secure communication use HTTPS: either enable the built-in TLS in NZBGet (`SecureControl=yes`, see options `SecureCert`, `SecureKey`, `SecurePort`) or put NZBGet behind a reverse proxy that terminates TLS (Traefik, Caddy, Nginx). When a reverse proxy forwards the header `X-Forwarded-Proto: https`, NZBGet marks the web-interface session cookie as `Secure`.

## Features and limitations

- Multicalls are supported for `XML-RPC` and not supported for `JSON-RPC`.
- Introspection is not supported.
- Only positional parameters are supported in `JSON-RPC`. Named parameters are not supported. Therefore parameter names are ignored but the order of parameters is important. All parameters are mandatory.
- Each call to `JSON-P-RPC` has one additional parameter - the name of callback-function. This parameter must be named `callback` and must be passed first (before any other parameters).
- 64 bit integers are returned in two separate fields `Hi` and `Lo` (for example `FileSizeHi` and `FileSizeLo`). These fields are unsigned 32 bit integers. Although dynamic languages such as PHP or Python have no problems with these fields the `XML-RPC` specification does not allow unsigned integers. This may cause troubles in statically typed languages such as Java or C++ if `XML-RPC`-parser expects only signed integers in 32 bit range. As a solution use `JSON-RPC` (which does allow unsigned integers) instead of `XML-RPC`.

## Program control

- [version](VERSION.md)
- [shutdown](SHUTDOWN.md)
- [reload](RELOAD.md)

## Queue and history

- [listgroups](LISTGROUPS.md)
- [listfiles](LISTFILES.md)
- [history](HISTORY.md)
- [append](APPEND.md)
- [editqueue](EDITQUEUE.md)
- [scan](SCAN.md)

## Feeds

- [fetchfeed](FETCHFEED.md)
- [viewfeed](VIEWFEED.md)
- [previewfeed](PREVIEWFEED.md)

## Status, logging and statistics

- [status](STATUS.md)
- [sysinfo](SYSINFO.md)
- [systemhealth](SYSTEMHEALTH.md)
- [log](LOG.md)
- [writelog](WRITELOG.md)
- [loadlog](LOADLOG.md)
- [logscript](LOGSCRIPT.md)
- [logupdate](LOGUPDATE.md)
- [servervolumes](SERVERVOLUMES.md)
- [resetservervolume](RESETSERVERVOLUME.md)

## Pause and speed limit

- [rate](RATE.md)
- [pausedownload](PAUSEDOWNLOAD.md)
- [resumedownload](RESUMEDOWNLOAD.md)
- [pausepost](PAUSEPOST.md)
- [resumepost](RESUMEPOST.md)
- [pausescan](PAUSESCAN.md)
- [resumescan](RESUMESCAN.md)
- [scheduleresume](SCHEDULERESUME.md)

## Configuration

- [config](CONFIG.md)
- [loadconfig](LOADCONFIG.md)
- [saveconfig](SAVECONFIG.md)
- [configtemplates](CONFIGTEMPLATES.md)

## Extensions

- [loadextensions](LOADEXTENSIONS.md)
- [downloadextension](DOWNLOADEXTENSION.md)
- [updateextension](UPDATEEXTENSION.md)
- [deleteextension](DELETEEXTENSION.md)

## Tests

- [testextension](TESTEXTENSION.md)
- [testserver](TESTSERVER.md)
- [testserverspeed](TESTSERVERSPEED.md)
- [testdiskspeed](TESTDISKSPEED.md)
- [testnetworkspeed](TESTNETWORKSPEED.md)
