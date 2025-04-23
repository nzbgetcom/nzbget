# com.nzbget.nzbget

Flatpak for [NZBGet](https://nzbget.com/).

## Installing

### Prerequisites:

For the current user `flatpak` is installed and the `flathub` repo is added.
Ubuntu/Debian example:
```
sudo apt install flatpak flatpak-builder appstream-compose
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

### Install

From the directory with downloaded nzbget flatpak bundle run:
```
flatpak install com.nzbget.nzbget.*.flatpak
```

### Run
```
flatpak run com.nzbget.nzbget
```

### Uninstall
```
flatpak uninstall com.nzbget.nzbget
```
