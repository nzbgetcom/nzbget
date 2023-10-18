About
-----
"build-nzbget.sh" is a bash script which is used to build macOS universal (Intel/Apple Silicon) application.

Prerequisites
-------------

- Homebrew package manager (https://brew.sh/) and several dependencies:

Install homebrew:
```
/bin/sh -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
Install dependencies:
```
brew install git zlib libxml2 ncurses openssl@3 pkg-config
```

- Xcode build tools
```
xcode-select --install
```

Building NZBGet
---------------
From cloned repository run
```
bash osx/build-nzbget.sh
```

Output files
------------
- osx/build/Release/NZBGet.app - application
- osx/build/Release/nzbget-$VERSION-bin-macos.zip - release archive
