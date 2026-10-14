# com.nzbget.nzbget

Flatpak for [NZBGet](https://nzbget.com/).

# Installing

## Prerequisites:

`Flatpak` is installed and the `Flathub` repository is added.

https://flatpak.org/setup/ - instructions based on your Linux distribution.

Ubuntu/Debian example:
```
sudo apt install flatpak
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

Optionally (if your distribution supports it), you can install the GNOME Software Flatpak plugin to install applications without using the command line.
```
sudo apt install gnome-software-plugin-flatpak
```

## Install

Download the nzbget flatpak bundle from https://github.com/nzbgetcom/nzbget/releases

### Command line install

Run from the directory containing the previously downloaded file:
```
flatpak install com.nzbget.nzbget.*.flatpak
```

### GNOME Software install

From File Explorer, double-click on the previously downloaded file, the Gnome Software application will open, just click on `Install` and follow the instructions on the screen.

### Run

From command line:
```
flatpak run com.nzbget.nzbget
```
Or choose NZBGet from the Applications menu of your desktop environment.

### Uninstall

From command line:
```
flatpak uninstall com.nzbget.nzbget
```
Or from Gnome Software, select the installed NZBGet application and click `Uninstall`
