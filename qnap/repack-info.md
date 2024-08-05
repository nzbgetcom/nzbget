# About

`repack-nzbget.sh` is a bash script which is used to repack nzbget linux installer to QNAP packages.


# Prerequisites

- linux x86_64 host (Ubuntu 22.04 LTS for example)
- installed QDK from `https://github.com/qnap-dev/QDK`
```
git clone https://github.com/qnap-dev/QDK
cd QDK
sed 's|python|python3|' -i InstallToUbuntu.sh
sudo ./InstallToUbuntu.sh install
```
- nzbget linux installer (`nzbget-${VERSION}-bin-linux.run`)

# Building NZBGet QNAP packages

From cloned repository run
```
bash qnap/repack-nzbget.sh <path_to_installer>
```

## Output files

- /qnap/nzbget/build/*.qpkg - one file per platform
