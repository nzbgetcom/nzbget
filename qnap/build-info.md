# About

"build-nzbget.sh" is a bash script which is used to build nzbget QNAP packages.


# Prerequisites

- linux x86_64 host (Ubuntu 22.04 LTS for example)

We support building nzbget for QNAP for x86 / x86_64 / arm_64 architectures only, because other platforms toolchains is too old (nzbget need gcc 4.9+ for building).

- download x86/x86_64 toolchains from http://download.qnap.com/dev/Toolchain/QNAP_cross_toolchains_64.20160606.tar
- download arm_64 toolchain from http://download.qnap.com/dev/Toolchain/aarch64-QNAP-linux-gnu.tgz
- extract toolchains to $QNAP_ROOT/toolchain (default - /qnap/toolchain), needed directory structure for script:
```
/qnap/toolchain
├── aarch64
│   └── cross-tools
├── i686
│   ├── cross-tools
│   └── fs
└── x86_64
    ├── cross-tools
    └── fs
```
- install QDK from `https://github.com/qnap-dev/QDK`
```
git clone https://github.com/qnap-dev/QDK
cd QDK
sed 's|python|python3|' -i InstallToUbuntu.sh
sudo ./InstallToUbuntu.sh install
```

## Building NZBGet

From cloned repository run
```
bash qnap/build-nzbget.sh
```

## Output files

- /qnap/nzbget/build/*.qpkg - one file per platform
