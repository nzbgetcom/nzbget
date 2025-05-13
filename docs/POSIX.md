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

  - For gzip support in web-server and web-client (enabled by default):
    - [zlib](https://www.zlib.net/)
  
  - For tests:
    - [Boost.Test](https://www.boost.org/doc/libs/1_84_0/libs/test/doc/html/index.html)

  - For static code analysis:
    - [Clang-Tidy](https://clang.llvm.org/extra/clang-tidy/)
    - [Cppcheck](https://cppcheck.sourceforge.io/)

Please note that you also 
need the developer packages for these libraries too, they package names 
have often suffix "dev" or "devel". On other systems you may need to 
download the libraries at the given URLs and compile them (see hints below).

### Debian:  
```bash
apt install cmake build-essential libncurses-dev libssl-dev libxml2-dev zlib1g-dev
```
  - Debian 12 (bookworm)
```bash
apt install libboost-json1.81-dev
apt install libboost-test1.81-dev #(optional: for testing)
```
  - Debian 13 (trixie)
```bash
apt install libboost-json-dev 
apt install libboost-test-dev #(optional: for testing)
```
  - For static code analysis:
```bash
apt install clang-tidy
```
### FreeBSD: 
```bash
pkg install cmake ncurses openssl libxml2 zlib boost-libs
```
### macOS:
```bash
xcode-select --install
brew install cmake ncurses openssl libxml2 zlib boost
```

## 4. Installation on POSIX

Installation from the source distribution archive (nzbget-VERSION.tar.gz):

  - Untar the nzbget-source:
```bash
tar -zxf nzbget-VERSION.tar.gz
```
  - Change into nzbget-directory:
```bash
cd nzbget-VERSION
```
  - Configure:
``` bash
mkdir build
cd build
cmake ..
```
  - In a case you don't have root access or want to install the program
    in your home directory use the configure parameter -DCMAKE_INSTALL_PREFIX:
```bash
cmake .. -DCMAKE_INSTALL_PREFIX=~/usr
```
  - Build, specifying (-j 8) how many CPU cores to use to speed up compilation:
```bash
cmake --build . -j 8 
```
  - Install:
```bash
cmake --install .
```
  - Uninstall:
```bash
cmake --build . --target uninstall
```
  - Install configuration files into <prefix>/etc via:
```bash
cmake --build . --target install-conf
```
  - Uninstall configuration files into <prefix>/etc via:
```bash
cmake --build . --target uninstall-conf
```
  - Run tests on POSIX:
```bash
ctest
```

### Configure-options
---------------------
You may run configure with additional arguments:
  - Enable tests:
```bash
cmake .. -DENABLE_TESTS=ON
```
  - Enable Clang-Tidy static code analyzer:
```bash
cmake .. -DENABLE_CLANG_TIDY=ON
```
  - Disable ncurses. Use this option if you cannot use ncurses:
```bash
cmake .. -DDISABLE_CURSES=ON
```
  - Disable parcheck. Use this option if you have troubles when compiling par2-module:
```bash
cmake .. -DDISABLE_PARCHECK=ON
```
  - Disable TLS. Use this option if you cannot use OpenSSL:
```bash
cmake .. -DDISABLE_TLS=ON
```
  - Disable gzip. Use this option if you cannot use zlib:
```bash
cmake .. -DDISABLE_GZIP=ON
``` 
  - Disable sigchld-handler. The disabling may be necessary on 32-Bit BSD:
```bash
cmake .. -DDISABLE_SIGCHLD_HANDLER=ON
``` 
  - For debug build:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
``` 
  - Enable leak, undefined, address sanitizers:
```
cmake .. -DENABLE_SANITIZERS=ON
```
  - To get a static binary:
```bash
cmake .. -DENABLE_STATIC=ON
```
`LIBS` and `INCLUDES` env variables can be useful for static linking, since CMake looks for shared libraries by default:
```
export LIBS="-lncurses -ltinfo -lboost_json -lxml2 -lz -lm -lssl -lcrypto -Wl,--whole-archive -lpthread -Wl,--no-whole-archive"
export INCLUDES="/usr/include/;/usr/include/libxml2/"
cmake .. -DENABLE_STATIC=ON
```
## Cppcheck
  - Install Cppcheck:
```bash
apt install cppcheck
```
  - Generate a compile database:
```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
```
  - The file compile_commands.json is created in the current folder. Now run Cppcheck like this:
```bash
cppcheck --project=compile_commands.json
```
 - To ignore certain folders you can use -i. This will skip analysis of source files in
the foo folder:
```bash
cppcheck --project=compile_commands.json -ifoo
```
