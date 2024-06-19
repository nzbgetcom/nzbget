# About
`build-nzbget.ps1` is a PowerShell script which is used to build Windows nzbget binaries and installer

# Prerequisites

## Execution policy

To run PowerShell scripts check ExecutionPolicy on yours system. From PowerShell prompt run:
```
Get-ExecutionPolicy
```
It should be minimum at `RemoteSigned` level. `Unrestricted` works too. To set desired policy, run as an Administrator from PowerShell prompt:
```
Set-ExecutionPolicy RemoteSigned
```


## Build package

- [vcpkg](https://vcpkg.io) with all needeed libraries (32-bit/64-bit must be installer separately)
    - more info in [Windows readme](../docs/WINDOWS.md)
    - script assumes that vcpkg is installed in `c:\vcpkg`
- [CMake](https://cmake.org)
- sed
    - 4.2.1 binary can be found [here](https://github.com/chapvic/sed/releases)
    - script assumes that sed.exe is placed in `c:\nzbget\sed\sed.exe`

## Build installer

- [NSIS 3.09](https://nsis.sourceforge.io)
    - script assumes that NSIS is installed in `c:\nzbget\nsis`
- [Advanced logging](https://nsis.sourceforge.io/Special_Builds)
    - download and extract to installed NSIS directory
- [AccessControl Plugin](https://nsis.sourceforge.io/AccessControl_plug-in)
    - download and place `AccessControl.dll` from `Plugins\i386-unicode` to NSIS's `Plugins\x86-unicode`
- [Simple Service Plugin](https://nsis.sourceforge.io/NSIS_Simple_Service_Plugin)
    - download unicode version and place `SimpleSC.dll` to NSIS's `Plugins\x86-unicode`

## Unpackers
For building nzbget package you must have downloaded 7zip/unrar unpackers to `$ToolsRoot\image` (default to `c:\nzbget\image`). 32/64 bit unpackers should be placed in `$ToolsRoot\image\32` and `$ToolsRoot\image\64` folders respectively. This can be done by running `build-nzbget.ps1` with `-DownloadUnpackers` parameter:
```
.\windows\build-nzbget.ps1 -DownloadUnpackers
```
Full `$ToolsRoot` layout:
```
nzbget
├── image
│   ├── 32
│   │   ├── 7za.exe
│   │   └── unrar.exe
│   └── 64
│       ├── 7za.exe
│       └── unrar.exe
├── nsis
│   ├── NSIS home
└── sed
    └── sed.exe
```

# Building NZBGet
Run from PowerShell in cloned repository:
```
.\windows\build-nzbget.ps1 [options]
```
`[options]` can be:
| Option             | Description |
|--------------------|-
| -BuildDebug        | build debug package
| -BuildRelease      | build release package
| -Build32           | build 32-bit package
| -Build64           | build 64-bit package
| -BuildSetup        | build NSIS installer package
| -BuildTesting      | build testing package (add VersionSuffix=`-testing-$yyyyMMdd` to package version)
| -DownloadUnpackers | download unpackers (see above)


Default parameters (if script invoked without options):
```
.\windows\build-nzbget.ps1 -BuildRelease -Build32 -Build64 -BuildSetup
```

If at least one parameter is present, all the others must be explicitly specified.
For example (build release/debug 32/64 package, release/debug installer, and add testing VersionSuffix):
```
.\windows\build-nzbget.ps1 -BuildRelease -BuildDebug -Build32 -Build64 -BuildSetup -BuildTesting
```

# Output files

| File(s)                                | Description |
|----------------------------------------|-
| build\Release32\Release\nzbget.exe     | 32-bit release nzbget binary
| build\Release64\Release\nzbget.exe     | 64-bit release nzbget binary
| build\Debug32\Debug\nzbget.exe         | 32-bit debug nzbget binary
| build\Debug64\Debug\nzbget.exe         | 64-bit debug nzbget binary
| build\distrib\nzbget\                  | latest built nzbget package
| build\nzbget-*-windows-setup.exe       | nzbget release installer
| build\nzbget-*-windows-debug-setup.exe | nzbget debug installer
