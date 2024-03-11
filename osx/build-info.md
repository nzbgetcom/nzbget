# About
"build-nzbget-*.sh" is a bash scripts which is used to build macOS nzbget binaries
- [build-nzbget-universal.sh](#build-nzbget-universalsh) - universal (Intel/Apple Silicon) application  (macOS Monterey 12+)
- [build-macos-x64.sh](#build-nzbget-x64sh) - x64 application (macOS Mojave 10.14+)

## build-nzbget-universal.sh

### Prerequisites
- Homebrew package manager (https://brew.sh/) and several dependencies:

Install homebrew:
```
/bin/sh -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```
Install dependencies:
```
brew install git zlib libxml2 ncurses openssl@3 pkg-config boost
```

- Xcode build tools
```
xcode-select --install
```

### Building NZBGet
From cloned repository run
```
bash osx/build-nzbget-universal.sh
```

### Output files
- osx/build/Release/NZBGet.app - application
- osx/build/Release/nzbget-$VERSION-bin-macos-universal.zip - release archive

## build-nzbget-x64.sh

### Prerequisites
- vcpkg package manager (https://vcpkg.io/) and several dependencies:
```
cd $HOME
git clone https://github.com/microsoft/vcpkg
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg install boost-json boost-optional libxml2 zlib openssl
```
- Xcode build tools
```
xcode-select --install
```
- cmake 3.13+

### Building NZBGet
From cloned repository run
```
bash osx/build-nzbget-x64.sh
```

### Output files
- tmp/osx/build/Release/NZBGet.app - application
- tmp/nzbget-$VERSION-bin-macos-x64.zip - release archive