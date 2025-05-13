## To build NZBGet you will need:

 - [CMake](https://cmake.org/)
 - [MS C++ Build tools](https://visualstudio.microsoft.com/downloads/?q=build+tools#build-tools-for-visual-studio-2022)
   - Download `Build Tools for Visual Studio 2022` and install it
   - Select `Desktop development with C++` in the `Desktop & Mobile` section and install the necessary components:
     - MSVC v143 - VS 2022 C++ x64/x86 build tools
     - Windows 11 SDK
     - C++ ATL for latest v143 build tools
     - C++ MFC for latest v143 build tools
     -  Edit the `Path` enviroment variable and append the folder's path that contains the `MSBuild.exe` to it, e.g.:

        `C:\Users\asus\AppData\Local\Programs\Microsoft VS Code\bin\`

To compile the program with TLS/SSL support you need OpenSSL:
   - [OpenSSL](https://www.openssl.org)

Also required are:
   - [Zlib](https://gnuwin32.sourceforge.net/packages/zlib.htm)
   - [libxml2](https://gitlab.gnome.org/GNOME/libxml2/-/wikis/home)
   - [Boost.JSON](https://www.boost.org/doc/libs/1_84_0/libs/json/doc/html/index.html)
   - [Boost.Asio](https://www.boost.org/doc/libs/1_85_0/doc/html/boost_asio.html)

For tests:
   - [Boost.Test](https://www.boost.org/doc/libs/1_84_0/libs/test/doc/html/index.html)

We recommend using [vcpkg](https://vcpkg.io/) to install dependencies:
 - Clone the repository to the recommended `C:\` disk:
```powershell
git clone --depth 1 https://github.com/microsoft/vcpkg.git
```
 - Run the `bootstrap` script:
```powershell
.\vcpkg\bootstrap-vcpkg.bat
```
 - Edit the `Path` enviroment variable and append the folder's path: `C:\vcpkg`
 - Install all the dependencies:
```powershell
vcpkg install openssl:x64-windows-static
vcpkg install libxml2:x64-windows-static
vcpkg install zlib:x64-windows-static
vcpkg install boost-json:x64-windows-static
vcpkg install boost-asio:x64-windows-static
```
  - For tests:
```powershell
vcpkg install boost-test:x64-windows-static
```

For `Win32`, instead of `:x64-windows-static`, use `:x86-windows-static`.

  - Configure:
```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -A x64
```
  - Or for Win32:
```powershell
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x86-windows-static -A Win32
```
  - Release build:
```powershell
cmake --build . --config Release
```
  - Or for debug build:
```powershell
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Debug 
```
  - Debug build:
```powershell
cmake --build . --config Debug
```
  - Now, you can find the binary in the `Release/Debug` directory.


You may run configure with additional arguments:
  - Disable TLS. Use this option if you cannot use OpenSSL:

```powershell
cmake .. -DDISABLE_TLS=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

  - Enable tests:
```powershell
cmake .. -DENABLE_TESTS=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
```
  - Run tests:
```powershell
ctest -C Release
```
 - Or for debug build:
```powershell
ctest -C Debug
```