For configuring and building:
 - [CMake](https://cmake.org/)
 - [MS C++ Build tools](https://visualstudio.microsoft.com/downloads/?q=build+tools)

To compile the program with TLS/SSL support you need OpenSSL:
   - [OpenSSL](https://www.openssl.org)

Also required are:
   - [Zlib](https://gnuwin32.sourceforge.net/packages/zlib.htm)
   - [libxml2](https://gitlab.gnome.org/GNOME/libxml2/-/wikis/home)
   - [Boost.JSON](https://www.boost.org/doc/libs/1_84_0/libs/json/doc/html/index.html)
   - [Boost.Optional](https://www.boost.org/doc/libs/1_84_0/libs/optional/doc/html/index.html)
For tests:
   - [Boost.Test](https://www.boost.org/doc/libs/1_84_0/libs/test/doc/html/index.html)

We recommend using [vcpkg](https://vcpkg.io/) to install dependencies:

```
  vcpkg install openssl:<x64|x86>-windows-static
  vcpkg install libxml2:<x64|x86>-windows-static
  vcpkg install zlib:<x64|x86>-windows-static
  vcpkg install boost-json:<x64|x86>-windows-static
  vcpkg install boost-optional:<x64|x86>-windows-static
```
  - For tests:
```
  vcpkg install boost-test:<x64|x86>-windows-static
```
  - Configure:
``` 
  mkdir build
  cd build
  cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -A x64
```
  - For Win32:
```
  cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x86-windows-static -A Win32
```
  - For debug build:
```
  cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Debug 
```
  - If Debug:
```
  cmake --build . --config Debug
```

You may run configure with additional arguments:
  - Enable tests:
```
  cmake .. -DENABLE_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
```
  - Disable TLS. Use this option if you can not use OpenSSL.
```
  cmake .. -DDISABLE_TLS=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
```
