# About

`build-pkg.sh` is a bash script which is used to build nzbget Linux DEB/RPM packages.


# Prerequisites

- linux x86_64 host (Ubuntu 22.04 LTS for example)
- installed packages:
```
sudo apt install rpm dpkg fakeroot
```

# Building NZBGet Linux packages

From cloned repository run
```
bash linux/pkg/build-pkg.sh [type] [arch]
```

Optional arguments:
- type: can be `deb` / `rpm`
- arch: can be `i686` / `x86_64` / `armel` / `armhf` / `aarch64` / `riscv64`

# Output files

- `build/deb` - Debian packages
- `build/rpm` - RPM packages
