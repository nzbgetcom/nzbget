![NZBGet logo](https://nzbget.com/img/logo.svg)

# NZBGet builds from [nzbgetcom/nzbget](https://github.com/nzbgetcom/nzbget) repository

NZBGet is an efficient, open-source Usenet software designed for downloading binary content from Usenet newsgroups.

# Supported Architectures

| Architecture 
|:-
| x86-64       
| arm64        
| armhf        

# Version Tags

| Tag          | Description
|:-------------|-
| latest       | Stable nzbget releases (from `main` repository branch)
| testing      | Development nzbget builds (from `develop` repository branch)
| debug        | Development nzbget builds with debug enabled (from `develop` repository branch)
| v*           | Version-specific release builds (like v22.0 for 22.0 nzbget release)

# Usage

[docker-compose](https://docs.docker.com/compose/) (recommended)
```
---
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

For development purposes the Docker Image can be build using the locally cloned repository (`docker` folder): 

```
git clone https://github.com/nzbgetcom/nzbget.git
docker compose -f docker/docker-compose.yml up --build
-or-
cd docker
docker build . -t nzbget-local
```

Dockerfile supports next build arguments:

| Argument	      | Description
|:----------------|-
| UNRAR6_VERSION  | Unrar 6 version
| UNRAR7_VERSION  | Unrar 7 version
| UNRAR7_NATIVE   | Build native unrar (see below)
| MAKE_JOBS       | Number of make jobs for speed up build

# ghcr.io

Docker images also available on [GitHub](https://github.com/nzbgetcom/nzbget/pkgs/container/nzbget). For use - replace `nzbgetcom/nzbget:TAG` with `ghcr.io/nzbgetcom/nzbget:TAG` in above examples.

# Python version

NZBGet docker image bundled with Python 3.11

# Troubleshooting max speed issues

In case a linux image or docker image is slower than expected, here are some tips to increase download speed:

1. Increase number of server connections (NEWS-SERVERS -> Connections) - default is 8, and 16 and 32 are worth trying
2. For slower machines/hosts - increase article read chunk size from 4 to 64 (CONNECTION -> ArticleReadChunkSize). This is new setting available in v23.

# Unrar 7 support

The NZBGet docker image contains the optimized unrar7 binary. To use unrar7, change in settings UNPACK - UnrarCmd to `unrar7`.
Unrar 7 built with next march parameters:

- x86_64: x86-64-v2
- arm64:  armv8-a+crypto+crc
- armhf:  armv7-a

More information about unrar performance: [Performance tips](https://github.com/nzbgetcom/nzbget/blob/develop/docs/PERFORMANCE.md#unrar)

# Native unrar 7 build support

To build image on hardware which support crypto acceleration with native-optimized unrar can be used docker-compose like this (also needed entrypoint.sh and Dockerfile from [official repository](https://github.com/nzbgetcom/nzbget/tree/develop/docker)):


```
---
services:
  nzbget:
    build:
      context: .
      args:
        # make jobs == host cpu cores
        MAKE_JOBS: 4
        # build native unrar
        UNRAR7_NATIVE: "true"
    environment:
      - PUID=1000
      - PGID=1000
      - TZ=Europe/London
      - NZBGET_USER=nzbget
      - NZBGET_PASS=tegbzn6789
    volumes:
      - ./config:/config
      - ./downloads:/downloads
    ports:
      - 6789:6789
    restart: unless-stopped
```
