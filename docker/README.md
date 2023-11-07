![NZBGet logo](https://nzbget.com/img/logo.svg)

# Official NZBGet builds from [nzbgetcom/nzbget](https://github.com/nzbgetcom/nzbget) repository

NZBGet is an efficient, open-source Usenet software designed for downloading binary content from Usenet newsgroups.

# Supported Architectures

| Architecture | Tag
|:-------------|-
| x86-64       | amd64-\<version tag\>
| arm64        | arm64v8-\<version tag\>
| armhf        | arm32v7-\<version tag\>

# Version Tags

| Tag          | Description
|:-------------|-
| latest       | Stable nzbget releases (from `main` repository branch)
| testing      | Development nzbget builds (from `develop` repository branch)
| v*           | Version-specific release builds (like v22.0 for 22.0 nzbget release)

# Usage

[docker-compose](https://docs.docker.com/compose/) (recommended)
```
---
version: "2.1"
services:
  nzbget:
    image: nzbgetcom/nzbget:latest
    container_name: nzbget
    environment:
      - PUID=1000
      - PGID=1000
      - TZ=Europe/London
      - NZBGET_USER=nzbget     #optional
      - NZBGET_PASS=tegbzn6789 #optional
    volumes:
      - /path/to/config:/config
      - /path/to/downloads:/downloads #optional
    ports:
      - 6789:6789
    restart: unless-stopped
```

[docker cli](https://docs.docker.com/engine/reference/commandline/cli/)
```
docker run -d \
  --name=nzbget \
  -e PUID=1000 \
  -e PGID=1000 \
  -e TZ=Europe/London \
  -e NZBGET_USER=nzbget `#optional` \
  -e NZBGET_PASS=tegbzn6789 `#optional` \
  -p 6789:6789 \
  -v /path/to/config:/config \
  -v /path/to/downloads:/downloads `#optional` \
  --restart unless-stopped \
  nzbgetcom/nzbget:latest
```

# Supported environment variables

NZBGet container can be configured by passing environment variables to it. This can be done in docker-compose mode by specifying `environment:` and in cli mode by using -e switch.

| Parameter	  | Description
|:------------|-
| PUID        | UserID (see below)
| PGID        | GroupID (see below)
| TZ          | Timezone
| NZBGET_USER | User name for web auth
| NZBGET_PASS | Password for web auth

# User / Group Identifiers

When using volumes (-v flags) permissions issues can arise between the host OS and the container. To avoid this problem we allow to specify the user PUID and group PGID.

The example above uses PUID=1000 and PGID=1000. To find the required ids, run `id user`:
```
$ id user
uid=1000(user) gid=1000(users) groups=1000(users)
```

# Building locally

For development purposes can be used Dockerfile or docker-compose file from official repository (`docker` folder): 

```
git clone https://github.com/nzbgetcom/nzbget.git
cd docker
docker compose up
-or-
docker build . -t nzbget-local
```

Dockerfile supports next build arguments:

| Argument	      | Description
|:----------------|-
| NZBGET_RELEASE  | Branch name or tag to build from
| UNRAR_VERSION   | Unrar version
| MAKE_JOBS       | Number of make jobs for speed up build
