## About NServ

NServ is a simple NNTP server developed for [functional testing](FUNCTIONAL_TESTING.md) of NZBGet.

Technically NServ is integrated into NZBGet as a sub-module. NServ is compiled during building process of NZBGet and can therefore be used on any platform supported by NZBGet. It’s rather a small module without extra dependencies and you shouldn’t worry about increased nzbget binary size.

Although NServ is primary used for functional testing it can also be used standalone for other purposes such as debugging NZBGet or for speed tests of NZBGet or even other download clients. NServ is fast although not very much optimized.

**Note:** NServ is available in NZBGet starting from v18.

## Usage

To start NServ pass parameter `--nserv` to nzbget binary:

```bash
  /path/to/nzbget --nserv
```

When nzbget is started with this parameter it executes the nserv-module; none of other nzbget code is executed in this case (nzbget will not start as server or daemon).

The command `--nserv` without other parameters only prints usage instructions for NServ. To start NServ as server it requires one additional parameter:

```bash
  /path/to/nzbget --nserv -d /path/to/datadir
```

**Datadir** is any directory whose content becomes available for NNTP clients.

There are also few other optional parameters to control logging level etc. A help screen printed by `nzbget --nserv` briefly explains them.

After NServ is started it keeps running until terminated with `Ctrl+C`.

## Port

NServ binds to all IP-addresses (0.0.0.0) and to port set via parameter `-p` (6791 by default).

## TLS/SSL

By default NServ works in plain mode but can also be started in TLS/SSL mode using parameter `-s` followed by paths to certificate-file and key-file:

```bash
/path/to/nzbget --nserv -d /path/to/datadir -s /path/to/nserv.cert /path/to/nserv.key
```
If you want a trusted certificate, and have a working Let's Encrypt setup, you can use the certifcate from there:

```bash
/path/to/nzbget --nserv -d /path/to/datadir -s /etc/letsencrypt/live/host.example.com/fullchain.pem /etc/letsencrypt/live/host.example.com/privkey.pem
```
Note: you probably need to be root to read from those letsencrypt directories. Or copy the keys to another, safe place.

## Authorization

Authorization isn’t required. Attempts to authorize are accepted and always succeed regardless of username and password.

## Uploading files to NServ

No uploading is needed. All files stored in data directory (set by parameter `-d`) are accessible for news clients via NNTP using special syntax of message-id. That makes NServ very lightweight and easy to install NNTP server suitable for testing purposes.

## Message-ID

In NNTP message-id identifies an article and is an arbitrary opaque string. Command `ARTICLE ` sent by NNTP-client to NNTP-server asks the server to return the message, for example:

```bash
  ARTICLE
```

In NServ message-id has a special format which includes information about file and portion of the file to be returned:

```bash
  ARTICLE
```

where:

- **xxx** - part number (integer)
- **yyy** - offset from which to read the files (integer)
- **zzz** - size of file block to return (integer)
- **1,2,3** - list of server instance ids, which have the article (optional), if the list is given and current server is not in the list the “article not found”-error is returned.

### Examples

- `ARTICLE ` - return first 50000 bytes starting from beginning
- `ARTICLE ` - return 50000 bytes starting from offset 50000
- `ARTICLE ` - article is missing on server instance 1 (see below).

## Multiple server instances

NServ supports testing of main and backup news servers. To perform such a test NServ must be started with multiple server instances, using parameter `-i `, for example:

```bash
  /path/to/nzbget --nserv -d /path/to/datadir -p 6781 -i 3
```

The first instance runs on port 6781, the second on 6782 and so on. Each server instance has an id associated with it, starting from 1.

When downloading articles the message-id may include server instances ids, for example in command `ARTICLE ` part `!2,3` means that the article is available on server instances 2 and 3 but is missing on server instance 1. Server instances 2 and 3 will return article body as usual whereas server instance 1 will return error message “430 No Such Article Found”.

This makes it possible to construct nzb-files which work on certain server instances but do not work on others.

## Generating nzb-files

Nzb-files can be constructed manually using format described above. To make life easier NServ can generate nzb-files for you; use parameter `-z` for that:

```bash
  /path/to/nzbget --nserv -d /path/to/datadir -z 500000 -q
```

Here `500000` is article size to use and additional parameter `-q` tells NServ to terminate after the files are generated. If executed without that parameter NServ will start normally.

NServ scans all files and directories in **Datadir** (only on top level) and creates nzb-files for each item (file or directory on the top level of Datadir). Created files are written into Datadir. Existed nzb-files are not overwritten; if you need to rebuild nzb-files delete old files first.

## Using cache

NServ serves articles in yEnc format. Encoding takes CPU resources. By default encoding is preformed each time a file is served. In order to save CPU resources NServ supports caching of encoded articles. When the cache is active the encoded articles are saved into cache directory and then reused on sub-sequential requests of the same articles.

To activate caching add parameter `-c ` when starting NNTP server:

```bash
  /path/to/nzbget --nserv -d /path/to/datadir -c /path/to/cachedir
```

The first download of nzb will take more time due to disk activity needed to save encoded articles into disk. Sub-sequential downloads of the same nzb should be much faster compared to using NServ without cache. Make sure to use SSD or RAM-drive as cache directory. Putting cache on a convenient HDD may decrease performance compared to not using cache at all.

The cache data is stored permanently and is reused on next starts of the server; there is no need to perform cache filling after each server start.
