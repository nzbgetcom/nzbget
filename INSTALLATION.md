# NZBGet installation

This is a short documentation. For more information please
visit NZBGet home page at 
  https://nzbget.com

Contents          
--------
1. About NZBGet
2. Supported OS
3. Prerequisites on POSIX
4. Installation on POSIX
5. Compiling on Windows
6. Configuration
7. Usage
8. Authors
9. Copyright
10. Contact


## 1. About NZBGet

NZBGet is a binary newsgrabber, which downloads files from usenet
based on information given in nzb-files. NZBGet can be used in
standalone and in server/client modes. In standalone mode you 
pass a nzb-file as parameter in command-line, NZBGet downloads
listed files and then exits. 

In server/client mode NZBGet runs as server in background.
Then you use client to send requests to server. The sample requests
are: download nzb-file, list files in queue, etc.

There is also a built-in web-interface. The server has RPC-support
and can be controlled from third party applications too.

Standalone-tool, server and client are all contained in only one 
executable file "nzbget". The mode in which the program works 
depends on command-line parameters passed to the program.

## 2. Supported OS

NZBGet is written in C++ and works on Windows, OS X, Linux and
most POSIX-conform OS'es.

Clients and servers running on different OS'es may communicate with
each other. For example, you can use NZBGet as client on Windows to 
control your NZBGet-server running on Linux.

The download-section of NZBGet web-site provides binary files
for Windows, OS X and Linux. For most POSIX-systems you need to compile
the program yourself. 

If you have downloaded binaries you can just jump to section 
"Configuration".

## 3. Prerequisites on POSIX

NZBGet is developed on a linux-system, but it runs on other
POSIX platforms.

To build NZBGet you will need:

For configuring and building:
 - [CMake](https://cmake.org/)
 - [GCC](https://gcc.gnu.org/)

   or
 - [CLang](https://clang.llvm.org/)

Libraries:
 - [libxml2](https://gitlab.gnome.org/GNOME/libxml2/-/wikis/home)
 - [Boost.JSON](https://www.boost.org/doc/libs/1_84_0/libs/json/doc/html/index.html)
> If you can't install Boost.Json on your system, jsut skip it. CMake will take care of it.

And the following libraries are optional:

  For curses-output-mode (enabled by default):
   - [ncurses](https://invisible-island.net/ncurses)
    
  For encrypted connections (TLS/SSL):
   - [OpenSSL](https://www.openssl.org)

     or
   - [GnuTLS](https://gnutls.org)

  For gzip support in web-server and web-client (enabled by default):
   - [zlib](https://www.zlib.net/)
  
  For tests:
   - [Boost.Test](https://www.boost.org/doc/libs/1_84_0/libs/test/doc/html/index.html)

Please note that you also 
need the developer packages for these libraries too, they package names 
have often suffix "dev" or "devel". On other systems you may need to 
download the libraries at the given URLs and compile them (see hints below).

### Debian:  
```
  apt install cmake build-essential libncurses-dev libssl-dev libxml2-dev zlib1g-dev libboost-json1.81-dev libboost-test1.81-dev
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


## 5. Compiling on Windows

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
  - Disable TLS. Use this option if you can not neither OpenSSL nor GnuTLS.
```
  cmake .. -DDISABLE_TLS=ON -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

## 6. Configuration

NZBGet needs a configuration file. 

An example configuration file is provided in "nzbget.conf", which
is installed into "<prefix>/share/nzbget" (where <prefix> depends on
system configuration and configure options - typically "/usr/local",
"/usr" or "/opt"). The installer adjusts the file according to your
system paths. If you have performed the installation step 
"make install-conf" this file is already copied to "<prefix>/etc" and
NZBGet finds it automatically. If you install the program manually
from a binary archive you have to copy the file from "<prefix>/share/nzbget"
to one of the locations listed below.

Open the file in a text editor and modify it accodring to your needs.

You need to set at least the option "MAINDIR" and one news server in
configuration file. The file has comments on how to use each option.

The program looks for configuration file in following standard 
locations (in this order):

On POSIX systems:
  <EXE-DIR>/nzbget.conf
  ~/.nzbget
  /etc/nzbget.conf
  /usr/etc/nzbget.conf
  /usr/local/etc/nzbget.conf
  /opt/etc/nzbget.conf
  
On Windows:
  <EXE-DIR>\nzbget.conf

If you put the configuration file in other place, you can use command-
line switch "-c <filename>" to point the program to correct location.

In special cases you can run program without configuration file using
switch "-n". You need to use switch "-o" to pass required configuration 
options via command-line.

## 7. Usage

NZBGet can be used in either standalone mode which downloads a single file 
or as a server which is able to queue up numerous download requests.

TIP for Windows users: NZBGet is controlled via various command line
parameters. For easier using there is a simple shell script included
in "nzbget-shell.bat". Start this script from Windows Explorer and you will
be running a command shell with PATH adjusted to find NZBGet executable.
Then you can type all commands without full path to nzbget.exe.

### Standalone mode:
--------------------

nzbget <nzb-file>

### Server mode:
----------------

First start the nzbget-server:

  - in console mode:

	nzbget -s
	
  - or in daemon mode (POSIX only):

	nzbget -D
	
	- or as a service (Windowx only, firstly install the service with command
	  "nzbget -install"):

  net start NZBGet
  
To stop server use:

  nzbget -Q  

TIP for POSIX users: with included script "nzbgetd" you can use standard
commands to control daemon:

  nzbgetd start
  nzbgetd stop
  etc.

When NZBGet is started in console server mode it displays a message that
it is ready to receive download requests. In daemon mode it doesn't print any
messages to console since it runs in background.

When the server is running it is possible to queue up downloads. This can be
done either in terminal with "nzbget -A <nzb-file>" or by uploading
a nzb-file into server's monitor-directory (<MAINDIR>/nzb by default).

To check the status of server start client and connect it to server:
  
  nzbget -C

The client have three different (display) outputmodes, which you can select
in configuration file (on client computer) or in command line. Try them:

  nzbget -o outputmode=log -C

  nzbget -o outputmode=color -C

  nzbget -o outputmode=curses -C

To list files in server's queue:

  nzbget -L
  
It prints something like:

  [1] nzbname\filename1.rar (50.00 MB)
  [2] nzbname\filename1.r01 (50.00 MB)
  [3] another-nzb\filename3.r01 (100.00 MB)
  [4] another-nzb\filename3.r02 (100.00 MB)

This is the list of individual files listed within nzb-file. To print
the list of nzb-files (without content) add G-modifier to the list command:

  [1] nzbname (4.56 GB)
  [2] another-nzb (4.20 GB)

The numbers in square braces are ID's of files or groups in queue.
They can be used in edit-command. For example to move file with
ID 2 to the top of queue:

  nzbget -E T 2
  
or to pause files with IDs from 10 to 20:

  nzbget -E P 10-20

or to delete files from queue:

  nzbget -E D 3 10-15 20-21 16

The edit-command has also a group-mode which affects all files from the
same nzb-file. You need to pass an ID of the group. For example to delete
the whole group 1:

  nzbget -E G D 1

The switch "o" is useful to override options in configuration files. 
For example:

  nzbget -o reloadqueue=no -o dupecheck=no -o parcheck=yes -s
  
or:

  nzbget -o createlog=no -C
  
### Running client & server on seperate machines:
-------------------------------------------------

Since nzbget communicates via TCP/IP it's possible to have a server running on
one computer and adding downloads via a client on another computer.

Do this by setting the "ControlIP" option in the nzbget.conf file to point to the
IP of the server (default is localhost which means client and server runs on 
same computer)

### Security warning
--------------------

NZBGet communicates via unsecured socket connections. This makes it vulnerable.
Although server checks the password passed by client, this password is still 
transmitted in unsecured way. For this reason it is highly recommended 
to configure your Firewall to not expose the port used by NZBGet to WAN. 

If you need to control server from WAN it is better to connect to server's
terminal via SSH (POSIX) or remote desktop (Windows) and then run
nzbget-client-commands in this terminal.

### Post processing scripts
---------------------------

After the download of nzb-file is completed nzbget can call post-processing
scripts, defined in configuration file.

Example post-processing scripts are provided in directory "scripts".

To use the scripts copy them into your local directory and set options
<ScriptDir>, <PostScript> and <ScriptOrder>.

For information on writing your own post-processing scripts please
visit NZBGet web site.

### Web-interface
-----------------

NZBGet has a built-in web-server providing the access to the program
functions via web-interface.

To activate web-interface set the option "WebDir" to the path with
web-interface files. If you install using "make install-conf" as
described above the option is set automatically. If you install using
binary files you should check if the option is set correctly.

To access web-interface from your web-browser use the server address
and port defined in NZBGet configuration file in options "ControlIP" and
"ControlPort". For example:

  http://localhost:6789/

For login credentials type username and the password defined by
options "ControlUsername" (default "nzbget") and "ControlPassword"
(default "tegbzn6789").

In a case your browser forget credentials, to prevent typing them each
time, there is a workaround - use URL in the form:

  http://localhost:6789/username:password/

Please note, that in this case the password is saved in a bookmark or in
browser history in plain text and is easy to find by persons having
access to your computer. 

## 8. Authors

NZBGet is developed and maintained by Andrey Prygunkov
(hugbug@users.sourceforge.net).

The original project was initially created by Sven Henkel
(sidddy@users.sourceforge.net) in 2004 and later developed by
Bo Cordes Petersen (placebodk@users.sourceforge.net) until 2005.
In 2007 the abandoned project was overtaken by Andrey Prygunkov.
Since then the program has been completely rewritten.

NZBGet distribution archive includes additional components
written by other authors:

Par2:
	Peter Brian Clements <peterbclements@users.sourceforge.net>

Par2 library API:
	Francois Lesueur <flesueur@users.sourceforge.net>

jQuery:
	John Resig <https://jquery.com>
	The Dojo Foundation <https://github.com/jquery/sizzle/wiki>

Bootstrap:
	Twitter, Inc <https://getbootstrap.com>

Raphaël:
	Dmitry Baranovskiy <http://raphaeljs.com>
	Sencha Labs <http://sencha.com>

Elycharts:
	Void Labs s.n.c. <http://void.it>

iconSweets:
	Yummygum <https://yummygum.com/>

Boost:
  Boost organization and wider Boost community <https://www.boost.org>


## 9. Copyright

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

The complete content of license is provided in file COPYING.

Additional exemption: compiling, linking, and/or using OpenSSL is allowed.

## 10. Contact

If you encounter any problem, feel free to contact us
	https://nzbget.com/contact/
