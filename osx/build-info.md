# About
`build-nzbget.sh` is a bash scripts which is used to build macOS nzbget binaries

## Prerequisites
- vcpkg package manager (https://vcpkg.io/) and several dependencies:
```
cd $HOME
git clone https://github.com/microsoft/vcpkg
cd vcpkg
./bootstrap-vcpkg.sh
```
For x64 builds:
```
./vcpkg install boost-json:x64-osx boost-optional:x64-osx libxml2:x64-osx zlib:x64-osx openssl:x64-osx
```
For arm64 builds:
```
./vcpkg install boost-json:arm64-osx boost-optional:arm64-osx libxml2:arm64-osx zlib:arm64-osx openssl:arm64-osx
```
- Xcode build tools
```
xcode-select --install
```
- cmake 3.13+

### Building NZBGet
From the cloned repository, run:
```
bash osx/build-nzbget.sh [arch] [testing]
```
- `arch` - can be
    - x64
    - arm64
    - universal (default value)
- `testing` - build testing package (add VersionSuffix=`-testing-$yyyyMMdd` to package version)

### Output files
- build/nzbget-$VERSION-bin-macos-$ARCH.zip - release archive
