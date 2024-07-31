## To build NZBGet you will need:

  - For configuring and building:
    - [CMake](https://cmake.org/)
    - [GCC](https://gcc.gnu.org/)

      or
    - [CLang](https://clang.llvm.org/)

  - Libraries:
    - [libxml2](https://gitlab.gnome.org/GNOME/libxml2/-/wikis/home)
    - [Boost.JSON](https://www.boost.org/doc/libs/1_84_0/libs/json/doc/html/index.html)
    - [Boost.Asio](https://www.boost.org/doc/libs/1_85_0/doc/html/boost_asio.html)
    
> If you face issues with Boost.JSON on your system, you can skip it - CMake will take care of it.

- And the following libraries are optional:

    - For curses-output-mode (enabled by default):
      - [ncurses](https://invisible-island.net/ncurses)
    
  - For encrypted connections (TLS/SSL):
    - [OpenSSL](https://www.openssl.org)

      or
    - [GnuTLS](https://gnutls.org)

  - For gzip support in web-server and web-client (enabled by default):
    - [zlib](https://www.zlib.net/)
  
  - For tests:
    - [Boost.Test](https://www.boost.org/doc/libs/1_84_0/libs/test/doc/html/index.html)

  - For static code analysis:
    - [Clang-Tidy](https://clang.llvm.org/extra/clang-tidy/)

Please note that you also 
need the developer packages for these libraries too, they package names 
have often suffix "dev" or "devel". On other systems you may need to 
download the libraries at the given URLs and compile them (see hints below).

### Debian:  
```
apt install cmake build-essential libncurses-dev libssl-dev libxml2-dev zlib1g-dev libboost-json1.81-dev
```
  - For tests:
```
apt install libboost-test1.81-dev
```
  - For static code analysis:
```
apt install clang-tidy
```
### FreeBSD: 
```
pkg install cmake ncurses openssl libxml2 zlib boost-libs
```
### macOS:
```
xcode-select --install
brew install cmake ncurses openssl libxml2 zlib boost
```

## 4. Installation on POSIX

Installation from the source distribution archive (nzbget-VERSION.tar.gz):

  - Untar the nzbget-source:
```
tar -zxf nzbget-VERSION.tar.gz
```
  - Change into nzbget-directory:
```
cd nzbget-VERSION
```
  - Configure:
``` 
mkdir build
cd build
cmake ..
```
  - In a case you don't have root access or want to install the program
    in your home directory use the configure parameter -DCMAKE_INSTALL_PREFIX:
```
cmake .. -DCMAKE_INSTALL_PREFIX=~/usr
```
  - Build, specifying (-j 8) how many CPU cores to use to speed up compilation:
```
cmake --build . -j 8 
```
  - Install:
```
cmake --install .
```
  - Uninstall:
```
cmake --build . --target uninstall
```

### Configure-options
---------------------
You may run configure with additional arguments:
  - Enable tests:
```
cmake .. -DENABLE_TESTS=ON
```
  - Enable Clang-Tidy static code analizer:
```
cmake .. -DENABLE_CLANG_TIDY=ON
```
  - Disable ncurses. Use this option if you can not use ncurses.
```
cmake .. -DDISABLE_CURSES=ON
```
  - Disable parcheck. Use this option if you have troubles when compiling par2-module.
```
cmake .. -DDISABLE_PARCHECK=ON
```
  - Use GnuTLS. Use this option if you want to use GnuTLS instead of OpenSSL.
```
cmake .. -DUSE_GNUTLS=ON
```
  - Disable TLS. Use this option if you can not neither OpenSSL nor GnuTLS.
```
cmake .. -DDISABLE_TLS=ON
```
  - Disable gzip. Use this option if you can not use zlib.
```
cmake .. -DDISABLE_GZIP=ON
``` 
  - Disable sigchld-handler. The disabling may be neccessary on 32-Bit BSD.
```
cmake .. -DDISABLE_SIGCHLD_HANDLER=ON
``` 
  - For debug build.
```
cmake .. -DCMAKE_BUILD_TYPE=Debug
```
  - To get a static binary, 
```
cmake .. -DENABLE_STATIC=ON
```
`LIBS` and `INCLUDES` env variables can be useful for static linking, since CMake looks for shared libraries by default
```
export LIBS="-lncurses -ltinfo -lboost_json -lxml2 -lz -lm -lssl -lcrypto -Wl,--whole-archive -lpthread -Wl,--no-whole-archive"
export INCLUDES="/usr/include/;/usr/include/libxml2/"
cmake .. -DENABLE_STATIC=ON
```
## Cppcheck
  - Install Cppcheck
```
apt install cppcheck
```
  - Generate a compile database:
```
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
```
  - The file compile_commands.json is created in the current folder. Now run Cppcheck like this:
```
cppcheck --project=compile_commands.json
```
 - To ignore certain folders you can use -i. This will skip analysis of source files in
the foo folder.
```
cppcheck --project=compile_commands.json -ifoo
```

## Building using autotools (deprecated)

  - configure it via
```
autoreconf --install./configure
```
   (maybe you have to tell configure, where to find some libraries then is your friend!
```
./configure --help
```
  also see "Configure-options" later)

  - in a case you don't have root access or want to install the program
   in your home directory use the configure parameter --prefix, e. g.:
```
./configure --prefix ~/usr
```
  - compile it via
```
make -j 4
```
  - to install system wide become root via:
```
su
```
 - install it via:
```
make install
```
 - install configuration files into <prefix>/etc via:
```
make install-conf
```
(you can skip this step if you intend to store configuration
    files in a non-standard location)

### Configure-options
---------------------
You may run configure with additional arguments:

  - --disable-curses - to make without curses-support. Use this option
    if you can not use curses/ncurses.

  - --disable-parcheck - to make without parcheck-support. Use this option
    if you have troubles when compiling par2-module.

  - --with-tlslib=(OpenSSL, GnuTLS) - to select which TLS/SSL library
    should be used for encrypted server connections.

  - --disable-tls - to make without TLS/SSL support. Use this option if
    you can not neither OpenSSL nor GnuTLS.

  - --disable-gzip - to make without gzip support. Use this option
    if you can not use zlib.

  - --enable-debug - to build in debug-mode, if you want to see and log
    debug-messages.

### Optional package: par-check
-------------------------------
NZBGet can check and repair downloaded files for you. For this purpose
it uses library par2.

For your convenience the source code of libpar2 is integrated into
NZBGet’s source tree and is compiled automatically when you make NZBGet.

In a case errors occur during this process the inclusion of par2-module
can be disabled using configure option "--disable-parcheck":
```
./configure --disable-parcheck
```

### Optional package: curses
----------------------------
For curses-outputmode you need ncurses or curses on your system.
If you do not have one of them you can download and compile ncurses yourself.
Following configure-parameters may be useful:

  - --with-libcurses-includes=/path/to/curses/includes
  - --with-libcurses-libraries=/path/to/curses/libraries

If you are not able to use curses or ncurses or do not want them you can
make the program without support for curses using option "--disable-curses":
```
./configure --disable-curses
```

### Optional package: TLS
-------------------------
To enable encrypted server connections (TLS/SSL) you need to build the program
with TLS/SSL support. NZBGet can use two libraries: OpenSSL or GnuTLS.
Configure-script checks which library is installed and use it. If both are
available it gives the precedence to OpenSSL. You may override that with
the option --with-tlslib=(OpenSSL, GnuTLS). For example to build with GnuTLS:
```
./configure --with-tlslib= GnuTLS
```

Following configure-parameters may be useful:

  - --with-libtls-includess=/path/to/gnutls/includes
  - --with-libtls-libraries=/path/to/gnutls/libraries

  - --with-openssl-includess=/path/to/openssl/includes
  - --with-openssl-libraries=/path/to/openssl/libraries

If none of these libraries is available you can make the program without 
TLS/SSL support using option "--disable-tls", but 
  some features of nzbget will stop working, such as Extension Manager:
```
./configure --disable-tls
```
## Known issues:
- does not compile on `armhf` hosts due to `yencode` library optimizations for vfp fpu hosts;
