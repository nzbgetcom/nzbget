# QNAP nzbget packages

QNAP packages are QPKGs repacked from the Linux installer.

## Installing

### Manual install

Prerequisites: Enable installation of applications without digital signature (`AppCenter` - `Settings` - `Allow installation of applications without a valid digital signature`)

To install NZBGet on QNAP, download the QPKG for your architecture, then in QNAP AppCenter select `Install Manually`, browse for the downloaded QPKG and press `Install`.
For the digital signature warning select `I understand the risks and want to install this application` and press `Install`.

### Install via sherpa package manager

- [Install](https://github.com/OneCDOnly/sherpa?tab=readme-ov-file#installation) sherpa package manager
- From SSH command-prompt, run
```
sudo sherpa install nzbget
```

## Post-install

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
By default, NZBGet downloads all files into the package directory, which is not accessible from shares.

## Extensions

The QNAP package is built with Python 2. To use Python 3 extensions, install the Python 3 package from the QNAP Store and add the Python 3 executable to `EXTENSION SCRIPTS` - `ShellOverride`:
```
.py=/share/CACHEDEV1_DATA/.qpkg/Python3/python3/bin/python3;
```
