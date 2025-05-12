# About

`build-flatpak.sh` is a bash script which is used to build Flatpak x86_64 nzbget package.

# Prerequisites

1. Linux x86_64 host (Ubuntu 24.04 LTS for example)
2. Installed build dependencies (Ubuntu/Debian example):
```
sudo apt install flatpak flatpak-builder appstream-compose
```

# Building NZBGet flatpak

From the cloned repository, run:
```
bash linux/flatpak/build-flatpak.sh [nzbget_installer]
```

# Output files

- build/flatpak/*.flatpak - flatpak package
