# QNAP nzbget packages

We support QNAP via native qpkg packages, built with QNAP toolchains (only x86/x86_64/arm_64 QNAP architectures) and buildroot qpkg, repacked from linux installer (all QNAP architectures)

## Installing

Prerequsites: Enable installation of applications without digital signature (`AppCenter` - `Settings` - `Allow installation of applications without a valid digital signature`)

To install nzbget for QNAP download qpkg for your architecture, then from QNAP AppCenter select `Install Manually` - browse for downloaded qpkg and press `Install`
For digital signature warning select `I understand the risks and want to install this application` and press `Install`.
After installation - Press `Open` in AppCenter or click NZBGet icon on desktop. Default login/password for WebUI
```
Login: nzbget
Password: tegbzn6789
```

## Configuring

Change `PATHS` - `MainDir` to point to shared folder location, for example
```
/share/CACHEDEV1_DATA/Public/nzbget
```
By default, nzbget will download all files to package directory, inaccessible from shares.

## Extensions

QNAP packaged with python2. To support python3 extensions, need to install Python3 package from QNAP Store, and add to `EXTENSION SCRIPTS` - `ShellOverride` path to python3 executable like this:
```
.py=/share/CACHEDEV1_DATA/.qpkg/Python3/python3/bin/python3;
```
