# com.nzbget.nzbget

Flatpak for [NZBGet](https://nzbget.com/).

## Build from source

### Prerequisites:

For the current user `flatpak` is installed and the `flathub` repo is added.
Ubuntu/Debian example:
```
sudo apt install flatpak flatpak-builder appstream-compose
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

### Build

```
flatpak-builder --force-clean --sandbox --user --install-deps-from=flathub --ccache --repo=repo build com.nzbget.nzbget.yml
```

### Run
```
flatpak run com.nzbget.nzbget
```

### Uninstall
```
flatpak uninstall com.nzbget.nzbget
```
