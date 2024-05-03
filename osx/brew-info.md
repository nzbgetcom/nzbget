# Install NZBGet via Homebrew

## Prerequisites

Homebrew package manager (https://brew.sh).

Installation supported on Intel / Apple Silicon Macs.

macOS supported: macOS Mojave+ 10.14+

## Install
Run from console:
```
brew install nzbget
```

## Post-install

### Starting nzbget

NZBGet Homebrew package conains user service definition.
To start NZBGet, run from console:
```
brew services start nzbget
```
And this will start NZBGet at user login too.

### Accessing WebUI

Default WebUI location: http://127.0.0.1:6789

Default login credentials:
```
login: nzbget
password: tegbzn6789
```

### Unpackers

`7zip` is installed automatically as a direct dependency.

`unrar` can be installed via `rar` Homebrew Cask:

```
brew install rar --cask
```
