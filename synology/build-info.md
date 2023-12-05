# About

"build-nzbget.sh" is a bash script which is used to build nzbget Synology packages.

# Prerequisites

## Synology toolkit and environments

Basic setup (assuming have Debian/Ubuntu host):

1. Install Synology toolkit (reference - https://help.synology.com/developer-guide/getting_started/prepare_environment.html)
```
sudo apt-get install git cifs-utils python3 python3-pip
sudo mkdir -p /toolkit
sudo chmod 777 /toolkit
cd /toolkit
git clone https://github.com/SynologyOpenSource/pkgscripts-ng
cd /toolkit/pkgscripts-ng/
git checkout DSM7.0
```

2. Install needed environments (please note - user must have sudo access)

- according to https://help.synology.com/developer-guide/appendix/platarchs.html - one per architecture, and specific separately
- we exclude `comcerto2k` - toolchain for this platform does not support C++14

```
cd /toolkit/pkgscripts-ng/
for PLATFORM in alpine armada370 armada375 armada37xx armada38x armadaxp avoton evansport monaco; do sudo ./EnvDeploy -v 7.0 -p $PLATFORM; done
```


## Building NZBGet

From cloned repository run
```
sudo bash synology/build-nzbget.sh [platform]
```
Please note - user must have sudo access. Synology toolkit requires root access to build packages.


## Output files

- /toolkit/result_spk/nzbget/*.spk - one file per platform
