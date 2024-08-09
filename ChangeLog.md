nzbget-24.2
  - Features:
    - System info tab and Server Speed Tests
    [#303](https://github.com/nzbgetcom/nzbget/commit/c5dce755226f359ca106bb70615d7d999a060660);
      * new API-methods:
        - struct sysinfo() - returns information about the user's environment and hardware
        - bool testserverspeed(url, serverId) - puts nzb file to be downloaded by the target server
      * API-method "status" now has 3 extra fields:
        - TotalDiskSpaceLo - Total disk space on ‘DestDir’, in bytes. This field contains the low 32-bits of 64-bit value
        - TotalDiskSpaceHi - Total disk space on ‘DestDir’, in bytes. This field contains the high 32-bits of 64-bit value
        - TotalDiskSpaceMB - Total disk space on ‘DestDir’, in megabytes.`
      * fixed NZB generator: the last segment was incorrect
      * added Boost.Asio - cross-platform library for network
    - Multithreading Improvements
    [#282](https://github.com/nzbgetcom/nzbget/commit/a98e6d11cf5e862e3bfc4819b26492348c4506be)
      * noticeable improvements in download speed - it became more stable
      * the download speed dropping to 0 MB/s has gone away
      * the application became more stable, resulting in fewer crashe
    - Android support
    [#325](https://github.com/nzbgetcom/nzbget/commit/7f8360c03eb26f2dda62980b76db39fd6d3c67ab)
    - Read nzbpassword from filename
    [#310](https://github.com/nzbgetcom/nzbget/commit/cf1eb073b1640f2ba351538918cfb69e2625136a)

  - Bug fixes:
    - Fixed speed value overflows if the download speed is greater than 2 GB/s
    [#314](https://github.com/nzbgetcom/nzbget/commit/6d6d973c5d53f72cfd05f40a7feb8f291d881432)
      * the Status xml-rpc response now has 4 extra fields:
        - DownloadRateLo - Download rate in bytes. This field contains the low 32-bits of 64-bit value
        - DownloadRateHi - Download rate in bytes. This field contains the high 32-bits of 64-bit value
        - AverageDownloadRateLo - Average download rate in bytes. This field contains the low 32-bits of 64-bit value
        - AverageDownloadRateHi - Average download rate in bytes. This field contains the high 32-bits of 64-bit value
      * DownloadRate and AverageDownloadRate fields are deprecated now
    - Fixed potential int32 overflow issues
    [#321](https://github.com/nzbgetcom/nzbget/commit/5c00f580a0690f7a3410059b711c6c89a86b1d6a)

    - QNAP:
        * added shutdown delay check for daemon process
        [#281](https://github.com/nzbgetcom/nzbget/commit/cb88ac9c94cfb98c6705e9403f9d8d462885659e)
        * fixed overwritting existing config file when upgrading QPKG
        [#285](https://github.com/nzbgetcom/nzbget/commit/c200293473e87d505b76b9e669fa5d0c368552a1)
        * improved package icons
        [#287](https://github.com/nzbgetcom/nzbget/commit/68ddad570786e7de15cef2cfc1dc425e9e258711)

  - For developers:
    - Complete migration to CMake
    [#301](https://github.com/nzbgetcom/nzbget/commit/515cd1081f3bee32913c48436c16e845346309db):
      * removed QNAP native packages build scripts and workflow
      * removed Synology build scripts / package sources / workflow
      * POSIX: removed autotools related files
      * WINDOWS: removed Visual Studio project file
      * removed obsoleted build scripts and documentation
      * updated CMakeLists.txt with C++17 config and cross-build support for macOS builds
    - Removed the use of Boost.Variant and Boost.Optional since they are no longer relevant after moving to C++17 and GCC 9+/Clang 8+
    [#317](https://github.com/nzbgetcom/nzbget/commit/a7ac9a9b061c140f7f688840be633ca8b780face)
    - Added missing HAVE_ALLOCA_H definition for regex and GCC 14+
    [#308](https://github.com/nzbgetcom/nzbget/commit/412d9e5b732c1cf39aac266dcf97bf097f85bc58)
    - Dockerfile optimized for local repo builds
    [#305](https://github.com/nzbgetcom/nzbget/commit/6fdc4fb90f910469c5568871e16d6437f5a9f395)
    - GnuTLS is deprecated now and will be removed in future releases

nzbget-24.1
  - Bug fixes:
      - Fixed: don't override user preference in unrar
      [#251](https://github.com/nzbgetcom/nzbget/commit/483702814e4d3b0c950e1ad2a48471e6c99bf624);
      - Fixed (Docker): unable to unpack rar files
      [#256](https://github.com/nzbgetcom/nzbget/commit/62aa1d900a9e9d2051301ef27beb672c4ce1e4b5);
      - Fixed: possibility to use `0` for umask
      [#264](https://github.com/nzbgetcom/nzbget/commit/f87a24b6f0d83608e15662967e76b96850537199);
      - Fixed: `fseeko` not found
      [#262](https://github.com/nzbgetcom/nzbget/commit/b9e51d482bd0e54bf917d0497c0736bfd12fefbf);
      - Fixed: icons in webui in old browser versions
      [#268](https://github.com/nzbgetcom/nzbget/commit/03916079949cebfb780d2a23c8667db31f57fae9).

nzbget-24.0
  - Features:
    - Dark theme and new icons 
    [#214](https://github.com/nzbgetcom/nzbget/commit/16ecaa5c3eb2efc43e965b796a4c75cdf1d959da);
    - Added macOS x64 build support (macOS Mojave 10.14+) 
    [#194](https://github.com/nzbgetcom/nzbget/commit/90a89f8fb3b81432e192203e8a66419cfd3cd723);
    - DEB/RPM packages support 
    [#230](https://github.com/nzbgetcom/nzbget/commit/1de712a428c031513431d0d75db7ca5ac51d3caa);
      * [Instructions](https://nzbgetcom.github.io/).
    - NewsServer Add UI - Default encryption and ports 
    [#225](https://github.com/nzbgetcom/nzbget/commit/cd1cab44b6052c3b02f14689fbafb65505f67a4e):
      * moved Server.Encryption between Server.Host and Server.Port;
      * made Server.Encryption to ON by default;
      * made port depend on Server.Encryption value unless user has put something up:
        - 563/443 for secure;
        - 119/80 for unsecure.
    - Improved error messages and help text in Extension Manager 
    [#166](https://github.com/nzbgetcom/nzbget/commit/d11ed84912fc486e6f717e56ccd973526920c0db):
      * added 7-Zip exit codes decoder according to 7-Zip 
      [doc](https://documentation.help/7-Zip/exit_codes.htm);
      * added a warning that SevenZipCmd may not be valid, in case of extension installation problems.
    - Fixed stable/test release notifications 
    [#181](https://github.com/nzbgetcom/nzbget/commit/1c03b719f88ece7d67ad348f3aca0d3800ab4d54):
      * added automatic checking for new testing releases;
      * fixed notifications about the new stable/testing release;
      * fixed "Uncaught ReferenceError: installedRev" error.

    - Fixed and update links in webui 
    [#177](https://github.com/nzbgetcom/nzbget/commit/0cc6023bfdf7a1d22311dea7d81b8a6ea7a7d880):
      * fixed license links;
      * fixed links from nzbget.conf.

  - For developers:
    - Moved to CMake. Autotools and MSBuild are deprecated now and may be removed in future versions
    [#182](https://github.com/nzbgetcom/nzbget/commit/56e4225fc73a6d1c7cdc6f647a4cac297e28e9f3):
      * switched to CMake from autotools and MSBuild, which will simplify cross-platform development;
      * fixed installing/uninstalling on FreeBSD and macOS via autotools/CMake;
      * added automatic installation of Boost.Json;
      * added support for static code analyzer Clang-Tidy.
    - Revised and updated documentation 
    [#199](https://github.com/nzbgetcom/nzbget/commit/c93b551b1fa0afae458c1e78485c6d3eb6410514):
      * moved CONTRIBUTING.md to docs/;
      * split INSTALLATION.md to POSIX.md, WINDOWS.md and HOW_TO_USE.md;
      * added Extensions docs;
      * added an instruction on how CPPCheck can be used with CMake.
    - Docker: support native unrar building
    [#231](https://github.com/nzbgetcom/nzbget/commit/cc6fb8b21a0332aac81b94e466c909e06743235b).

  - Bug fixes:
    - Fixed buffer overflow in CString::AppendFmtV 
    [#208](https://github.com/nzbgetcom/nzbget/commit/c8c59277cfcf085596e124209a3b07e56ada00dc);
    - Fixed files/dirs permissions overridden by the unpacker
    [#229](https://github.com/nzbgetcom/nzbget/commit/4c92e423fe54a9194b84a9dcd7942865a7df72ec);
    - Fixed dynamic addition of extension options 
    [#209](https://github.com/nzbgetcom/nzbget/commit/d49dc6dc426f71d87626820afd45348643a41458);
    - Removed changing ownership at the beginning of container start 
    [#218](https://github.com/nzbgetcom/nzbget/commit/36dad2f793a5503871f39eca3d7dbdfa945824ee);
    - Docker: fixed fresh install 
    [#219](https://github.com/nzbgetcom/nzbget/commit/a95269e8e15f42196a1314603fb802f3bedd4fa8).

nzbget-23.0
  - Features:
    - Extension Manager 
    [#76](https://github.com/nzbgetcom/nzbget/commit/7348b19cf8a81739922303e3f9047d06e59670d0);
        * The new nzbget extension system, which makes it easy to download/update/delete 
        extensions with backward compatibility with the old system;
        * extensions 
        [master list](https://github.com/nzbgetcom/nzbget-extensions/);
        * changed:
          -  RPC request "configtemplates" -  no longer returns script templates, but only the config template;
        * added:
          - new RPC requests:
            - "loadextensions" - 
            loads all extensions from {ScriptDIR} and returns an array of structures in JSON/XML formats;
            - "updateextension" - 
            downloads by url and name and installs the extension. Returns 'true' or error response in JSON/XML formats;
            - "deleteextension" - 
            deletes extension by name. Returns 'true' or error response in JSON/XML formats;
            - "downloadextension" - 
            downloads by url and installs the extension. Returns 'true' or error response in JSON/XML formats;
            - "testextension" - 
            tries to find the right executor program for the extension, e.g. Python. 
            Returns 'true' or error response in JSON/XML formats;
          - "EXTENSION MANAGER" section in webui to download/delete/update extensions;
          - Boost.Json library to work with JSON;
          - more unit tests;
        * refactored:
          - replaced raw pointers with smart pointers and const refs where possible for memory safty reasons;
        * removed:
          - testdata_FILES from Makefile.am;
          - EMail and Logger scripts;
    - Docker support 
    [#55](https://github.com/nzbgetcom/nzbget/commit/1913e331bb87cdd592048b37fd57507f82a17144);
    - Synology support (spk) 
    [#72](https://github.com/nzbgetcom/nzbget/commit/3f8fd6d433b38ed9f18ab7ffe25fd7a4a0c5c246);
    - QNAP support
    [#158](https://github.com/nzbgetcom/nzbget/commit/9676c77dc8f9e9498a99a6e774e7bf23ca4be537);
    - aarch64 mipseb mipsel ppc6xx ppc500 architectures to linux build 
    [#61](https://github.com/nzbgetcom/nzbget/commit/959985473cdfcf51d88e6a49770650b4edced043) 
    [#146](https://github.com/nzbgetcom/nzbget/commit/fcc104dc294a905155f65e93483de4a1f40d843e);
    - article read chunk size 
    [#52](https://github.com/nzbgetcom/nzbget/commit/1aa42f2df9a9148d3a79e17694381de873557fce);
      * Added ArticleReadChunkSize config option which allows to adjust 
      the buffer size for customization on different platforms, which can lead to increased performance;
    - increased the number of default connections to 8 
    [#54](https://github.com/nzbgetcom/nzbget/commit/55eca4ce57cc119b51acc2e6915f6c0547dd8123);
    - automatic search for a suitable interpreter on POSIX 
    [#74](https://github.com/nzbgetcom/nzbget/commit/ec0756d5f63137d879f56a6bc60cab6b81238eaf);
    - certificate verification levels 
    [#150](https://github.com/nzbgetcom/nzbget/commit/5956a17e2618480dbbc423e6a515313165f817db);
        * levels:
          - None: NO certificate signing check, NO certificate hostname check;
          - Minimal: certificate signing check, NO certificate hostname check;
          - Strict: certificate signing check, certificate hostname check;
        * tested on a mock nzbget NNTP nserv server with self-signed certificate and got expected results:
          - "Strict" -> test failed;
          - "Minimal" -> test failed;
          - "None" -> test passed";
  - Bug fixes:
    - possible memory corruption 
    [#148](https://github.com/nzbgetcom/nzbget/commit/16ab25d5780c8100309c44eb60d505afe5c528db);
  - For developers:
    - fixed unit tests (Windows only for now) and started migration to CMake 
    [#64](https://github.com/nzbgetcom/nzbget/commit/73e8c00d29f21a44de8fd3d979e4f74628d99e1e);
      * We are going to completely migrate to CMake as a more universal one for cross-platform development and drop autotools and MSBuild;
    - using libxml2 on Windows and vcpkg package manager to install dependencies 
    [#70](https://github.com/nzbgetcom/nzbget/commit/f8cb2bda415fb6167e8aa7e9d596bb583dc0b1c1);
      * libxml2 library is now used on Windows to work with xml in the same way we already do on Linux and macOS;
      * removed platform-dependent code for working with xml on Windows;

nzbget-22.0
 - Add NZBGet version to Windows registry during installation so version displays in programm list.
	https://github.com/nzbgetcom/nzbget/pull/14	
 - Enabled FIPS servers using OpenSSL 3. 
	https://github.com/nzbgetcom/nzbget/pull/3	
 - Change progress bar width to calculated instead of fixed.
	https://github.com/nzbgetcom/nzbget/pull/17	
 - Fix Download time with empty minutes and seconds in email sended via EMail.py
	https://github.com/nzbgetcom/nzbget/pull/16
 - UI and web links fixes, temporary disable testing/develop update channels
	https://github.com/nzbgetcom/nzbget/pull/19	
 - Fixed negative values for ThreadCount in json-rpc
	https://github.com/nzbget/nzbget/issues/696
	https://github.com/nzbgetcom/nzbget/pull/10	
 - Fix python 3.x script execution windows support
	https://github.com/nzbgetcom/nzbget/pull/8
 - regex library moved directly to project for windows build
 - removed one certificate from cacert.pem that caused error message
 - build scripts files updated

nzbget-21.2-testing:
  - please see repository change log at
    https://github.com/nzbget/nzbget/commits/develop

nzbget-21.1:
  - fixed crash on systems with 64-bit time;
  - corrected icon in Windows "uninstall program" list;
  - allow special characters in URL for username and password;
  - improved reporting for binding errors on Windows;
  - fixed unicode space characters in javascript files, which could cause issues
    with nginx proxy;
  - fixed negative values for "FileSizeLo" in json-rpc;
  - corrected url detection in rpc-method "append";
  - added support for new error messages in unrar 5.80;
  - now always using snapshots when reading directory contents:
    - in previous versions snapshots were used on macOS only;
    - now they are used on all OSes;
    - this solves issue with leftovers during directory cleanup, which could
      happen on certain OSes when working with network drives;
  - fixed file allocating on file systems where sparse files are not supported:
    - the issue could happen when InterDir was located on a network drive;
  - fixed crash caused by malformed nzb files;
  - fixed GROUP command in nserv;
  - updated url of the global certificate storage file in the build scripts;
  - fixed: file selector in WebKit based browsers doesn't allow to choose the
    same file again;
  - removed outdated links from web interface;
  - fixed PC sleep mode not working (Windows only);
  - set "SameSite" attribute for cookies;
  - corrected typo in about dialog of web interface;
  - updated license text: changed address of Free Software Foundation and minor
    formatting changes.

nzbget-21.0:
  - reworked duplicate handling to support URLs, especially when using RSS
    feeds:
      - queue items added via URLs (to be fetched by nzbget) are no longer
        immediately fetched;
      - instead url-items are handled by duplicate check similar to nzb-items
        and may be placed into history as duplicate backups;
      - if an url-item needs to be downloaded as backup for a failed other item
        the nzb-file is fetched via provided URL;
      - this greatly reduces the number of nzbs fetched from indexers when using
        RSS feeds and duplicate handling;
  - improved support for Android devices:
    - now providing a separate installer package for Android;
    - the package contains binaries built using Android NDK;
    - this improves compatibility with Android, in particular with Android 8,
      where general Linux installer version of NZBGet didn't work anymore due
      to security changes in Android;
    - android installer app is updated to use the new android installer package
      instead of general Linux package;
  - thoroughly optimised the program to reduce power consumption in idle state:
    - number of CPU wake ups in idle state has been reduced from hundreds times
      per second to about only one per second;
  - optimisations for large queues with thousands of items:
    - speed up saving of queue state and reduced number of queue state savings;
    - improved queue state format to reduce amount of state data saved during
      downloading;
    - in tests download speed for very large queue (16000 items) has been
      increased from 45 MB/s to 300 MB/s (comparing to 400 MB/s with small
      queue); 
  - added native support for aarch64 architecture (ARM 64 Bit CPU) in Linux and
    Android installers;
  - force par-check for nzbs without archives;
  - added functional tests for unpack CRC error;
  - click on nzbget logo in web-interface now switches to downloads tab instead
    of showing "About dialog" which has been moved into settings;
  - improved handling of files splitted via par2;
  - added python 3 compatibility to EMail.py script;
  - added python 3 compatibility to Logger.py script;
  - proper UTF-8 encoding of email content in EMail.py script;
  - improved error reporting for queue disk state corruption;
  - updated unrar to 5.7 and 7-zip to 19.0;
  - Windows installer now includes unrar in 32 bit and 64 bit variants;
  - allowing wildcards in option AuthorizedIP;
  - removed suggestion of RC4 cipher;
  - better description of option UMask;
  - integrated LGTM code analyser tool into project;
  - fixed: failed downloads not having any par2- or archive- files were moved to
    DestDir instead of remaining in InterDir;
  - fixed crash when using FIPS version of OpenSSL;
  - fixed compatibility issue with OpenSSL compiled without compression support;
  - fixed deprecated OpenSSL calls;
  - fixed potential crash in built-in web-server;
  - fixed: statistics for session download time and speed may be way off on high
    load;
  - fixed many compilation warnings in GCC;
  - fixed: macOS menubar widget could not connect if password contained special
    characters;
  - fixed: remote clients not displaying current download speed;
  - fixed: remote server could crash when feed with invalid api request;
  - fixed trimming of relative paths in config.

nzbget-20.0:
  - massive performance optimisations in downloader:
        - improved yEnc decoder;
        - improved CRC32 calculation;
        - processing data in one pass;
        - SIMD implementation of decoder and CRC functions on x86 and ARM CPUs;
          SIMD code relies on node-yencode library by Anime Tosho
          (https://github.com/animetosho/node-yencode);
        - overall performance improvement up to +500% on x86 and +250% on ARM
          (better speed or less CPU usage);
  - using glibc instead of uClibc in universal installer builds for Linux:
        - this significantly improves performance;
        - compatibility with Android and other systems is hopefully also improved;
        - in universal installer glibc is used on x86 and ARM;
        - uClibc is still used on MIPS and PPC;
  - performance optimisations in web-interface:
        - reduced number of requests when loading webui by combining all
          javascript-files into one and all css-files into one;
        - reduced load time by calling multiple API-methods simultaneously;
        - extensive use of browser caching for static files significantly
          reduces the amount of data transferred on webui initialisation;
        - extensive use of browser caching for API requests reduces the amount
          of data transferred during webui status updates, especially when
          nzbget is in idle state and there are no changes in download queue or
          history;
        - avoid work in browser on status updates if there are no changes in
          download queue or history;
        - support for keep alive connections significantly reduces overhead for
          establishing connections on webui status updates, especially when
          connecting to nzbget via TLS/SSL (avoiding TLS handshakes);
  - a number of performance optimisations for large download queue with
    thousands of items:
        - much faster loading of queue from disk greatly improves program start
          up time;
        - improved queue management for faster download speed;
  - now offering 64 bit binaries for Windows:
        - installer includes 32 and 64 bit nzbget binaries;
        - when updating from older versions the 64 bit binary is installed
          automatically, although into the old location to keep all your
          shortcuts intact;
        - using word "windows" instead of "win32" in the setup file name;
  - automatic update check:
        - new option "UpdateCheck" to check for stable or testing versions (or
          disable);
        - when a new version is found a notification is shown;
        - the update check is enabled by default for stable versions;
  - significantly improved logging performance, especially in debug builds;
  - par-check prints par2 creator application to help identify creator app
    issues;
  - added support for Unix domain sockets (POSIX only);
  - better error handling when fetching rss feeds;
  - updated POSIX build files to newer autotools version:
        - compatibility with newer autotools;
        - compatibility with newer platforms such as aarch64;
  - better username/password validation when testing connection on settings
    page;
  - improved rar-renamer to better handle certain cases;
  - new option "SkipWrite" for easier speed tests;
  - support for redirect codes 303, 307 and 308 in web-client for fetching of
    rss feeds and nzb-files;
  - installer for FreeBSD is now built using Clang instead of GCC; this fixes
    incompatibility with FreeBSD 11;
  - universal Linux installer can now be used on FreeBSD (via Linux
    compatibility mode);
  - updated unrar to v5.50;
  - more robust news server connection test;
  - enhancements in NServ:
        - memory cache to reduce disk load during speed tests - new command line
          switch "-m";
        - speed control - new command line switches "-w" and "-r";
        - show IP address of incoming connection;
  - changed default location of log-file;
  - better handling of broken connections;
  - removed obsolete or less useful options "SaveQueue", "ReloadQueue",
    "TerminateTimeout", "AccurateRate", "BrokenLog";
  - renamed option "LogBufferSize" to "LogBuffer";
  - passwords of failed login attempts are no longer printed to log to improve
    security;
  - cookies in web interface are now saved with "httpOnly" attribute to improve
    security;
  - titles and duplicate keys in duplicate check are now case insensitive;
  - added LibreSSL support;
  - web interface now has a better icon for favorites or home screen of mobile
    devices;
  - improved duplicate detection for obfuscated downloads having files with
    same subjects;
  - direct rename and direct unpack are now active by default on new
    installations, except for slow Linux systems;
  - added advice for letsencrypt in option descriptions;
  - fixed incorrect renaming in rar-renamer which could cause some downloads to
    fail;
  - fixed race condition in queue script coordinator which could cause crashes;
  - fixed: post-processing parameters were sometimes case sensitive causing
    issues;
  - fixed DNS resolving issues on Android;
  - fixed: backup servers not used on certain article decoding errors;
  - fixed: when direct rename was active certain downloads with damaged
    par2-files become paused at near completion and required manual resuming;
  - fixed: crash when flushing article cache after direct rename;
  - fixed: deleting active par-job may crash the program;
  - fixed: functional tests may fail on Windows due to locked files;
  - fixed: unpack using password file doesn't work on Windows;
  - fixed: compiler error when building using GnuTLS;
  - fixed: Linux installer failure on android emulator;
  - fixed: options formatted as password fields when they shouldn't;
  - fixed: slightly off article statistics after direct rename;
  - fixed: NServ terminated if client interrupted connection;
  - fixed: example pp-scripts may not work properly if nzbget password or
    username contained special characters;
  - fix in functional tests to not rely on sizes of externally generated files;
  - fixed: option AuthorizedIP did not work with IPv6 addresses;
  - fixed crash on certain malformed articles;
  - fixed crash which could happen on Windows when reloading or terminating the
    program;
  - fixed logging of IPv6 addresses.

nzbget-19.1:
  - proper handling of changing category (and destination path) during direct
    unpack; direct unpack now gracefully aborts with cleanup; the files will
    be unpacked during post-processing stage;
  - fixed: password protected downloads of certain kind may sometimes end up
    with no files if direct unpack was active;
  - fixed: rar-renamer mistakenly renamed some encrypted rar3 files causing
    unnecessary repair;
  - fixed: rar-renamer could not process some encrypted rar3-archives.

nzbget-19.0:
  - unpack during downloading:
        - downloaded files can now be unpacked as soon as every archive part is
          downloaded;
        - new option "DirectUnpack" to activate direct unpacking;
        - direct unpack works even with obfuscated downloads; option
         "DirectRename" (see below) must be active for that;
        - option "ReorderFiles" (see below) should be also active for optimal
          file download order;
        - direct unpack works for rar-archives; 7-zip archives and simply
          splitted files are processed by default unpack module;
        - direct unpack obviously works only for healthy download; if download
          is damaged the direct unpack cancels and the download is unpacked
          during post-processing stage after files are repaired;
        - direct unpack reduces the time needed to complete download and
      post-processing;
        - it also allows to start watching of video files during download
          (requires compatible video player software);
  - renaming of obfuscated file names during downloading:
        - correct file names for obfuscated downloads are now determined during
          download stage (instead of post-processing stage);
        - downloaded files are saved into disk directly with correct names;
        - direct renaming uses par2-files to restore correct file names;
        - new option "DirectRename" to activate direct renaming;
        - new queue-event NZB_NAMED, sent after the inner files are renamed;
  - automatic reordering of files:
        - inner files within nzb reordered to ensure download of files in
          archive parts order;
        - the files are reordered when nzb is added to queue;
        - if direct renaming is active (option "DirectRename") the files are
      reordered again after the correct names becomes known;
        - new option "ReorderFiles";
        - new command "GroupSortFiles" in api-method "editqueue";
        - new subcommand "SF" of remote command "-E/--edit";
  - new option "FileNaming" to control how to name obfuscated files (before they
    get renamed by par-rename, rar-rename or direct-rename);
  - TLS certificate verification:
        - when connecting to a news server (for downloading) or a web server
         (for fetching of rss feeds and nzb-files) the authenticity of the
          server is validated using server security certificate. If the check
          fails that means the connection cannot be trusted and must be closed
          with an error message explaining the security issue;
        - new options "CertCheck" and "CertStore";
        - official NZBGet packages come with activated certificate check;
        - when updating from an older NZBGet version the option CertCheck will
          be automatically activated when the settings is saved (switch to
          Settings page in web-interface and click "Save all changed");
  - authentication via form in web-interface as alternative to HTTP
    authentication:
        - that must help with password tools having issues with HTTP
          authentication dialog;
        - new option "FormAuth";
  - drop-downs (context menus) for priority, category and status columns:
        - quicker changing of priority and category;
        - easier access to actions via drop-down (context menu) in status
          column;
  - extensions scripts can now be executed from settings page:
        - script authors define custom buttons;
        - when clicked the script is executed in a special mode and obtain extra
          parameters;
        - example script "Email.py" extended with button "Send test e-mail";
  - on Windows NZBGet can now associate itself with nzb-files:
        - use option in Windows installer to register NZBGet for nzb-files;
  - unrar shipped within Linux package is now compiled with "fallocate" option
    to improve compatibility with media players when watching videos during
    downloading and unpacking;
  - support for HTTP-header "X-Forwarded-For" in IP-logging;
  - improvements in RSS feed view in phone mode;
  - set name, password and dupe info when adding via URL by click on a button
    near URL field in web-interface;
  - backup-badge for items in history similar to downloads tab;
  - show backup icon in history in phone theme;
  - added support for ECC certificates in built-in web-server;
  - save changes before performing actions in history dialog;
  - proper exit code on client command success or failure.
  - added host name to all error messages regarding connection issues;
  - new button "Volume Statistics" in section "News Servers" of settings page;
    shows the same volume data as in global statistics dialog;
  - new option "ServerX.Notes" for user comments on news servers;
  - new parameters for api-method "servervolumes" as a performance optimization
    measure to reduce amount of transferred data;
  - new option to force connection to news servers via ipv4 or ipv6;
  - removed unnecessary requests to news servers;
  - updated unrar to v5.40;
  - clear script execution log before executing script;
  - added support for crash dumps on Windows:
        - renamed option "DumpCore" to "CrashDump";
        - new option "CrashTrace" to make it possible to disable default
          printing off call stack in order to produce more relevant crash dumps;
  - fixed: startup scheduler tasks can be executed again;
  - fixed: "fatal" messages when compiling from sources.
  - fixed: per-nzb download statistics could be wrong if the program was
    reloaded during downloading.
  - fixed crash which may happen between post-processing steps;
  - fixed: asterix (*) was sometimes passed as parameter to extension scripts
    (Windows only);
  - fixed potential crash during update via web-interface.

nzbget-18.1:
  - fixed: crash during download caused by a race condition;
  - fixed: sleep mode did not work on Windows;
  - fixed: queue was not saved after deleting of queued post-jobs;
  - fixed: possible crash at the end of post-processing;
  - fixed: "undefined" in reorder extension scripts.

nzbget-18.0:
  - automatic deobfuscation of rar-archives without par-files:
        - obfuscated downloads not having par-files can now be successfully
          unpacked;
        - also helps with downloads where rar-files were obfuscated before
          creating par-files;
        - new options "RarRename" and "UnpackIgnoreExt";
  - multi post-processing:
        - in addition to classic post-processing strategy where items are
          processed one after another it is now possible to post-process
          multiple items at the same time;
        - new option "PostStrategy" to choose from four: sequential, balanced,
          aggressive, rocket;
        - in "balanced" strategy downloads needing repair do not block other
          items which are processed sequentially but simultaneously with
          repairing item;
        - in "aggressive" mode up to three items are post-processed at the same
          time and in "rocket" mode up to six items (including up to two repair
          tasks);
  - unified extension scripts settings:
        - options "PostScript", "QueueScript", "ScanScript" and "FeedScript"
          were replaced with one option "Extensions";
        - users don't need to know the technical details os extension scripts as
          all scripts are now can be selected at one place;
        - easier activation of complex extension scripts which previously needed
          to be selected in multiple options for their proper work;
  - reordering download queue with drag and drop in web-interface:
        - new actions "GroupMoveBefore" and "GroupMoveAfter" in API-method
          "editqueue";
  - priorities are now displayed as a column instead of badge; that makes it
    possible to manually sort on priority;
  - removed vertical lines in tables; looks better in combination with new
    priority column;
  - keyboard shortcuts in web-interface;
  - improved UI to prevent accidental deletion of many items:
        - visual indication of records selected on other pages;
        - extra warning when deleting many records from history;
  - additional options in "custom pause dialog";
  - better handing of damaged par2-files in par-renamer:
        - if par-renamer can't load a (damaged) par2-file then another par2-file
          is downloaded and par-renamer tries again;
        - reverted non-strict par2-filename matching to handle article subjects
          with non-parseable filenames;
  - better handling of obfuscated par-files;
  - splitted option "Retries" into "ArticleRetries" and "UrlRetries"; option
    "RetryInterval" into "ArticleInterval" and "UrlInterval";
  - scheduler tasks can be started at program launch:
        - use asterisk as TaskX.Time;
  - graceful termination of scheduler scripts:
        - scripts receive signal SIGINT (CTRL+BREAK on Windows) before
          termination;
  - added support for nZEDb attributes in rss feeds;
  - better cleanup handling: if parameter "unpack" is disabled for an nzb-file
    the cleanup isn't performed for it;
  - fields containing passwords are now displayed as protected fields;
  - showing password-badge for nzbs with passwords;
  - allow control of what tab is shown when opening web-interface:
    add "#downloads", "#history", "#messages" or "#settings" to the URL,
    for example "http://localhost:6789/#history" or
    "http://localhost:6789/index.html#history";
  - functional testing to ensure program quality:
        - implemented built-in simple nntp server to be used for functional
          testing;
        - created a number of tests;
        - new features come with additional tests;
  - improved API-method "append" in combination with duplicate check; method
    returns nzb-id also for items added straight to history;
  - removed parameter "offset" from api-method "editqueue":
        - when needed the "offset" is now passed within parameter "Args" as
          string;
        - old method signature is supported for compatibility;
  - improved error reporting on feed parse errors;
  - highlighting selected rows with alternative colors;
  - improved selecting of par2-file for repair;
  - splitted config section "Download Queue" and moved many options into new
    section "Connection";
  - disabled SSLv3 in built-in web-server;
  - multiple recipients in the example pp-script "EMail.py";
  - added compatibility with openssl 1.1.0;
  - fixed TLS handshake error when using GnuTLS;
  - fixed: sorting of selected items may give wrong results;
  - fixed: search box filter in feed view were not reset.

nzbget-17.1:
  - adjustments and fixes for "Retry failed articles" function, better handling
    of certain corner cases;
  - partial compatibility with gcc 4.8;
  - removed unnecessary debug logging to javascript console;
  - improved error reporting on certain file operations;
  - corrected option description;
  - corrected text in history delete confirmation dialog;
  - fixed performance issue on certain Windows systems;
  - fixed: root drive paths on Windows could not be used (for example
    "NzbDir=N:\");
  - fixed hanging after marking as BAD from queue script;
  - fixed: old nzbget.exe was deleted even when installing into a new directory
    (Windows only);
  - fixed: compilation error if configured with unit tests but without par-module;
  - fixed crash on malformed articles;
  - fixed javascript error on Chrome for Linux;
  - fixed compilation error if configured without TLS.

nzbget-17.0:
  - reworked the full source code base to utilize modern C++ features:
        - with the main motivation to make the code nicer and more fun to work
          with;
        - to compile NZBGet from source a compiler supporting C++14 is required;
        - most users don't have to compile on their own and can use official
          installers offered on download page;
        - for a detailed list of internal changes see Milestone "Modern C++" on
          GitHub;
  - now offering an official installer for FreeBSD:
        - automatic installation;
        - built-in update via web-interface;
        - currently supporting only x86_64 CPU architecture;
  - full support for Unicode and extra long file paths (more than 260 characters)
    on Windows including:
        - downloading;
        - par-verification and -repair;
        - par-renaming (deobfuscation);
        - unpacking;
        - post-processing;
  - added download volume quota management:
        - new options "MonthlyQuota", "QuotaStartDay", "DailyQuota";
        - downloading is suspended when the quota is reached and automatically
          resumed when the next billing period starts (month or day);
        - new fields in RPC-method "status": MonthSizeLo, MonthSizeHi,
          MonthSizeMB, DaySizeLo, DaySizeHi, DaySizeMB, QuotaReached.
          MonthSizes are related to current billing month taking option
          "QuotaStartDay" into account;
        - download volume for "this month" shown in web-interface in statistics
          dialog shows data for current billing month (taking option
          "QuotaStartDay" into account);
        - remaining time is shown in orange when the quota is reached;
        - dialog "statistics and status" may show extra row "Download quota:
          reached";
  - new function "Retry failed articles" in history:
        - failed downloads can be now tried again but in contrast to command
          "Download again" only failed articles are downloaded whereas
          successfully downloaded pieces are reused;
        - new command "HistoryRetryFailed" of RPC-method "editqueue";
        - new subcommand "F" of command line switch "-E/--edit" for history;
  - reworked options to delete already downloaded files when deleting downloads
    from queue:
        - removed option "DeleteCleanupDisk";
        - in the "Delete downloads confirmation dialog" allowing user to decide
          if the already downloaded files must be deleted or not;
        - option "HealthCheck" extended with new possible value "Park"; Health
          check now offers:
            - "Delete" - to move download into history and delete already
               downloaded files;
            - "Park" - to move download into history and keep already downloaded
               files;
        - remote command "GroupDelete" now always delete already downloaded files;
        - new remote command "GroupParkDelete" keeps already downloaded files;
        - new subcommand "DP" of console command "--edit/-E" to delete download
          from queue and keep already downloaded files;
  - new queue script event "NZB_MARKED"; new env var "NZBNA_MARKSTATUS" is
    passed to queue scripts:
  - new option "ShellOverride" allows to configure path to python, bash, etc.;
    useful on systems with non-standard paths; eliminating the need to change
    shebang for every script; also makes it possible to put scripts on non-exec
    file systems;
  - reduced disk fragmentation in direct write mode on Windows; this improves
    unpack speed;
  - news servers can now be configured as optional; marking server as optional
    tells NZBGet to ignore this server if a connection to this server cannot be
    established;
  - added support for tvdbid and tvmazeid in rss feeds;
  - added button to save nzb-log into a file directly from web-ui;
  - queue-scripts can now change destination after download is completed and
    before unpack;
  - queue-scripts save messages into nzb-log;
  - showing number of selected items in confirmation box when deleting or
    performing other actions on multiple items in web-interface;
  - built-in web-server can now use certificate chain files through option
    "SecureCert", when compiled using OpenSSL;
  - added support for SNI in TLS/SSL;
  - better error reporting when using GnuTLS;
  - allowing character "=" in dupe-badges;
  - par-check doesn't ignore files from option "ExtCleanupDisk" anymore; only
    files listed in option "ParIgnoreExt" are ignored;
  - low-level messages from par2-module are now added to log (as DETAIL);
  - option "ScriptDir" now accepts multiple directories;
  - path to original queued nzb-file is now passed to scripts;
  - hidden files and directories are now ignored by the scanner of incoming nzb
    directory;
  - separated disk state files for queue and history for better performance;
  - improved replacing of invalid characters in file names in certain cases;
  - automatically removing orphaned diskstate files from QueueDir;
  - added support for file names with reserved words on Windows;
  - added several settings to change behavior of web-interface, new section
    "WEB-INTERFACE" on settings page;
  - showing various status info in browser window title:
        - number of downloads, current speed, remaining time, pause state;
        - new option "WindowTitle";
  - improved tray icon (Windows) to look better on a dark background;
  - feed scripts must now return exit codes; this is to prevent queueing of
    all files from the feed if the feed script crashes;
  - improved error reporting on DNS lookup errors;
  - fixed: splitted files were not always joined;
  - fixed: wrong encoding in file names of downloaded files;
  - fixed: queue-scripts not called for failed URLs if the scripts were set in
    category’s option "PostScript";
  - fixed: crash when executing command "--printconfig";
  - fixed: error messages when trying to delete intermediate directory on Windows;
  - fixed: web-ui didn't load in Chrome on iOS;
  - improved POSIX configure script - now using pkg-config for all required
    libraries;
  - improved Windows installer - scripts are now installed into a subdirectory
    of default "MainDir" (C:\ProgramData\NZBGet\scripts) instead of program's
    directory;
  - updated and corrected option descriptions.

nzbget-16.4:
  - fixed resource (socket) leak which may cause "too many open files"
    errors with a possible crash.

nzbget-16.3:
  - fixed: certain downloads may fail due to a bug in the workaround
    implemented in v16.2.

nzbget-16.2:
  - implemented workaround to deal with malformed news server responses,
    which may still contain useful data.

nzbget-16.1:
  - fixed issues with reverse proxies;
  - fixed unpack failure on older AMD CPUs, when installed via
    universal Linux installer;
  - fixed: when the program was started from setup the default directories
    were created with wrong permission (Windows only).

nzbget-16.0:
  - moved project hosting to GitHub:
        - moved source code repository from subversion to git;
        - updated POSIX makefile to generate revision number for testing
          releases;
        - updated Linux installer build script to work with git;
        - adjusted function "check for updates" in web-interface;
        - update-scripts for Linux installer and Windows setup fetch new
          versions from GitHub releases area;
        - cleaned up project root directory, removed many unneeded files which
          were required by GNU build tools;
  - added verification for setup authenticity during update on Linux and Windows;
  - improvements in Linux installer:
        - improved compatibility with android;
        - added support for paths with spaces in parameter "--destdir";
        - auto-selecting "armhf"-architecture on ARM 64 bit systems (aarch64);
  - ignored nzbs are now added to history and processed by queue scripts:
	  - when an nzb-file isn’t added to queue, for some reason, the file is now
	    also added to history (in addition to messages printed to log);
	  - on one hand that makes it easier to see errors (as history items instead
	    of error log messages), on the other hand that provides more info for
	    extension scripts and third-party programs;
	  - for malformed nzb-files which cannot be parsed the status in history is
	    "DELETE: SCAN";
	  - for duplicate files with exactly same content status "DELETE: COPY";
	  - for duplicate files having history items with status "GOOD" and for
	    duplicate files having hidden history items with status "SUCCESS" which
	    do not have any visible duplicates - status "DELETE: GOOD";
	  - new values for field "DeleteStatus" of API-Method "history": "GOOD",
	   "COPY", "SCAN";
	  - new values for field "Status" of API-Method "history": "FAILURE/SCAN",
	    "DELETED/COPY", "DELETED/GOOD" (they will also be passed to pp-scripts
	    as NZBPP_STATUS);
	  - new queue-script event "NZB_DELETED";
	  - new queue event "URL_COMPLETED", with possible details: "FAILURE",
	    "SCAN_SKIPPED", "SCAN_FAILURE";
  - improved quick filter (the search box at the top of the page):
	  - now supporting OR, AND, NOT, parenthesis;
	  - can search in specific fields;
	  - can search in API-fields which are not shown in the table;
	  - filters can be saved and loaded using new drop down menu;
  - new advanced duplicate par-check mode:
	  - can be activated via option "ParScan=Dupe";
	  - the new mode is based on the fact that the same releases are posted to
	    Usenet multiple times;
	  - when an item requires repair but doesn't have enough par-blocks the
	    par-checker can reuse parts downloaded in other duplicates of the same
	    title;
	  - that makes it sometimes possible to repair downloads which would fail
	    if tried to repair individually;
	  - the downloads must be identifiable as duplicates (same nzb name or same
	    duplicate key);
	  - when par-checker needs more par-blocks it goes through the history and
	    scans the download directories of duplicates until it finds missing
	    blocks; it's smart enough to abort the scanning if the files come from
	    different releases (based on video file size);
	  - adjusted option "HealthCheck"; when set to "Delete" and duplicate
	    par-scan is active, the download is aborted only if the completion is
	    below 10%; that's to avoid deletion of damaged downloads which can be
	    potentially repaired using new par-check mode;
	  - downloads which were repaired using the new mode or which have donated
	    their blocks to repair other downloads are distinguishable in the
	    history details dialog with new status "EXPAR: RECIPIENT" or "EXPAR: 
	    DONOR"; a tooltip on the status shows amount of par-blocks
	    received/donated;
	  - new field "ExParStatus" returned by API-method "history";
	  - to quickly find related history items use quick filter
	    "-exparstatus:none" in history list;
  - when adding nzb-files via web-interface it's now possible to change name and
    other properties:
        - nzb-files in the upload-list are now clickable;
        - the click opens properties dialog where the name, password, duplicate
          key and duplicate score can be changed;
  - queue script activity is now indicated in web-interface:
	  - items in download list may have new statuses "QS-QUEUED" (gray) or
	    "QUEUE-SCRIPT" (green);
	  - new values for field "Status" in API-method "listgroups": "QS_QUEUED",
	    "QS_EXECUTING";
	  - the number of active (and queued) scripts are shown in the status dialog
	    in web-interface; this new row is hidden if no scripts are queued;
	  - active queue scripts account for activity indicator in web-interface
	    (rotating button);
	  - new field "QueueScriptCount" in API-method "status" indicates number of
	    queue-scripts queued for execution including the currently running;
  - added scripting support to RSS Feeds:
	  - new option "FeedScript" to set global rss feed scripts;
	  - new option "FeedX.FeedScript" to define per rss feed scripts;
  - new option "FeedX.Backlog" to control the RSS feed behavior when fetched for
    the first time or after changing of URL or filter;
  - implemented cleanup for field "description" in RSS feeds to remove HTML
    formatting;
  - new option "FlushQueue" ("yes" by default) to force disk cache flush when
    saving queue;
  - quick toggle of speed limit; "Command/Control + click-on-speed-icon" toggles
    between "all servers active and speed-limit=none" and "servers and speed
    limit as in the config file";
  - new option "RequiredDir" to wait for external drives mount on boot;
  - hidden webui option "rowSelect" now works for feed view too;
  - improved error message for password protected archives;
  - increased limit for log-entries in history dialog from 1000 to 10000;
  - completion tab of download details dialog (and history details dialog)
    shows per server article completion in percents; now there are also
    tooltips to show article counts;
  - do not reporting bad blocks for missing files (which are already reported as
    missing);
  - new setting to set tray icon behavior on Windows;
  - improvements in built-in web-server to fix communication errors which may
    occur on certain systems, in particular on OS X Safari:
        - implemented graceful disconnect strategy in web-server;
        - added authorization via X-Auth-Token to overcome Safari’s bug, where
          it may stop sending HTTP Basic Auth header when executing ajax
          requests;
  - pp-script EMail.py now supports new TLS/SSL mode "Force". When active the
    secure communication with SMTP server is built using secure socket on
    connection level instead of plain connection and following switch into
    secure mode using SMTP-command "STARTLS". This new mode is in particular
    required when using GMail on port 465;
  - speed improvement in built-in web-server on Windows when serving API
    requests (web-interface) for very large queue or history (with thousands
    items);
  - better performance when deleting many items from queue at once (hundreds or
    thousands);
  - improved performance in web-interface when working with very large queue or
    history (thousands of items);
  - integrated unit testing framework, created few first unit tests:
        - new configure parameter "--enable-tests" to build the program with
          tests;
        - use "nzbget --tests" to execute all tests or "nzbget --tests -h" for
          more options;
  - fixes in Linux installer:
        - fixed: installer may show wrong IP-address in the quick help message
          printed after installation (thanks to @sanderjo);
        - fixed: installation failed on certain WD NAS models due to Busybox
          limitations;
        - fixed: not working endianness detection on mipseb architecture;
        - fixed: unrar too slow on x86_64 architecture;
  - fixed: reporting of bad blocks for empty files could print garbage file
    names;
  - fixed: par-check did not work on UNC paths (Windows only);
  - fixed: config error messages were not printed to log or screen but only to
    stdout, where users typically don’t see them;
  - fixed: incorrect reading of UrlStatus from disk state;
  - fixed: total articles wasn't reset when downloading again;
  - fixed: crash on reload if a queue-script is running;
  - fixed: mark as bad may return items to queue if used on multiple items
    simultaneously;
  - fixed: updating may fail if NZBGet was not installed on the system
    drive (Windows only);
  - fixed: the program may hang during multithreading par-repair, only certain
    Linux system affected;
  - fixed: active URL download could not be deleted;
  - fixed: par-verification of repaired files were sometimes not skipped in
    quick verification mode (option "ParQuick=yes");
  - fixed: when parsing nzb-files filenames may be incorrectly detected causing
    certain downloads to fail;
  - fixed: nzb-files containing very large individual files (many GB) may cause
    program to crash or print error "Could not create file ...".

nzbget-15.0:
  - improved application for Windows:
        - added tray icon (near clock);
        - left click on icon pauses/resumes download;
        - right click opens menu with important functions;
        - console window can be shown/hidden via preferences (is hidden by
          default);
        - new preference to automatically start the program after login;
        - new preference to show browser on start;
        - new preference to hide tray icon;
        - menu commands to show important folders in windows explorer
          (destination, etc.);
        - on first start the config file is now placed into subdirectory
          "NZBGet" inside standard AppData-directory;
        - default destination and other directories are now placed in the
          AppData\NZBGet-directory instead of programs directory; this allows
          to install the program into "program files"-directory since the
          program does not write into the programs directory anymore;
        - the program exe has an icon now;
        - if the exe is started from windows explorer the program starts in
          application mode; if the exe is called from command prompt the program
          works in console mode;
        - added built-in update feature to windows package; accessible via
          web-interface -> settings -> system -> check for updates;
  - created installer for windows:
        - the program is installed into "program files" by default;
        - the working directory with all subdirectories is now placed into
          "AppData" directory;
        - the batch files nzbget-start.bat and nzbget-recovery-mode.bat are not
          needed and not installed anymore;
  - created installer for Linux:
        - included are precompiled binaries for the most common CPU architectures:
          x86, ARM, MIPS and PowerPC;
        - installer automatically detects CPU architecture of the system and
          installs an appropriate executable;
        - configuration file is automatically preconfigured for immediate use;
        - installation on supported platforms has become as simple as:
          download, run installer, start installed nzbget;
        - installer supports automatic updates via web-interface -> settings
          -> system - check for updates;
  - added support for password list file:
        - new option "UnpackPassFile" to set the location of the file;
        - during unpack the passwords are tried from the file until unpack
          succeeds or all passwords were tried;
        - implemented different strategies for rar4 and rar5-archives taking
          into account the features of formats;
        - for rar5-archives a wrong password is reported by unrar unambiguously
          and the program can immediately try other passwords from the password
          list;
        - for rar4-archives and for 7z-archives it is not possible to
          differentiate between damaged archive and wrong password; for those
          archives if the first unpack attempt (without password) fails the
          program executes par-check (preferably quick par-check if enabled via
          option "ParQuick) before trying the passwords from the list;
        - another optimization is that the password list is tried only when the
          first unpack attempt (without password) reports a password error or
          decryption errors; this saves unnecessary unpack attempts for damaged
          unencrypted archives;
  - options "UnrarCmd" and "SevenZipCmd" can include extra switches to pass to
    unrar/7-zip:
        - this allows for easy passing of additional parameters without creating
          of proxy shell scripts;
  - improved news server connections handling:
        - if a download of an article fails due to connection error the news
          server becomes temporary disabled (blocked) for several seconds
          (defined by option "RetryInterval");
        - the download is then retried on another news server (of the same
          level) if available;
        - if no other news servers (of the same level) exist the program will
          retry the same news server after its block interval expires;
        - this increases failure tolerance when multiple news servers are used;
  - added on-demand queue sorting:
        - one click on column title in web interface sorts the selected or all
          items;
        - if the items were already sorted in that order they are sorted
          backwards; in other words the second click sorts in descending order;
        - when sorting selected items they are also grouped together in a case
          there were holes between selected items;
        - RPC-method "editqueue" has new command "GroupSort", parameter "Text"
          must be one of: "name", "priority", "category", "size", "left"; add
          character "+" or "-" to explicitly define ascending or descending
          order (for example "name-"); if none of these characters
          is used the auto-mode is active: the items are sorted in ascending
          order first, if nothing changed - they are sorted again in descending
          order;
  - added restricted user and add-user:
        - restricted user has access to most program functions but cannot see
          security related options (including usernames and passwords) and
          cannot save configuration;
        - restricted user can be used with other programs and web-sites;
        - add-user can only add new downloads via RPC-API and can be used with
          other programs or web-sites;
  - added per-nzb logging:
        - each nzb now has its own individual log;
        - messages printed during download or post-processing are saved;
        - the messages can be retrieved later at any time;
        - new button "Log" in the history details dialog;
        - button "Log" in the download details dialog is now active during
          download too (not only during post-processing);
        - the log contains all nzb-related messages except detail-messages and
          errors printed during retrieving of articles (they would produce way
          too many messages and are not that useful anyway);
        - new option "NzbLog" to deactivate per-nzb logging if necessary;
        - per-nzb logs are saved in the queue-directory (option "QueueDir");
        - new RPC-method "loadlog" returns the previously saved messages for a
          given nzb-file;
        - new field "MessageCount" is returned by RPC-methods "listgroups" and
          "history" and indicates if there are any messages saved for the item;
        - parameter "NumberOfLogEntries" of RPC-method "listgroups" and the
          field "Log" returned by the method are now deprecated, use method
          "loadlog" instead;
        - field "PostInfoText" returned by RPC-method "listgroups" is now
          automatically filled with the latest message printed by a pp-script
          eliminating the need to access deprecated field "Log"';
  - actions for history items can now be performed for multiple (selected) records:
        - post-process again, download again, mark as good, mark as bad;
        - extended RPC-API method "editqueue": for history-records of type
          "URL" the action "HistoryRedownload" can now be used as synonym to
          "HistoryReturn" (makes it easier to redownload multiple items of
          different types (URL and NZB) with one API call).
  - options "ParIgnoreExt" and "ExtCleanupDisk" can now contain wildcard
    characters * and ?;
  - added new option "ServerX.Retention" to define server retention time (days);
    files older than configured server retention time are not even tried on this
    server;
  - added support for negative numeric values in rss filter (useful for fields
    "dupescore" and "priority");
  - added subcommand "HA" to remote command "--list/-L" to list the whole
    history including hidden records;
  - added optional parameters to remote command "--append/-A" allowing to pass
    duplicate key, duplicate mode and duplicate score; removed parameters "F"
    and "U" of command "--append/-A", which were used to set mode (file or URL),
    which is now detected automatically; the parameters are still supported for
    compatibility;
  - name and category of history items can now be changed in web-interface;
    RPC-API method "editqueue" extended with new actions "HistorySetName" and
    "HistorySetCategory";
  - improved timeout handling during establishing of connections;
  - updated pp-script "EMail.py":
        - using the new nzb-log feature;
        - new option "SendMail" allows to choose if the e-mail should be send
          always or on failure only;
  - updated pp-script "Logger.py" to use the new nzb-log feature;
  - improved cleanup (option ExtCleanupDisk):
        - if download was successful with health 100% the cleanup is now
          performed even if par-check and unpack were not made; previously a
          successful par-check or unpack were required for cleanup;
        - now the files are deleted in subdirectories too (recursively);
  - added a small button near feed name in the feed menu on downloads-page; a
    click on the button fetches the feed, whereas a click on the feed title
    shows feed's content (as before);
  - improved detection of malformed nzb-files: nzbs which are valid
    xml-documents but without nzb content are now rejected with an appropriate
    error message;
  - new action "Mark as success" on history page and in history details dialog:
        - items marked as success are considered successfully downloaded and
          processed, which is important for duplicate check;
        - use this command if the download was repaired outside of NZBGet;
        - new action "HistoryMarkSuccess" in RPC-method "editqueue";
        - new subcommand "S" of command "-E H" (command line interface);
        - new status "SUCCESS/MARK" can be returned by RPC-method "history";
  - improved support for update-scripts:
        - all command line parameters used to launch nzbget are passed to the
          script in env vars NZBUP_CMDLINEX, where X is a parameter number
          starting with 0;
        - if the path to update-script defined in package-info.json does not
          start with slash the path is considered being relative to application
          directory;
        - new env var NZBUP_RUNMODE (DAEMON, SERVER) is passed to the script;
        - fixed: env var NZBUP_PROCESSID had wrong value (ID of the parent
          process instead of the nzbget process);
  - added button "Test Connection" to make a news server connection test from
    web-interface;
  - renamed option "CreateBrokenLog" to "BrokenLog"; the old option name is
    recognized and automatically converted when the configuration is saved in
    web-interface;
  - improved the quality of speed throttling when a speed limit is active;
  - added hidden webui setting "rowSelect" to select records by clicking on any
    part of the row, not just on the check mark; to activate it change the
    setting "rowSelect" in webui/index.js;
  - when moving files to final destination the hidden files (with names starting
    with dot) are considered unimportant and no errors are printed if they
    cannot be moved; such files (.AppleDouble, .DS_Store, etc.) are usually
    used by services to hold metadata and can be safely ignored;
  - option sets (such as news-servers, categories, etc.) can now be reordered
    using news buttons "move up" and "move down";
  - updated info in about dialogs (Windows and Mac OS X);
  - updated description of few options;
  - changed defaults for few logging options;
  - improved timeout handling when connecting to news servers which have
    multiple addresses;
  - improved error handling when communicating with secure servers (do not
    trying to send quit-command if connection could not be established or was
    interrupted; this avoids unnecessary timeout);
  - improved connection handling when fetching nzb-files and rss feeds; do not
    print warning "Content-Length is not submitted by server..." anymore;
  - download speed in context menu of menubar icon is now shown in MB/s instead
    of KB/s (for speeds from 1 MB/s) (Mac OS X only);
  - configuration file nzbget.conf is now also searched in the app-directory on
    all platforms (for easier installation);
  - removed shell script "nzbgetd" which were used to control nzbget as a
    service; modern systems manage services in a diffreent way and do not
    require that old script anymore;
  - disabled changing of compiler options during configuring in debug mode
    (--enable-debug); it conflicted with cross-compiling and did not allow to
    pass extra options via CXXFLAGS; required debug options must be passed via
    CXXFLAGS now (for example for gcc: CXXFLAGS=-g ./configure --enable-debug);
  - disabled unnecessary assert-statements in par2-module when building in
    release mode;
  - fixed: parsing of RPC-parameters passed via URL were sometimes incorrect;
  - fixed: lowercase hex digits were not correctly parsed in URLs passed to
    RPC-API method "append";
  - fixed: in JSON-RPC the request-id was not transfered back in the response as
    required by JSON-RPC specification;
  - fixed: par-check in full verification mode (not in quick mode) could not
    detect damaged files if they were completely empty (0 bytes), which is
    possible when option "DirectWrite" was not active and all articles of the
    file were missing;
  - fixed possible crash when using remote command "-B dump" to print debug
    info;
  - fixed: remote command "-L HA" (which prints the history including hidden
    records) could crash;
  - suppress printing of memory leaks reports when the program terminates
    because of wrong command line switches (Windows debug mode only);
  - fixed: command "nzbget -L H" may crash if the history contained URL-items
    with certain status;
  - fixed: action "Split" may not work for bad nzb-files with missing segments;
    new Field "Progress" returned by RPC-method "listfiles" shows the download
    progress of the file taking missing articles into account;
  - if the lock-file cannot be created or the lock could not be acquired an
    error message is printed to the log-file;
  - fixed: update log shown during automatic update via web-interface may show
    duplicate messages or messages may clear out;
  - fixed: web-interface may fail to load on Firefox mobile;
  - fixed: command "make install" installed README from par2-subdirectory
    instead of main README.

nzbget-14.2:
  - fixed: the program could crash during download when article cache was active
    (more likely on very high download speeds);
  - fixed: unlike to all other scripts the update-script should not be
    automatically terminated when the program quits;
  - fixed: XML-RPC method "history" returned invalid xml when used with
    parameter "hidden=true" (JSON-RPC was fine).

nzbget-14.1:
  - fixed: program could crash during unpack (Posix) or unpack failure was
    reported (Windows);
  - fixed: quick par-check could hang on certain nzb-files containing multiple
    par-sets (occured only in 64 bit mode);
  - fixed: menubar icon was not visible on Mac OS X in dark mode; system sleep
    on idle state is now prevented during download and post-processing
    (Mac OS X only);
  - fixed: unrar may sometimes fail with message "no files to extract" (certain
    Linux systems);
  - fixed false memory leak warning when compiled in debug mode (Windows only);
  - fixed: Mac OS X app didn't work on OS X 10.7 Lion.

nzbget-14.0:
  - added article cache:
        - new option "ArticleCache" defines memory limit to use for cache;
        - when cache is active the articles are written into cache first and
          then all flushed to disk into the destination file;
        - article cache reduces disk IO and may reduce file fragmentation
          improving post-processing speed (unpack);
        - it works with both writing modes (direct write on and off);
        - when option "DirectWrite" is disabled the cache should be big enough
          (for best performance) to accommodate all articles of one file
          (sometimes up to 500 MB) in order to avoid writing articles into
          temporary files, otherwise temporary files are used for articles which
          do not fit into cache;
        - when used in combination with DirectWrite there is no such limitation
          and even a small cache (100 MB or even less) can be used effectively;
          when the cache becomes full it is flushed automatically (directly into
          destination file) providing room for new articles;
        - new row in the "statistics and status dialog" in web-interface
          indicates the amount of memory used for cache;
        - new fields "ArticleCacheLo", "ArticleCacheHi" and "ArticleCacheMB"
          returned by RPC-method "status";
  - renamed option "WriteBufferSize" into "WriteBuffer":
        - changed the dimension - now option is set in kilobytes instead of bytes;
        - old name and value are automatically converted;
        - if the size of article is below the value defined by the option, the
          buffer is allocated with the articles size (to not waste memory);
        - therefore the special value "-1" is not required anymore; during
          conversion "-1" is replaced with "1024" (1 megabyte) but it can be of
          course manually changed to any other value later;
  - integrated par2-module (libpar2) into NZBGet’s source code tree:
        - the par2-module is now built automatically during building of NZBGet;
        - this eliminates dependency from external libpar2 and libsigc++;
        - making it much easier for users to compile NZBGet without patching
          libpar2;
  - added quick file verification during par-check/repair:
        - if par-repair is required for download the files downloaded without
          errors are verified quickly by comparing their checksums against the
          checksums stored in the par2-file;
        - this makes the verification of undamaged files almost instant;
        - damaged (partially downloaded) files are also verified quickly by
          comparing block's checksums against the checksums stored in the
          par2-file; when necessary the small amounts of data is read from files
          to calculate block's checksums;
        - this makes the verification of damaged files very fast;
        - new option "ParQuick" (active by default);
        - when quick par verification is active the repaired files are not
          verified to save time; the only reason for incorrect files after
          repair can be hardware errors (memory, disk) but this is not something
          NZBGet should care about;
        - if unpack fails (excluding invalid password errors) and quick
          par-check does not find any errors or quick par-check was already
          performed the full par-check is performed; this helps in certain rare
          situations caused by abnormal program termination;
  - added multithreading par-repair:
        - doesn't depend on other libraries and works everywhere, on all
          platforms and all CPUs (with multiple cores), no special compiling
          steps are required;
        - new option "ParThreads" to set the number of threads for repairing;
        - the number of repair threads is automatically reduced to the amount of
          bad blocks if there are too few of them; if there is only one bad
          block the multithreading par-repair is switched off to avoid overhead
          of thread synchronisation (which does not make sense for one working
          thread);
        - new option "ParBuffer" to define the memory limit to use during
          par-repair;
  - added support for detection of bad downloads (fakes, etc.):
        - queue-scripts are now called after every downloaded file included in
          nzb;
        - new events "FILE_DOWNLOADED" and "NZB_DOWNLOADED" of parameter
          "NZBNA_EVENT"; new env. var "NZBNA_DIRECTORY" passed to queue scripts;
        - queue-scripts have a chance to detect bad downloads when the download
          is in progress and cancel bad downloads by printing a special command;
          downloads marked as bad become status "FAILURE/BAD" and are processed
          by the program as failures (triggering duplicate handling); scripts
          executed thereafter see the new status and can react accordingly
          (inform an indexer or a third-party automation tool);
        - when a script marks nzb as bad the nzb is deleted from queue, no
          further internal post-processing (par, unrar, etc.) is made for the
          nzb but all post-processing scripts are executed; if option
          "DeleteCleanupDisk" is active the already downloaded files are deleted;
        - new status "BAD" for field "DeleteStatus" of nzb-item in RPC-method
          "history";
        - queue-scripts can set post-processing parameters by printing special
          command, just like post-processing-scripts can do that; this
          simplifies transferring (of small amount) of information between
          queue-scripts and post-processing-scripts;
        - scripts supporting two modes (post-processing-mode and queue-mode) are
          now executed if selected in post-processing parameters: either in
          options "PostScript" and "CategoryX.PostScript" or manually on page
          "Postprocess" of download details dialog in web-interface; it is not
          necessary to select dual-mode scripts in option "QueueScript"; that
          provides more flexibility: the scripts can be selected per-category or
          activated/deactivated for each nzb individually;
        - added option "EventInterval" allowing to reduce the number of calls of
          queue-scripts, which can be useful on slow systems;
        - queue scripts can define what events they are interested in; this
          avoids unnecessary calling of the scripts which do not process certain
          events;
  - the list of scripts (pp-scripts, queue-scripts, etc.) is now read once on
    program start instead of reading everytime a script is executed:
        - that eliminates the unnecessary disk access;
        - the settings page of web-interface loads available scripts every time
          the page is shown;
        - this allows to configure newly added scripts without restarting the
          program first (just like it was before); a restart is still required
          to apply the settings (just like it was before);
        - RPC-method "configtemplates" has new parameter "loadFromDisk";
  - options "ParIgnoreExt" and "ExtCleanupDisk" are now respected by par-check
    (in addition to being respected by par-rename): if all damaged or missing
    files are covered by these options then no par-repair is performed and the
    download assumed successful;
  - added new search field "dupestatus" for use in rss filters:
        - the search is performed through download queue and history testing
          items with the same dupekey or title as current rss item;
        - the field contains comma-separated list of following possible statuses
          (if duplicates were found): QUEUED, DOWNLOADING, SUCCESS, WARNING,
          FAILURE or an empty string if there were no matching items found;
  - added log file rotation:
        - options "CreateLog" and "ResetLog" replaced with new option "WriteLog
          (none, append, reset, rotate)";
        - new option "RotateLog" defines rotation period;
  - improved joining of splitted files:
        - instead of performing par-repair the files are now joined by unpacker,
          which is much faster;
        - the files splitted before creating of par-sets are now joined as well
          (they were not joined in v13 because par-repair has nothing to repair
          in this case);
        - the unpacker can detect missing fragments and requests par-check if
          necessary;
  - added per-nzb time and size statistics:
        - total time, download, verify, repair and unpack times, downloaded size
          and average speed, shown in history details dialog via click on the
          row with total size in statistics block;
        - RPC-methods "listgroups" and "history" return new fields:
          "DownloadedSizeLo", "DownloadedSizeHi", "DownloadedSizeMB",
          "DownloadTimeSec", "PostTotalTimeSec", "ParTimeSec", "RepairTimeSec",
          "UnpackTimeSec";
  - pp-script "EMail.py" now supports mail server relays (thanks to l2g for the
    patch);
  - when compiled in debug mode new field "process id" is printed to the file
    log for each row (it is easier to identify processes than threads);
  - if an nzb has only few failed articles it may have completion shown as 100%;
    now it is shown as 99.9% to indicate that not everything was successfully
    downloaded;
  - updated configure-script to not require gcrypt for newer GnuTLS versions
    (when gcrypt is not needed);
  - for downloads delayed due to propagation delay (option "PropagationDelay")
    a new badge "propagation" is now shown near download name;
  - added new option "UrlTimeout" to set timeout for URL fetching and RSS feed
    fetching; renamed option "ConnectionTimeout" to "ArticleTimeout";
  - improved pp-script EMail.py: now it can send time statistics (thanks to JVM
    for the patch);
  - improvement in duplicate check:
        - if a new download with empty dupekey and empty dupescore is marked as
          "dupe" and the another download with the same name have non empty
          dupekey or dupescore these properties are copied from that download;
        - this is useful because the new download is most likely another upload
          of the same file and it should have the same duplicate properties for
          best duplicate handling results;
  - when connecting in remote mode using command line parameter "--connect/-C"
    the option "ControlIP" is now interpreted as "127.0.0.1" if it is set to
    "0.0.0.0" (instead of failing with an error message);
  - when option "ContinuePartial" is active the current state is saved not more
    often than once per second instead of after every downloaded article; this
    significantly reduce the amount of disk writings on high download speeds;
  - added commands "PausePostProcess" and "UnpausePostProcess" to scheduler;
  - unpack is now automatically immediately aborted if unrar reports CRC errors;
  - unpack is now immediately aborted if unrar reports wrong password (works for
    rar5 as well as for older formats); the unpack error status "PASSWORD" is
    now set for older formats too (not only rar5);
  - improved cleanup:
        - disk cleanup is now not performed if unrar failed even if par-check
          was successful;
        - queue cleanup (for remaining par2-files) is now made more smarter: the
          files are kept (parked) if they can be used by command "post-process
          again" and are removed otherwise;
  - improved scan-scripts: if the category of nzb-file is changed by the
    scan-script the assigned post-processing scripts are now automatically reset
    according to the new category;
  - added missing new line character at the end of the help screen printed by
    "nzbget -h";
  - better error reporting if a temp file could not be found;
  - added news server name to message "Cancelling hanging download ..." to help
    identifying problematic servers;
  - added column "age" to history tab in web-interface;
  - debug builds for Windows now print call stack on crash to the log-file,
    which is very useful for debugging;
  - additional parameters (env. vars) are now passed to scan scripts:
    NZBNP_DUPEKEY, NZBNP_DUPESCORE, NZBNP_DUPEMODE; scan-scripts can now set
    dupekey, dupemode and dupescore by printing new special commands;
  - fixed potential crash which could happen in debug mode during program
    restart;
  - fixed: program could crash during restart if an extension script was
    running; now all active scripts are terminated during restart;
  - fixed: RPC-method "editqueue" with action "HistoryReturn" caused a crash if
    the history item did not have any remaining (parked) files;
  - fixed: RPC-method "saveconfig" did not work via XML-RPC (but worked via
    JSON-RPC);
  - fixed: a superfluous comma at the end of option "TaskX.Time" was interpreted
    as an error or may cause a crash;
  - fixed: relative destination paths (options "DestDir" and
    "CategoryX.DestDir") caused failures during unrar;
  - fixed: splitted .cbr-files were not properly joined;
  - fixed: inner files (files listed in nzb) bigger than 2GB could not be
    downloaded;
  - fixed: cleanup may leave some files undeleted (Mac OS X only);
  - fixed: compiler error if configured using parameter "--disable-gzip";
  - fixed: one log-message was printed only to global log but not to nzb-item
    pp-log;
  - fixed: par-check could fail on valid files (bug introduced in libpar2 0.3);
  - fixed: scheduler tasks were not checked after wake up if the sleep time was
    longer than 90 minutes;
  - fixed: the "pause extra pars"-state was missing in the pause/resume-loop of
    curses interface, key "P";
  - fixed: web interface showed an error box when trying to submit files with
    extensions other than .nzb, although these files could be processed by a
    scan-script; now the error is not shown if any scan-script is set in options;

nzbget-13.0:
  - reworked download queue:
        - new dialog to build filters in web-interface with instant preview;
        - queue now holds nzb-jobs instead of individual files (contained
          within nzbs);
        - this drastically improves performance when managing queue
          containing big nzb-files on operations such as pause/unpause/move items;
        - tested with queue of 30 nzb-files each 40-100GB size (total queue
          size 1.5TB) - queue managing is fast even on slow device;
        - limitation: individual files (contained within nzbs) now cannot
          be moved beyond nzb borders (in older version it was possible to
          move individual files freely and mix files from different nzbs,
          although this feature was not supported in web-interface and
          therefore was not much known);
        - this change opens doors for further speed optimizations and integration
          of download queue with post-processing queue and possibly url-queue;
        - current download data such as remained size or size of paused files
          is now internally automatically updated on related events (download
          of article is completed, queue edited, etc.);
        - this eliminates the need of calculating this data upon each
          RPC-request (from web-interface) and greatly decrease CPU load
          of processing RPC-requests when having large download queue
          (and/or large nzb-files in queue);
        - field "Priority" was removed from individual files;
        - instead nzb-files (collections) now have field "Priority";
        - nzb-files now also have new fields "MinTime" and "MaxTime", which
          are set when nzb-file is parsed and then kept;
        - this eliminates the need of recalculation file statistics (min and
          max priority, min and max time);
        - removed action "FileSetPriority" from RPC-command "editqueue";
        - removed action "I" from remote command "--edit/-E" for individual
          files (now it is allowed for groups only);
        - removed few (not more necessary) checks from duplicate manager;
        - merged post-processing queue into main download queue;
        - changing the order of (pp-queued) items in the download queue
          now also means changing the order of post-processing jobs;
        - priorities of downloads are now respected when picking the next
          queued post-processing job;
        - the moving of download items in web-interface is now allowed for
          downloads queued for post-processing;
        - removed actions of remote command "--edit/-E" and of RPC-method
          "editqueue" used to move post-processing jobs in the post-processing
          queue (the moving of download items should be used instead);
        - remote command "-E/--edit" and RPC-method "editqueue" now use NZBIDs
          of groups to edit groups (instead of using ID of any file in the
          group as in older versions);
        - remote command "-L/--list" for groups (G) and group-view in
          curses-frontend now print NZBIDs instead of "FirstID-LastID";
        - RPC-method "listgroups" returns NZBIDs in fields "FirstID" and "LastID",
          which are usually used as arguments to "editqueue" (for compatibility 
          with existing third-party software);
        - items queued for post-processing and not having any remaining files
          now can be edited (to cancel post-processing), which was not possibly
          before due to lack of "LastID" in empty groups;
        - edit commands for download queue and post-processing queue are now
          both use the same IDs (NZBIDs);
        - merged url queue into main download queue;
        - urls added to queue are now immediately shown in web-interface;
        - urls can be reordered and deleted;
        - when urls are fetched the downloaded nzb-files are put into queue at
          the positions of their urls;
        - this solves the problem with fetched nzb-files ordered differently
          than the urls if the fetching of upper (position wise) urls were
          completed after the lower urls;
        - removed options "ReloadUrlQueue" and "ReloadPostQueue" since there
          are no separate url- and post-queues anymore;
        - nzb-files added via urls have new field "URL" which can be accessed
          via RPC-methods "listgroups" and "history";
        - new env. var. "NZBNP_URL", "NZBNA_URL" and "NZBPP_URL" passed to
          scan-, queue- and pp-scripts;
        - removed remote command "--list U", urls are now shown as groups
          by command "--list G";
        - RPC-method "urlqueue" is still supported for compatibility but
          should not be used since the urls are now returned by method
          "listgroups", the entries have new field "Kind" which can be
          "NZB" or "URL";
  - added collecting of download volume statistics data per news server:
        - in web-interface the data is shown as chart in "Statistics and
          Status" dialog;
        - new RPC-method "servervolumes" returns the collected data;
        - new RPC-method "resetservervolume" to reset the custom counter;
  - fast par-renamer now automatically detects and renames misnamed (obfuscated)
    par2-files;
  - for downloads not having any (obviously named) par2-files the critical
    health is assumed 85% instead of 100% as the absense of par2-files suggests:
        - this avoids the possibly false triggering of health-check action
          (delete or pause) for downloads having misnamed (obfuscated) par2-files;
        - combined with improved fast par-renamer this provides proper
          processing of downloads with misnamed (obfuscated) par2-files;
  - fast par-renamer now detects missing files (files listed in par2-files
    but not present on disk):
       - when checking for missing files the files whose extensions match
         with option "ExtCleanupDisk" are ignored now (to avoid time consuming
         restoring of files which will be deleted later anyway);
        - added option "ParIgnoreExt" which lists files which do not trigger
          par-repair if they are missing (similar to option "ExtCleanupDisk"
          but those files are not deleted during cleanup);
  - added new choice "Always" for option "ParCheck":
        - it forces the par-check for every (even undamaged) download but
          in contrast to choice "Force" only one par2-file is downloaded first;
        - additional files are downloaded if needed;
  - improved par-check for damaged collections with multiple par-sets and
    having missing files:
        - only orphaned files (not belonging to any par-set) are scanned
          when looking for missing files;
        - this greatly decrease the par-check time for big collections;
  - eliminated the distinction between manual pause and soft-pause:
        - there is only one pause register now;
        - options "ParPauseQueue", "UnpackPauseQueue" and "ScriptPauseQueue"
          do not change the state of the pause but instead are respected directly;
        - RPC-methods "pausedownload2" and "resumedownload2" are aliases to
          "pausedownload" and "resumedownload" (kept for compatibility);
        - field "Download2Paused" of RPC-method "status" is an alias to
          "DownloadPaused" (kept for compatibility);
        - action "D2" of remote commands "--pause/-P" and "--unpause/-U"
          is not supported anymore;
  - implemented general scripts concept:
        - the concept is a logical extension of the post-processing scripts
          concept initially introduced in v11;
        - the general scripts concept applies to all scripts used in the
          program: scan-script, queue-script and scheduler-script (in
          addition to post-processing scripts);
        - option "NzbProcess" renamed to "ScanScript";
        - option "NzbAddedProcess" renamed to "QueueScript";
        - option "DefScript" and "CategoryX.DefScript" renamed to
          "PostScript" and "CategoryX.PostScript" (options with old names
          are recognized and automatically converted on first settings saving);
        - new option "TaskX.Script";
        - old option "TaskX.Process" kept for scheduling of external
          programs not related to nzbget (to avoid writing of intermediate
          proxy scripts);
        - scan-script, queue-script and scheduler-script now work similar
          to post-processing scripts:
             - scripts must be put into scripts-directory;
             - scripts can be configured via web-interface and can have options;
             - multiple scripts can be chosen for each scripts-option, all
               chosen scripts are executed;
             - program and script options are passed to the script as
               env. variables;
        - renamed default directory with scripts from "ppscripts" to "scripts";
        - script signature indicates the type of script (post-processing,
          scan, queue or scheduler);
        - one script can have mixed signature allowing it to be used for
          multiple purposes (for example a notification script can send
          a notification on both events: after adding to queue and after
          post-processing);
        - result of RPC-method "configtemplates" has new fields "PostScript",
          "ScanScript", "QueueScript", "SchedulerScript" to indicate the
          purpose of the script;
        - queue-script (formerly NzbAddedProcess) has new parameter
          "NZBNA_EVENT" indicating the reason of calling the script;
          currently the script is called only after adding of files to
          download queue and therefore the parameter is always set to
          "NZB_ADDED" but the queue-script can be called on other events
          in the future too;
  - post-processing scripts now have two new parameters:
        - env. var "NZBPP_STATUS" indicates the status of download including
          the total status (SUCCESS, FAILURE, etc.) and the detail field
          (for example in case of failures: PAR, UNPACK, etc.);
        - env. var "NZBPP_TOTALSTATUS" is equal to the total status of
          parameter "NZBPP_STATUS" and is provided for convenience (to avoid
          parsing of "NZBPP_STATUS");
        - the new parameters provide a simple way for pp-scripts to determine
          download status without a guess work needed in previous versions;
        - parameters "NZBPP_PARSTATUS" and "NZBPP_UNPACKSTATUS" are now
          considered deprecated (still passed for compatibility);
        - updated script "EMail.py" to use new parameters "NZBPP_TOTALSTATUS"
          and "NZBPP_STATUS" instead of "NZBPP_PARSTATUS" and "NZBPP_UNPACKSTATUS";
  - when changing category in web-interface the post-processing parameters
    are now automatically updated according to new category settings:
        - only parameters which are different in old and new category are changed;
        - parameters which present in both or in neither categories are not changed;
        - that ensures that only the relevant parameters are updated and
          parameters which were manually changed by user remain they settings
          when it make sense;
        - in the "download details dialog" the new parameters are updated
          on the postprocess-tab directly after changing of category and
          can be controlled before saving;
        - in the "edit multiple downloads dialog" the parameters are updated
          individually for each download on saving;
        - new action "CP" of remote command "--edit/-E" for groups to set
          category and apply parameters;
        - new action "GroupApplyCategory of RPC-method "editqueue" for
          the same purpose;
  - changed the way option "ContinuePartial" works:
        - now the information about completed articles is stored in a
          special file in QueueDir;
        - when option "DirectWrite" is active no separate flag-files per
          article are created in TempDir;
        - the file contains additional information, which were not
          stored/available before;
  - improved per-server/per-nzb article completion statistics:
        - the statistics are now available for active downloads in details
          dialog (not only for history);
        - the info on that page is constantly updated as long as the page
          is active (unless refresh is disabled);
        - download age info removed from details dialog to save place
          (it is shown in the download list anyway);
        - if backup news-servers start to be used for nzb-file a badge
          appears in the download list showing the percentage of articles
          downloaded from backup servers;
        - click on the badge opens download details dialog directly on
          the completion page;
        - per-server/per-nzb article completion statistics are now
          available via RPC-method "listgroups" for active downloads
          (not only for "history");
  - improved RPC-API:
        - RPC-method "listgroups" now returns info about post-processing
          similar to info returned by method "postqueue";
        - RPC-method "postqueue" is obsolete now;
        - web-interface requires less requests to NZBGet on each page
          update and it is now easier for third-party developers to obtain
          the info about download and post-processing status (no need to
          merge download queue and post queue);
        - RPC-method "listgroups" now returns new field "Status" making it
          easier for third-party apps to determine the status of download entry;
        - new field "Status" in RPC-method "history" to allow third-party
          apps easier determine the status of an item without inspecting
          status-fields of every processing step;
        - changed web-interface to use new field "Status";
        - method "append" now returns id of added nzb-file or "0" on an error;
        - this makes it easier for third-party apps to track added nzb-files;
        - for backward compatibility with older software expecting a boolean
          result the old version of method "append" is still supported;
        - the new version of method "append" has a different signature
          (order of parameters);
        - parameter "content" can now be either nzb-file content (encoded
          in base 64) or an URL;
        - this makes the method "appendurl" obsolete (still supported
          for compatibility);
        - if an URL was added to queue the queue entry created for fetched
          nzb-file has the same "NZBID" for easier tracking;
  - added force-priorities:
        - downloads with priorities equal to or greater than 900 are
          downloaded and post-processed even if the program is in paused
          state (force mode);
        - in web-interface the combo for choosing priority has new entry
          "force" (priority value 900);
        - new fields "ForcedSizeLo", "ForcedSizeHi" and "ForcedSizeMB"
          returned by RPC-method "status";
        - history items now preserve "NZBID" from queue items; that makes
          the tracking of items across queue and history easier for
          third-party apps;
        - field "NZBID" returned by RPC-method "history" is now available
          for history items of all kinds (NZB, URL, DUP); field "ID" is
          deprecated and should not be used;
  - post-processing scripts which move the whole download into a new
    location can inform the program about new location using command
    "[NZB] DIRECTORY=/new/path", allowing other scripts to process files further;
  - added support for power management on windows to avoid pc going into
    sleep mode during download or post-processing;
  - apostrophe is not considered an invalid file name character anymore;
  - adjusted modules initialization to avoid possible bugs due to delayed
    thread starts;
  - reorganized source code directory structure: created directory "daemon"
    with several subdirectories and put all source code files there;
  - added new option "PropagationDelay", which sets the minimum post age
    to download; newer posts are kept on hold in download queue until
    they get older than the defined delay, after that they are downloaded;
  - download speeds above 1024 KB/s are now indicated in MB/s;
  - data sizes above 1000 GB are now shown as TB in web-interface (instead of GB);
  - splitted files are now joined automatically (again);
  - adjusted modules initialization to avoid possible bugs due to delayed
    thread starts;
  - extended info printed by remote command "nzbget -B dump" (for debug
    purposes);
  - eliminated loop waiting time in queue coordinator on certain
    conditions - may improve performance on very high speed connections;
  - increased few wait intervals which were unnecessary too small;
  - improved error reporting: added error check when closing article
    file for writing and when deleting files or directories;
  - when building nzbget if both OpenSSL and GnuTLS are available now
    using OpenSSL by default (the preferred library can still be selected
    with configure-parameter --with-tlslib=OpenSSL/GnuTLS);
  - windows version is now configured to use OpenSSL instead of GnuTLS;
    windows binaries provided on download page now use OpenSSL;
  - column "age" in web-interface now shows minutes for recent posts
    (instead of "0 h");
  - remote command "-B dump" now can be used also in release (non-debug)
    versions and prints useful debug data as "INFO" instead of "DEBUG";
  - to detect daylight saving activation/deactivation the time zone
    information is now checked every minute if a download is active or
    once in 3 hours if the program is in stand-by; these delays should
    work well with hibernation mode on synology);
  - pp-script "EMail.py" now takes the status of previous pp-scripts
    into account and report a failure if any of the scripts has failed;
  - updated all links to go to new domain (nzbget.com);
  - impoved error reporting if unpacker or par-renamer fail to move files;
  - removed libpar2-patches from NZBGet source tree; the documentation
    now suggests to use the libpar2 version maintained by Debian/Ubuntu
    team, which already includes all necessary patches;
  - removed patches to create libpar2 and libsigc++ project files for
    Visual Studio on Windows, no one needed them anyway;
  - fixed: the program could crash during cleanup if files with invalid
    timestamps were found in the directory (windows only);
  - fixed: RSS feed preview dialog displayed slightly incorrect post
    ages because of the wrong time zone conversion;
  - fixed: sometimes URLs were removed too early from the feed history
    causing them to be detected as "new" and fetched again; if duplicate
    check was not active the same nzb-files could be downloaded again;
  - fixed: strange (damaged?) par2-files could cause a crash during par-renaming;
  - fixed: damaged nzb-files containing multiple par-sets and not having
    enough par-blocks could cause a crash during par-check;
  - fixed: if during par-repair the downloaded extra par-files were damaged
    and the repair was terminated with failure status the post-processing
    scripts were executed twice sometimes;
  - fixed: post-processing scripts were not executed in standalone mode
    ("nzbget /path/to/file.nzb");
  - fixed: renaming or deleting of temporary files could fail, especially
    when options "UnpackPauseQueue" and "ScriptPauseQueue" were not active
    (windows only);
  - fixed: per-server/per-nzb article completion statistics could be
    inaccurate for nzb-files whose download were interrupted by reload/restart;
  - fixed: after deleting servers from config file the program could crash
    on start when loading server volume statistics data from disk;
  - fixed: download speeds above approx. 70 MB/s were not indicated
    correctly in web-interface and by RPC-method "status";
  - fixed: cancelling of active par-job sometimes didn't work;
  - fixed: par-check could hang on renamed and splitted files;
  - fixed: the program could crash during parsing of malformed nzb-files;
  - fixed: errors during loading of queue from disk state may render the
    already loaded parts useless too; now at least these parts of queue are used;
  - fixed: queue was not locked during loading on program start and that
    could cause problems;
  - fixed: data sizes exactly equal to 10, 100, 1000 MB or GB were formatted 
    using 4 digits instead of 3 (one digit after decimal point too much);
  - fixed: if post-processing step "move" failed, the command "post-process
    again" did not try to move again;
  - fixed: nzb-files were sometimes not deleted from NzbDir (option
    "NzbCleanupDisk");
  - fixed: scheduler command "FetchFeed" did not work properly with
    parameter "0" (fetch all feeds).
  - fixed: port number was not sent in headers when downloading from URLs
    which could cause issues with RSS for web-sites using non-standard http ports;
  - fixed: queued nzb-files was not deleted from disk when deleting
    download without history tracking;

nzbget-12.0:
  - added RSS feeds support:
        - new options "FeedX.Name", "FeedX.URL", "FeedX.Filter",
          "FeedX.Interval", "FeedX.PauseNzb", "FeedX.Category",
          "FeedX.Priority" (section "Rss Feeds");
        - new option "FeedHistory" (section "Download Queue");
        - button "Preview Feed" on settings tab near each feed definition;
        - new toolbar button "Feeds" on downloads tab with menu to
          view feeds or fetch new nzbs from all feeds (the button is
          visible only if there are feeds defined in settings);
        - new dialog to see feed content showing status of each item (new,
          fetched, backlog) with ability to manually fetch selected items;
        - powerful filters for RSS feeds;
        - new dialog to build filters in web-interface with instant preview;
  - added download health monitoring:
        - health indicates download status, whether the file is damaged
          and how much;
        - 100% health means no download errors occurred; 0% means all
          articles failed;
        - there is also a critical health which is calculated for each
          nzb-file based on number and size of par-files;
        - if during download the health goes down below 100% a health
          badge appears near download name indicating the necessity of
          par-repair; the indicator can be orange (repair may be possible)
          or red (unrepairable) if the health goes down below critical health;
        - new option "HealthCheck" to define what to do with unhealthy
          (unrepairable) downloads (pause, delete, none);
        - health and critical health are displayed in download-edit dialog;
          health is displayed in history dialog; if download was aborted
          (HealthCheck=delete) this is indicated in history dialog;
        - health allows to determine download status for downloads which
          have unpack and/or par-check disabled; for such downloads the
          status in history is shown based on health: success (health=100%),
          damaged (health > critical) or failure (health < critical);
        - par-check is now automatically started for downloads having
          health below 100%; this works independently of unpack (even if
          unpack is disabled);
        - for downloads having health less than critical health no par-check
          is performed (it would fail); Instead the par-check status is
          set to "failure" automatically saving time of actual par-check;
        - new fields "Health" and "CriticalHealth" are returned by
          RPC-Method "listgroups";
        - new fields "Health", "CriticalHealth", "Deleted" and "HealthDeleted"
          are returned by RPC-Method "history";
        - new parameters "NZBPP_HEALTH" and "NZBPP_CRITICALHEALTH" are passed
          to pp-scripts;
  - added collecting of server usage statistical data for each download:
        - number of successful and failed article downloads per news server;
        - new page in history dialog shows collected statistics;
        - new fields in RPC-method "history": ServerStats (array),
          TotalArticles, SuccessArticles, FailedArticles;
        - new env. vars passed to pp-scripts: NZBPP_TOTALARTICLES,
          NZBPP_SUCCESSARTICLES, NZBPP_FAILEDARTICLES and per used news
          server: NZBPP_SERVERX_SUCCESSARTICLES, NZBPP_SERVERX_FAILEDARTICLES;
        - also new env.var HEALTHDELETED;
  - added smart duplicates feature:
        - mostly for use with RSS feeds;
        - automatic detection of duplicate nzb-files to avoid download of
          duplicates;
        - nzb-files can be also manually marked as duplicates;
        - if download fails - automatically choose another release (duplicate);
        - if download succeeds all remaining duplicates are skipped (not downloaded);
        - download items have new properties to tune duplicate handling
          behavior: duplicate key, duplicate score and duplicate mode;
        - if download was deleted by duplicate check its status in the
          history is shown as "DUPE";
        - new actions "GroupSetDupeKey", "GroupSetDupeScore", "GroupSetDupeMode",
          "HistorySetDupeKey", "HistorySetDupeScore", "HistorySetDupeMode",
          "HistoryMarkBad" and "HistoryMarkGood" of RPC-command "editqueue";
          new actions "B" and "G" of command "--edit/-E" for history items
          (subcommand "H");
        - when deleting downloads from queue there are three options now:
          "move to history", "move to history as duplicate" and "delete
          without history tracking";
        - new actions "GroupDupeDelete", "GroupFinalDelete" and
          "HistorySetDupeBackup" in RPC-method "editqueue";
        - RPC-commands "listgroups", "postqueue" and "history" now return
          more info about nzb-item (many new fields);
        - removed option "MergeNzb" because it conflicts with duplicate
          handling, items can be merged manually if necessary;
        - automatic detection of exactly same nzb-files (same content)
          coming from different sources (different RSS feeds etc.);
          individual files (inside nzb-file) having extensions listed in
          option "ExtCleanupDisk" are excluded from content comparison
          (unless these are par2-files, which are never excluded);
        - when history item expires (as defined by option "KeepHistory")
          and the duplicate check is active (option "DupeCheck") the item
          is not completely deleted from history; instead the amount of
          stored data reduces to minimum required for duplicate check
          (about 200 bytes vs 2000 bytes for full history item);
        - such old history items are not shown in web-interface by default
          (to avoid transferring of large amount of history items);
        - new button "Hidden" in web-interface to show hidden history items;
          the items are marked with badge "hidden";
        - RPC-method "editqueue" has now two actions to delete history
          records: "HistoryDelete", "HistoryFinal"; action "HistoryDelete"
          which has existed before now hides records, already hidden records
          are ignored; 
        - added functions "Mark as Bad" and "Mark as Good" for history
          items;
        - duplicate properties (dupekey, dupescore and dupemode) can now
          be viewed and changed in download-edit-dialog and
          history-edit-dialog via new button "Dupe";
        - for full documentation see https://nzbget.com/documentation/rss/#duplicates;
  - created NZBGet.app - NZBGet is now a user friendly Mac OS X application
          with easy installation and seamless integration into OS UI:
          works in background, is controlled from a web-browser, few
          important functions are accessible via menubar icon;
  - better Windows package:
        - unrar is included;
        - several options are set to better defaults;
        - all paths are set as relative paths to program directory;
          the program can be started after installation without editing
          anything in config;
        - included two new batch-files:
              - nzbget-start.bat - starts program in normal mode (dos box);
              - nzbget-recovery-mode.bat - starts with empty password (dos box);
              - both batch files open browser window with correct address;
        - config-file template is stored in nzbget.conf.template;
        - nzbget.conf is not included in the package. When the program is
          started for the first time (using one of batch files) the template
          config file is copied into nzbget.conf;
        - updates will be easy in the future: to update the program all
          files from newer archive must be extracted over old files. Since
          the archive doesn't have nzbget.conf, the existing config is kept
          unmodified. The template config file will be updated;
        - added file README-WINDOWS.txt with small instructions;
        - version string now includes revision number (like "r789");
  - added automatic updates:
        - new button "Check for updates" on settings tab of web-interface,
          in section "SYSTEM", initiates check and shows dialog allowing to
          install new version;
        - it is possible to choose between stable, testing and development
          branches;
        - this feature is for end-users using binary packages created and
          updated by maintainers, who need to write an update script specific
          for platform;
        - the script is then called by NZBGet when user clicks on install-button;
        - the script must download and install new version;
        - for more info visit https://nzbget.com/documentation/packaging/;
  - news servers can now be temporarily disabled via speed limit dialog
    without reloading of the program:
        - new option "ServerX.Active" to disable servers via settings;
        - new option "ServerX.Name" to use for logging and in UI;
  - changed the way how option "Unpack" works:
        - instead of enabling/disabling the unpacker as a whole, it now
          defines the initial value of post-processing parameter "Unpack"
          for nzb-file when it is added to queue;
        - this makes it now possible to disable Unpack globally but still
          enable it for selected nzb-files;
        - new option "CategoryX.Unpack" to set unpack on a per category basis;
  - combined all footer buttons into one button "Actions" with menu:
        - in download-edit-dialog: "Pause/Resume", "Delete" and "Cancel
          Post-Processing";
        - in history-dialog: "Delete", "Post-Process Again" and "Download
          Remaining Files (Return to Queue)";
  - DirectNZB headers X-DNZB-MoreInfo and X-DNZB-Details are now processed
    when downloading URLs and the links "More Info" and "Details" are shown
    in download-edit-dialog and in history-dialog in Actions menu; 
  - program can now be stopped via web-interface: new button "shutdown"
    in section "SYSTEM";
  - added menu "View" to settings page which allows to switch to "Compact Mode"
    when option descriptions are hidden;
  - added confirmation dialog by leaving settings page if there are unsaved
    changes;
  - downloads manually deleted from queue are shown with status "deleted"
    in the history (instead of "unknown"); 
  - all table columns except "Name" now have fixed widths to avoid annoying
    layout changes especially during post-processing when long status messages
    are displayed in the name-column;
  - added filter buttons to messages tab (info, warning, etc.);
  - added automatic par-renaming of extracted files if archive includes
    par-files;
  - added support for http redirects when fetching URLs;
  - added new command "Download again" for history items; new action
    "HistoryRedownload" of RPC-method "editqueue"; for controlling via command
    line: new action "A" of subcommand "H" of command "--edit/-E";
  - download queue is now saved in a more safe way to avoid potential loss
    of queue if the program crashes during saving of queue;
  - destination directory for option "CategoryX.DestDir" is not checked/created
    on program start anymore (only when a download starts for that category);
    this helps when certain categories are configured for external disks,
    which are not always connected;
  - added new option "CategoryX.Aliases" to configure category name matching
    with nzb-sites; especially useful with rss feeds;
  - in RPC-Method "appendurl" parameter "addtop" adds nzb to the top of
    the main download queue (not only to the top of the URL queue);
  - new logo (thanks to dogzipp for the logo);
  - added support for metatag "password" in nzb-files;
  - pp-scripts which move files can now inform the program about new
    location by printing text "[NZB] FINALDIR=/path/to/files"; the final
    path is then shown in history dialog instead of download path;
  - new env-var "NZBPP_FINALDIR" passed to pp-scripts;
  - pp-scripts can now set post-processing parameters by printing
    command "[NZB] NZBPR_varname=value"; this allows scripts which are
    executed sooner to pass data for scripts executed later;
  - added new option "AuthorizedIP" to set the list of IP-addresses which
    may connect without authorization;
  - new option "ParRename" to force par-renaming as a first post-processing
    step (active by default); this saves an unpack attempt and is even more
    useful if unpack is disabled;
  - post-processing progress label is now automatically trimmed if it
    doesn't fill into one line; this avoids layout breaking if the text
    is too long;
  - reversed the order of priorities in comboboxes in dialogs: the highest
    priority - at the top, the lowest - at the bottom;
  - small changes in button captions: edit dialogs called from settings
    page (choose script, choose order, build rss filter) now have buttons
    "Discard/Apply" instead of "Close/Save"; in all other dialogs button
    "Close" renamed to "Cancel" unless it was the only button in dialog;
  - small change in css: slightly reduced the max height of modal dialogs
    to better work on notebooks;
  - options "DeleteCleanupDisk" and "NzbCleanupDisk" are now active by
    default (in the example config file);
  - extended add-dialog with options "Add paused" and "Disable duplicate check";
  - source nzb-files are now deleted when download-item leaves queue and
    history (option "NzbCleanupDisk");
  - when deleting downloads from queue the messages about deleted
    individual files are now printed as "detail" instead of "info";
  - failed article downloads are now logged as "detail" instead of
    "warning" to reduce number of warnings for downloads removed from
    server (DMCA); one warning is printed for a file with a summary of
    number of failed downloads for the file;
  - tuned algorithm calculating maximum threads limit to allow more
    threads for backup server connections (related to option "TreadLimit"
    removed in v11); this may sometimes increase speed when backup servers
    were used;
  - by adding nzb-file to queue via RPC-methods "append" and "appendurl"
    the actual format of the file is checked and if nzb-format is detected
    the file is added even if it does not have .nzb extension;
  - added new option "UrlForce" to allow URL-downloads (including fetching
    of RSS feeds and nzb-files from feeds) even if download is paused;
    the option is active by default;
  - destination directory for option "DestDir" is not checked/created on
    program start anymore (only when a download starts); this helps when
    DestDir is mounted to a network drive which is not available on program start;
  - added special handling for files ".AppleDouble" and ".DS_Store" during
    unpack to avoid problems on NAS having support for AFP protocol (used
    on Mac OS X);
  - history records with failed script status are now shown as "PP-FAILURE"
    in history list (instead of just "FAILURE");
  - option "DiskSpace" now checks space on "InterDir" in addition to
    "DestDir";
  - support for rar-archives with non-standard extensions is now limited
    to file extensions consisting of digits; this is to avoid extracting
    of rar-archives having non-rar extensions on purpose (example: .cbr);
  - if option "ParRename" is disabled (not recommended) unpacker does
    not initiate par-rename anymore, instead the full par-verify is
    performed then;
  - for external script the exec-permissions are now added automatically;
    this makes the installation of pp-scripts and other scripts easier;
  - option "InterDir" is now active by default;
  - when option "InterDir" is used the intermediate destination directory
    names now include unique numbers to avoid several downloads with same
    name to use the same directory and interfere with each other;
  - when option "UnpackCleanupDisk" is active all archive files are now
    deleted from download directory without relying on output printed by
    unrar; this solves issues with non-ascii-characters in archive file
    names on some platforms and especially in combination with rar5;
  - improved handling of non-ascii characters in file names on windows;
  - added support for rar5-format when checking signatures of archives
    with non-standard file extensions;
  - small restructure in settings order:
        - combined sections "REMOTE CONTROL" and "PERMISSIONS" into one
          section with name "SECURITY";
        - moved sections "CATEGORIES" and "RSS FEEDS" higher in the
          section list;
  - improved par-check: if main par2-file is corrupted and can not be
    loaded other par2-files are downloaded and then used as replacement
    for main par2-file;
  - if unpack did not find archive files the par-check is not requested
    anymore if par-rename was already done;
  - better handling of obfuscated nzb-files containing multiple files
    with same names; removed option "StrictParName" which was not working
    good with obfuscated files; if more par-files are required for repair
    the files with strict names are tried first and then other par-files;
  - added new scheduler commands "ActivateServer", "DeactivateServer" and
    "FetchFeed"; combined options "TaskX.DownloadRate" and "TaskX.Process"
    into one option "TaskX.Param", also used by new commands;
  - added status filter buttons to history page;
  - if unpack fails with write error (usually because of not enough space
    on disk) this is shown as status "Unpack: space" in web-interface;
    this unpack-status is handled as "success" by duplicate handling
    (no download of other duplicate); also added new unpack-status "wrong
    password" (only for rar5-archives); env.var. NZBPP_UNPACKSTATUS has
    two new possible values: 3 (write error) and 4 (wrong password);
    updated pp-script "EMail.py" to support new unpack-statuses;
  - fixed a potential seg. fault in a commonly used function;
  - added new option "TimeCorrection" to adjust conversion from system
    time to local time (solves issues with scheduler when using a
    binary compiled for other platform);
  - NZBIDs are now generated with more care avoiding numbering holes
    possible in previous versions;
  - fixed: invalid "Offset" passed to RPC-method "editqueue" or command
    line action "-E/--edit" could crash the program;
  - fixed: crash after downloading of an URL (happen only on certain systems);
  - fixed: restoring of settings didn't work for multi-sections (servers,
    categories, etc.) if they were empty;
  - fixed: choosing local files didn't work in Opera;
  - fixed: certain characters printed by pp-scripts could crash the
    program;
  - fixed: malformed nzb-file could cause a memory leak;
  - fixed: when a duplicate file was detected in collection it was
    automatically deleted (if option DupeCheck is active) but the
    total size of collection was not updated;
  - when deleting individual files the total count of files in collection
    was not updated;
  - fixed: when multiple nzb-files were added via URL (rss including) at
    the same time the info about category and priority could get lost for
    some of files;
  - fixed: if unpack fails the created destination directory was not
    automatically removed (only if option "InterDir" was active);
  - fixed scrolling to the top of page happening by clicking on items in
    downloads/history lists and on action-buttons in edit-download and
    history dialogs;
  - fixed potential buffer overflow in remote client;
  - improved error reporting when creation of temporary output file fails;
  - fixed: when deleting download, if all remaining queued files are
    par2-files the disk cleanup should not be performed, but it was
    sometimes;
  - fixed a potential problem in incorrect using of one library function.

nzbget-11.0:
  - reworked concept of post-processing scripts:
        - multiple scripts can be assigned to each nzb-file;
        - all assigned scripts are executed after the nzb-file is
          downloaded and internally processed (unpack, repair);
        - option <PostProcess> is obsolete;
        - new option <ScriptDir> sets directory where all pp-scripts must
          be stored;
        - new option <DefScript> sets the default list of pp-scripts to
          be assigned to nzb-file when it's added to queue;
        - new option <CategoryX.DefScript> to set the default list of
          pp-scripts on a category basis;
        - the execution order of pp-scripts can be set using new option
          <ScriptOrder>;
        - there are no separate configuration files for pp-scripts;
        - configuration options and pp-parameters are defined in the
          pp-scripts;
        - script configuration options are saved in nzbget configuration
          file (nzbget.conf);
        - changed parameters list of RPC-methods <loadconfig> and
          <saveconfig>;
        - new RPC-method <configtemplates> returns configuration
          descriptions for the program and for all pp-scripts;
        - configuration of all scripts can be done in web-interface;
        - the pp-scripts assigned to a particular nzb-file can be viewed
          and changed in web-interface on page <pp-parameters> in the
          edit download dialog;
        - option <PostPauseQueue> renamed to <ScriptPauseQueue> (the old
          name is still recognized);
        - new option <ConfigTemplate> to define the location of template
          configuration file (in previous versions it must be always
          stored in <WebDir>);
        - history dialog shows status of every script;
  - the old example post-processing script replaced with two new scripts:
        - EMail.py - sends E-Mail notification;
        - Logger.py - saves the full post-processing log of the job into
          file _postprocesslog.txt;
        - both pp-scripts are written in python and work on Windows too
          (in addition to Linux, Mac, etc.);
  - added possibility to set post-processing parameters for history items:
        - pp-parameters can now be viewed and changed in history dialog
          in web-interface;
        - useful before post-processing again;
        - new action <HistorySetParameter> in RPC-method <editqueue>;
        - new action <O> in remote command <--edit/-E> for history items
          (subcommand <H>);
  - added new feature <split download> which creates new download from
    selected files of source download;
        - new command <Split> in web-interface in edit download dialog
          on page <Files>;
        - new action `<S>` in remote command <--edit/-E>;
        - new action <FileSplit> in JSON-/XML-RPC method <editqueue>;
  - added support for manual par-check:
        - if option <ParCheck> is set to <Manual> and a damaged download
          is detected the program downloads all par2-files but doesn't
          perform par-check; the user must perform par-check/repair
          manually then (possibly on another, faster computer);
        - old values <yes/no> of option <ParCheck> renamed to <Force>
          and <Auto> respectively;
        - when set to <Force> all par2-files are always downloaded;
        - removed option <LoadPars> since its functionality is now
          covered by option <ParCheck>;
        - result of par-check can now have new value <Manual repair
          necessary>;
        - field <ParStatus> in RPC-method <history> can have new value
          <MANUAL>;
        - parameter <NZBPP_PARSTATUS> for pp-script can have new value
          <4 = manual repair necessary>;
  - when download is resumed in web-interface the option <ParCheck=Force>
    is respected and all par2-files are resumed (not only main par2-file);
  - automatic deletion of backup-source files after successful par-repair;
    important when repairing renamed rar-files since this could cause
    failure during unpack;
  - par-checker and renamer now add messages into the log of pp-item
    (like unpack- and pp-scripts-messages); these message now appear in
    the log created by scripts Logger.py and EMail.py;
  - when a nzb-file is added via web-interface or via remote call the
    file is now put into incoming nzb-directory (option "NzbDir") and
    then scanned; this has two advantages over the old behavior when the
    file was parsed directly in memory:
        - the file serves as a backup for troubleshootings;
        - the file is processed by nzbprocess-script (if defined in
          option "NzbProcess") making the pre-processing much easier;
  - new env-var parameters are passed to NzbProcess-script: NZBNP_NZBNAME,
    NZBNP_CATEGORY, NZBNP_PRIORITY, NZBNP_TOP, NZBNP_PAUSED;
  - new commands for use in NzbProcess-scripts: "[NZB] TOP=1" to add nzb
    to the top of queue and "[NZB] PAUSED=1" to add nzb-file in paused state;
  - reworked post-processor queue:
        - only one job is created for each nzb-file; no more separate
          jobs are created for par-collections within one nzb-file;
        - option <AllowReProcess> removed; a post-processing script is
          called only once per nzb-file, this behavior cannot be altered
          anymore;
        - with a new feature <Split> individual par-collections can be
          processed separately in a more effective way than before
  - improved unicode (utf8) support:
        - non-ascii characters are now correctly transferred via JSON-RPC;
        - correct displaying of nzb-names and paths in web-interface;
        - it is now possible to use non-ascii characters on settings page
          for option values (such as paths or category names);
        - improved unicode support in XML-RPC and JSON-RPC;
  - if username and password are defined for a news-server the
    authentication is now forced (in previous versions the authentication
    was performed only if requested by server); needed for servers
    supporting both anonymous (restricted) and authorized (full access)
    accounts;
  - added option <ExtCleanupDisk> to automatically delete unwanted files
    (with specified extensions or names) after successful par-check or unpack;
  - improvement in JSON-/XML-RPC:
        - all ID fields including NZBID are now persistent and remain
          their values after restart;
        - this allows for third-party software to identify nzb-files by
          ID;
        - method <history> now returns ID of NZB-file in the field
          <NZBID>;
        - in versions up to 0.8.0 the field <NZBID> was used to identify
          history items in the edit-commands <HistoryDelete>,
          <HistoryReturn>, <HistoryProcess>; since version 9 field <ID>
          is used for this purpose; in versions 9-10 field <NZBID> still
          existed and had the same value as field <ID> for compatibility
          with version 0.8.0; the compatibility is not provided anymore;
          this change was needed to provide a consistent using of field
          <NZBID> across all RPC-methods;
  - added support for rar-files with non-standard extensions (such as
    .001, etc.);
  - added functions to backup and restore settings from web-interface;
    when restoring it's possible to choose what sections to restore
    (for example only news servers settings or only settings of a
    certain pp-script) or restore the whole configuration;
  - new option "ControlUsername" to define login user name (if you don't
    like default username "nzbget");
  - if a communication error occurs in web-interface, it retries multiple
    times before giving up with an error message;
  - the maximum number of download threads are now managed automatically
    taking into account the number of allowed connections to news servers;
    removed option <ThreadLimit>;
  - pp-scripts terminated with unknown status are now considered failed
    (status=FAILURE instead of status=UNKNOWN);
  - new parameter (env. var) <NZBPP_NZBID> is passed to pp_scripts and
    contains an internal ID of NZB-file;
  - improved thread synchronisation to avoid (short-time) lockings of
    the program during creation of destination files;
  - more detailed error message if a directory could not be created
    (<DstDir>, <NzbDir>, etc.); the message includes error text reported
    by OS such as <permission denied> or similar;
  - when unpacking the unpack start time is now measured after receiving
    of unrar copyright message; this provides better unpack time
    estimation in a case when user uses unpack-script to do some things
    before executing unrar (for example sending Wake-On-Lan message to
    the destination NAS); it works with unrar only, it's not possible
    with 7-Zip because it buffers printed messages;
  - when the program is reloaded, a message with version number is
    printed like on start;
  - configuration can now be saved in web-interface even if there were
    no changes made but if obsolete or invalid options were detected in
    the config file; the saving removes invalid entries from config file;
  - option <ControlPassword> can now be set to en empty value to disable
    authentication; useful if nzbget works behind other web-server with
    its own authentication;
  - when deleting downloads via web-interface a proper hint regarding
    deleting of already downloaded files from disk depending on option
    <DeleteCleanupDisk> is displayed;
  - if a news-server returns empty or bad article (this may be caused
    by errors on the news server), the program tries again from the same
    or other servers (in previous versions the article was marked as
    failed without other download attempts);
  - when a nzb-file whose name ends with ".queued" is added via web-
    interface the ".queued"-part is automatically removed;
  - small improvement in multithread synchronization of download queue;
  - added link to catalog of pp-scripts to web-interface;
  - updated forum URL in about dialog in web-interface;
  - small correction in a log-message: removed <Request:> from message
    <Request: Queue collection...>;
  - removed option "ProcessLogKind"; scripts should use prefixes ([INFO],
    [DETAIL], etc); messages printed without prefixes are added as [INFO];
  - removed option "AppendNzbDir"; if it was disabled that caused problems 
    in par-checker and unpacker; the option is now assumed always active;
  - removed option "RenameBroken"; it caused problems in par-checker
    (the option existed since early program versions before the par-check
    was added);
  - configure-script now defines "SIGCHLD_HANDLER" by default on all
    systems including BSD; this eliminates the need of configure-
    parameter "--enable-sigchld-handler" on 64-Bit BSD; the trade-off:
    32-Bit BSD now requires "--disable-sigchld-handler";
  - improved configure-script: defining of symbol "FILE_OFFSET_BITS=64",
    required on some systems, is not necessary anymore;
  - fixed: in the option "NzbAddedProcess" the env-var parameter with
    nzb-name was passed in "NZBNA_NAME", should be "NZBNA_NZBNAME";
    the old parameter name "NZBNA_NAME" is still supported for
    compatibility;
  - fixed: download time in statistics were incorrect if the computer
    was put into standby (thanks Frank Kuypers for the patch);
  - fixed: when option <InterDir> was active and the download after
    unpack contained rar-file with the same name as one of original
    files (sometimes happen with included subtitles) the original
    rar-file was kept with name <.rar_duplicate1> even if the option
    <UnpackCleanupDisk> was active;
  - fixed: failed to read download queue from disk if post-processing
    queue was not empty;
  - fixed: when a duplicate file was detected during download the
    program could hang;
  - fixed: symbol <DISABLE_TLS> must be defined in project settings;
    defining it in <win32.h> didn't work properly (Windows only);
  - fixed: crash when adding malformed nzb-files with certain
    structure (Windows only);
  - fixed: by deleting of a partially downloaded nzb-file from queue,
    when the option <DeleteCleanupDisk> was active, the file
    <_brokenlog.txt> was not deleted preventing the directory from
    automatic deletion;
  - fixed: if an error occurs when a RPC-client or web-browser
    communicates with nzbget the program could crash;
  - fixed: if the last file of collection was detected as duplicate
    after the download of the first article the file was deleted from
    queue (that's OK) but the post-processing was not triggered
    (that's a bug);
  - fixed: support for splitted files (.001, .002, etc.) were broken.

nzbget-10.2:
  - fixed potential segfault which could happen with file paths longer
    than 1024 characters;
  - fixed: when options <DirectWrite> and <ContinuePartial> were both
    active, a restart or reload of the program during download may cause
    damaged files in the active download;
  - increased width of speed indication ui-element to avoid layout
    breaking on some linux-browsers;
  - fixed a race condition in unpacker which could lead to a segfault
    (although the chances were low because the code wasn't executed often).

nzbget-10.1:
  - fixed: articles with decoding errors (incomplete or damaged posts)
    caused infinite retry-loop in downloader.

nzbget-10.0:
  - added built-in unpack:
        - rar and 7-zip formats are supported (via external Unrar and
          7-Zip executables);
        - new options <Unpack>, <UnpackPauseQueue>, <UnpackCleanupDisk>,
          <UnrarCmd>, <SevenZipCmd>;
        - web-interface now shows progress and estimated time during
          unpack (rar only; for 7-Zip progress is not available due to 
          limitations of 7-Zip);
        - when built-in unpack is enabled, the post-processing script is
          called after unpack and possibly par-check/repair (if needed);
        - for nzb-files containing multiple collections (par-sets) the
          post-processing script is called only once, after the last
          par-set;
        - new parameter <NZBPP_UNPACKSTATUS> passed to post-processing
          script;
        - if the option <AllowReProcess> is enabled the post-processing-
          script is called after each par-set (as in previous versions);
        - example post-processing script updated: removed unrar-code, 
          added check for unpack status;
        - new field <UnpackStatus> in result of RPC-method <history>;
        - history-dialog in web-interface shows three status: par-status,
          unpack-status, script-status;
        - with two built-in special post-processing parameters <*Unpack:>
          and <*Unpack:Password> the unpack can be disabled for individual
          nzb-file or the password can be set;
        - built-in special post-processing parameters can be set via web-
          interface on page <PP-Parameters> (when built-in unpack is
          enabled);
  - added support for HTTPS to the built-in web-server (web-interface and 
    XML/JSON-RPC):
        - new options <SecureControl>, <SecurePort>, <SecureCert> and 
          <SecureKey>;
        - module <TLS.c/h> completely rewritten with support for server-
          side sockets, newer versions of GnuTLS, proper thread lockings
          in OpenSSL;
  - improved the automatic par-scan (option <ParScan=auto>) to 
    significantly reduce the verify-time in some common cases with renamed
    rar-files:
        - the extra files are scanned in an optimized order;
        - the scan stops when all missings files are found;
  - added fast renaming of intentionally misnamed (rar-) files:
        - the new renaming algorithm doesn't require full par-scan and 
          restores original filenames in just a few seconds, even on very
          slow computers (NAS, media players, etc.);
        - the fast renaming is performed automatically when requested by
          the built-in unpacker (option <Unpack> must be active);
  - added new option <InterDir> to put intermediate files during download
    into a separate directory (instead of storing them directly in 
    destination directory (option <DestDir>):
        - when nzb-file is completely (successfully) downloaded, repaired 
          (if neccessary) and unpacked the files are moved to destination 
          directory (option <DestDir> or <CategoryX.DestDir>);
        - intermediate directory can significantly improve unpack 
           performance if it is located on a separate physical hard drive;
  - added new option <ServerX.Cipher> to manually select cipher for 
    encrypted communication with news server:
        - manually choosing a faster cipher (such as <RC4>) can 
          significantly improve performance (if CPU is a limiting factor);
  - major improvements in news-server/connection management (main and fill
    servers):
        - if download of article fails, the program tries all servers of 
          the same level before trying higher level servers;
        - this ensures that fill servers are used only if all main servers
          fail;
        - this makes the configuring of multiple servers much easier than
          before: in most cases the simple configuration of level 0 for 
          all main servers and level 1 for all fill servers suffices;
        - in previous versions the level was increased immediately after
          the first tried server of the level failed; to make sure all 
          main servers were tried before downloading from fill servers it
          was required to create complex server configurations with 
          duplicates; these configurations were still not as effective as
          now;
        - do not reconnect on <article/group not found> errors since this
          doesn't help but unnecessary increases CPU load and network 
          traffic;
        - removed option <RetryOnCrcError>; it's not required anymore; 
        - new option <ServerX.Group> allows more flexible configuration 
          of news servers when using multiple accounts on the same server;
          with this option it's also possible to imitate the old server 
          management behavior regarding levels;
  - news servers configuration is now less error-prone:
        - the option <ServerX.Level> is not required to start from <0> and
          when several news servers are configured the Levels can be any
          integers - the program sorts the servers and corrects the Levels
          to 0,1,2,etc. automatically if needed;
        - when option <ServerX.Connections> is set to <0> the server is 
          ignored (in previous version such a server could cause hanging
          when the program was trying to go to the next level);
        - if no news servers are defined (or all definitions are invalid) 
          a warning is printed to inform that the download is not 
          possible;
  - categories can now have their own destination directories; new option
    <CategoryX.DestDir>;
  - new feature <Pause for X Minutes> in web-interface; new XML-/JSON-RPC
    method <scheduleresume>;
  - improved the handling of hanging connections: if a connection hangs 
    longer than defined by option <ConnectionTimeout> the program tries to
    gracefully close connection first (this is new); if it still hangs 
    after <TerminateTimeout> the download thread is terminated as a last 
    resort (as in previous versions);
  - added automatic speed meter recalibration to recover after possible
    synchronization errors which can occur when the option <AccurateRate>
    is not active; this makes the default (less accurate but fast) speed
    meter almost as good as the accurate one; important when speed 
    throttling is active;
  - when the par-checked requests more par-files, they get an extra 
    priority and are downloaded before other files regardless of their 
    priorities; this is needed to avoid hanging of par-checker-job if a 
    file with a higher priority gets added to queue during par-check;
  - when post-processing-parameters are passed to the post-processing 
    script a second version of each parameter with a normalized parameter-
    name is passed in addition to the original parameter name; in the 
    normalized name the special characters <*> and <:> are replaced with
    <_> and all characters are passed in upper case; this is important for
    internal post-processing-parameters (*Unpack:=yes/no) which include
    special characters;
  - warning <Non-nzbget request received> now is not printed when the 
    connection was aborted before the request signature was read;
  - changed formatting of remaining time for post-processing to short 
    format (as used for remaining download time);
  - added link to article <Performance tips> to settings tab on web-
    interface;
  - removed hint <Post-processing script may have moved files elsewhere> 
    from history dialog since it caused more questions than helped;
  - changed default value for option <ServerX.JoinGroup> to <no>; most 
    news servers nowadays do not require joining the group and many 
    servers do not keep headers for many groups making the join-command
    fail even if the articles still can be successfully downloaded;
  - small change in example post-processing script: message <Deleting 
    source ts-files> are now printed only if ts-files really existed;
  - improved configure-script:
        - libs which are added via pkgconfig are now put into LIBS instead
          of LDFLAGS - improves compatibility with newer Linux linkers;
        - OpenSSL libs/includes are now added using pkgconfig to better 
          handle dependencies;
        - additional check for libcrypto (part of OpenSSL) ensures the 
          library is added to linker command even if pkgconfig is not 
          used;
  - adding of local files via web-interface now works in IE10;
  - if an obsolete option is found in the config file a warning is printed
    instead of an error and the program is not paused anymore;
  - fixed: the reported line numbers for configuration errors were 
    sometimes inaccurate;
  - fixed warning <file glyphicons-halflings.png not found>;
  - fixed: some XML-/JSON-RPC methods may return negative values for file
    sizes between 2-4GB; this had also influence on web-interface.
  - fixed: if an external program (unrar, pp-script, etc.) could not be
    started, the execute-function has returned code 255 although the code
    -1 were expected in this case; this could break designed post-
    processing flow;
  - fixed: some characters with codes below 32 were not properly encoded
    in JSON-RPC; sometimes output from unrar contained such characters 
    and could break web-interface;
  - fixed: special characters (quotation marks, etc.) in unpack password
    and in configuration options were not displayed properly and could be
    discarded on saving;

nzbget-9.1:
  - added full par-scan feature needed to par-check/repair files which
    were renamed after creation of par-files:
  - new option <ParScan> to activate full par-scan (always or automatic);
    the automatic full par-scan activates if missing files are detected
    during par-check, this avoids unnecessary full scan for normal
    (not renamed) par sets;
  - improved the post-processing script to better handle renamed rar-files;
  - replaced a browser error message when trying to add local files in
    IE9 with a better message dialog;

nzbget-9.0:
  - changed version naming scheme by removing the leading zero: current
    version is now called 9.0 instead of 0.9.0 (it's really the 9th major
    version of the program);
  - added built-in web-interface:
        - completely new designed and written from scratch;
        - doesn't require a separate web-server;
        - doesn't require PHP;
        - 100% Javascript application; the built-in web-server hosts only
          static files; the javascript app communicates with NZBGet via
          JSON-RPC;
        - very efficient usage of server resources (CPU and memory);
        - easy installation. Since neither a separate web-server nor PHP
          are needed the installation of new web-interface is very easy.
          Actually it is performed automatically when you "make install"
          or "ipkg install nzbget";
        - modern look: better layout, popup dialogs, nice animations,
          hi-def icons;
        - built-in phone-theme (activates automatically);
        - combined view for "currently downloading", "queued", "currently
          processing" and "queued for processing";
        - renaming of nzb-files;
        - multiselect with multiedit or merge of downloads;
        - fast paging in the lists (downloads, history, messages);
        - search box for filtering in the lists (downloads, history, messages)
          and in settings;
        - adding nzb-files to download queue was improved in several ways:
            - add multiple files at once. The "select files dialog" allows
              to select multiple files;
            - add files using drag and drop. Just drop the files from your
              file manager directly into the web-browser;
            - add files via URLs. Put the URL and NZBGet downloads the
              nzb-file and adds it to download queue automatically;
            - the priority of nzb-file can now be set when adding local-files
              or URLs;
  - the history can be cleared completely or selected items can be removed;
  - file mode is now nzb-file related;
  - added the ability to queue URLs:
        - the program automatically downloads nzb-files from given URLs
          and put them to download queue.
        - when multiple URLs are added in a short time, they are put
          into a special URL-queue.
        - the number of simultaneous URL-downloads are controlled via
          new option UrlConnections.
        - with the new option ReloadUrlQueue can be controlled if the URL-queue
          should be reloaded after the program is restarted (if the URL-queue
          was not empty).
        - new switch <-U> for remote-command <--append/-A> to queue an URL.
        - new subcommand <-U> in the remote command <--list/-L> prints the
          current URL-queue.
        - if URL-download fails, the URL is moved into history.
        - with subcommand <-R> of command <--edit> the failed URL can be
          returned to URL-queue for redownload.
        - the remote command <--list/-L> for history can now print the infos
          for URL history items.
        - new XML/JSON-RPC command <appendurl> to add an URL or multiple
          URLs for download.
        - new XML/JSON-RPC command <urlqueue> returns the items from the
          URL-queue.
        - the XML/JSON-RPC command <history> was extended to provide
          infos about URL history items.
        - the URL-queue obeys the pause-state of download queue.
        - the URL-downloads support HTTP and HTTPS protocols;
  - added new field <name> to nzb-info-object.
        - it is initially set to the cleaned up name of the nzb-file.
        - the renaming of the group changes this field.
        - all RPC-methods related to nzb-object return the new field, the
          old field <NZBNicename> is now deprecated.
        - the option <MergeNZB> now checks the <name>-field instead of
          <nzbfilename> (the latter is not changed when the nzb is renamed).
        - new env-var-parameter <NZBPP_NZBNAME> for post-processing script;
  - added options <GN> and <FN> for remote command <--edit/-E>. With these
    options the name of group or file can be used in edit-command instead
    of file ID;
  - added support for regular expressions (POSIX ERE Syntax) in remote
    commands <--list/-L> and <--edit/-E> using new subcommands <GR> and <FR>;
  - improved performance of RPC-command <listgroups>;
  - added new command <FileReorder> to RPC-method <editqueue> to set the
    order of individual files in the group;
  - added gzip-support to built-in web-server (including RPC);
  - added processing of http-request <OPTIONS> in RPC-server for better
    support of cross domain requests;
  - renamed example configuration file and postprocessing script to make
    the installation easier;
  - improved the automatic installation (<make install>) to install all
    necessary files (not only the binary as it was before);
  - improved handling of configuration errors: the program now does not
    terminate on errors but rather logs all of them and uses default option values;
  - added new XML/JSON-RPC methods <config>, <loadconfig> and <saveconfig>;
  - with active option <AllowReProcess> the NZB considered completed even if
    there are paused non-par-files (the paused non-par-files are treated the
    same way as paused par-files): as a result the reprocessable script is called;
  - added subcommand <W> to remote command <-S/--scan> to scan synchronously
    (wait until scan completed);
  - added parameter <SyncMode> to XML/JSON-RPC method <scan>;
  - the command <Scan> in web-interface now waits for completing of scan
    before reporting the status;
  - added remote command <--reload/-O> and JSON/XML-RPC method <reload> to
    reload configuration from disk and reintialize the program; the reload
    can be performed from web-interface;
  - JSON/XML-RPC method <append> extended with parameter <priority>;
  - categories available in web-interface are now configured in program
    configuration file (nzbget.conf) and can be managed via web-interface
    on settings page;
  - updated descriptions in example configuration file;
  - changes in configuration file:
        - renamed options <ServerIP>, <ServerPort> and <ServerPassword> to
          <ControlIP>, <ControlPort> and <ControlPassword> to avoid confusion
          with news-server options <ServerX.Host>, <ServerX.Port> and
          <ServerX.Password>;
        - the old option names are still recognized and are automatically
          renamed when the configuration is saved from web-interface;
        - also renamed option <$MAINDIR> to <MainDir>;
  - extended remote command <--append/-A> with optional parameters:
        - <T> - adds the file/URL to the top of queue;
        - <P> - pauses added files;
        - <C category-name> - sets category for added nzb-file/URL;
        - <N nzb-name> - sets nzb filename for added URL;
        - the old switches <--category/-K> and <--top/-T> are deprecated
          but still supported for compatibility;
  - renamed subcommand <K> of command <--edit/-E> to <C> (the old
    subcommand is still supported for compatibility);
  - added new option <NzbAddedProcess> to setup a script called after
    a nzb-file is added to queue;
  - added debug messages for speed meter;
  - improved the startup script <nzbgetd> so it can be directly used in
    </etc/init.d> without modifications;
  - fixed: after renaming of a group, the new name was not displayed
    by remote commands <-L G> and <-C in curses mode>;
  - fixed incompatibility with OpenSLL 1.0 (thanks to OpenWRT team
    for the patch);
  - fixed: RPC-method <log(0, IdFrom)> could return wrong results if
    the log was filtered with options <XXXTarget>;
  - fixed: free disk space calculated incorrectly on some OSes;
  - fixed: unrar failure was not always properly detected causing the
    post-processing to delete not yet unpacked rar-files;
  - fixed compilation error on recent linux versions;
  - fixed compilation error on older systems;

nzbget-0.8.0:
  - added priorities; new action <I> for remote command <--edit/-E> to set
    priorities for groups or individual files; new actions <SetGroupPriority>
    and <SetFilePriority> of RPC-command <editqueue>; remote command
    <--list/-L> prints priorities and indicates files or groups being
    downloaded; ncurses-frontend prints priorities and indicates files or
    groups being download; new command <PRIORITY> to set priority of nzb-file
    from nzbprocess-script; RPC-commands <listgroups> and <listfiles> return
    priorities and indicate files or groups being downloaded;
  - added renaming of groups; new subcommand <N> for command <--edit/-E>; new
    action <SetName> for RPC-method <editqueue>;
  - added new option <AccurateRate>, which enables syncronisation in speed
    meter; that makes the indicated speed more accurate by eliminating
    measurement errors possible due thread conflicts; thanks to anonymous
    nzbget user for the patch;
  - improved the parsing of filename from article subject;
  - option <DirectWrite> now efficiently works on Windows with NTFS partitions;
  - added URL-based-authentication as alternative to HTTP-header authentication
    for XML- and JSON-RPC;
  - fixed: nzb-files containing umlauts and other special characters could not
    be parsed - replaced XML-Reader with SAX-Parser - only on POSIX (not on 
    Windows);
  - fixed incorrect displaying of group sizes bigger than 4GB on many 64-bit
    OSes;
  - fixed a bug causing error on decoding of input data in JSON-RPC;
  - fixed a compilation error on some windows versions;
  - fixed: par-repair could fail when the filenames were not correctly parsed
    from article subjects;
  - fixed a compatibility issue with OpenBSD (and possibly other BSD based
    systems); added the automatic configuring of required signal handling logic
    to better support BSD without breaking the compatibility with certain Linux
    systems;
  - corrected the address of Free Software Foundation in copyright notice.

nzbget-0.7.0:
  - added history: new option <KeepHistory>, new remote subcommand <H> for
    commands <L> (list history entries) and <E> (delete history entries,
    return history item, postprocess history item), new RPC-command <History>
    and subcommands <HistoryDelete>, <HistoryReturn>, <HistoryProcess> for
    command <EditQueue>;
  - added support for JSON-P (extension of JSON-RPC);
  - changed the result code returning status <ERROR> for postprocessing script
    from <1> to <94> (needed to show the proper script status in history);
  - improved the detection of new files in incoming nzb directory: now the
    scanner does not rely on system datum, but tracks the changing of file
    sizes during a last few (<NzbDirFileAge>) seconds instead;
  - improvements in example postprocessing script: 1) if download contains
    only par2-files the script do not delete them during cleanup;
    2) if download contains only nzb-files the script moves them to incoming
    nzb-directory for further download;
  - improved formatting of groups and added time info in curses output mode;
  - added second pause register, which is independent of main pause-state and
    therfore is intended for usage from external scripts;
    that allows to pause download without interfering with options
    <ParPauseQueue> and <PostPauseQueue> and scheduler tasks <PauseDownload>
    and <UnpauseDownload> - they all work with first (default) pause register;
    new subcommand <D2> for commands <--pause/-P> and <--unpause/-U>;
    new RPC-command <pausedownload2> and <resumedownload2>;
    existing RPC-commands <pause> und <resume> renamed to <pausedownload> and
    <resumedownload>;
    new field <Download2Paused> in result struct for RPC-command <status>;
    existing fields <ServerPaused> and <ParJobCount> renamed to 
    <DownloadPaused> and <PostJobCount>;
    old RPC-commands and fields still exist for compatibility;
    the status output of command <--list/-L> indicates the state of second
    pause register;
    key <P> in curses-frontend can unpause second pause-register;
  - nzbprocess-script (option <NZBProcess>) can now set category and
    post-processing parameters for nzb-file;
  - redesigned server pool and par-checker to avoid using of semaphores
    (which are very platform specific);
  - added subcommand `<S>` to remote commands <--pause/-P> and <--unpause/-U> to
    pause/unpause the scanning of incoming nzb-directory;
  - added commands <PauseScan> and <UnpauseScan> for scheduler option 
    <TaskX.Command>;
  - added remote commands <PauseScan> and <ResumeScan> for XML-/JSON-RPC;
  - command <pause post-processing> now not only pauses the post-processing
    queue but also pauses the current post-processing job (par-job or 
    script-job);
    however the script-job can be paused only after the next line printed to
    screen;
  - improved error reporting while parsing nzb-files;
  - added field <NZBID> to NZBInfo; the field is now returned by XML-/JSON-RPC
    methods <listfiles>, <listgroups> and <postqueue>;
  - improvements in configure script;
  - added support for platforms without IPv6 (they do not have <getaddrinfo>);
  - debug-messages generated on early stages during initializing are now
    printed to screen/log-file;
  - messages about obsolete options are now printed to screen/log-file;
  - imporved example postprocessing script: added support for external
    configuration file, postprocessing parameters and configuration via
    web-interface;
  - option <TaskX.Process> now can contain parameters which must be passed
    to the script;
  - added pausing/resuming for post-processor queue; 
    added new modifier <O> to remote commands <--pause/-P> and <--unpause/-U>;
    added new commands <postpause> and <postresume> to XML-/JSON-RPC;
    extended output of remote command <--list/-L> to indicate paused state
    of post-processor queue; extended command <status> of XML-/JSON-RPC
    with field <PostPause>;
  - changed the command line syntax for requesting of post-processor queue
    from <-O> to <-L O> for consistency with other post-queue related
    commands (<-P O>, <-U O> and <-E O>);
  - improved example post-processing script: added support for delayed
    par-check (try unrar first, par-repair if unrar failed);
  - added modifier <O> to command <-E/--edit> for editing of
    post-processor-queue;
    following subcommands are supported: <+/-offset>, <T>, <B>, <D>;
    subcommand <D> supports deletion of queued post-jobs and active job as well;
    deletion of active job means the cancelling of par-check/repair or
    terminating of post-processing-script (including child processes of the 
    script);
    updated remote-server to support new edit-subcommands in XML/JSON-RPC;
  - extended the syntax of option <TaskX.Time> in two ways: 
    1) it now accepts multiple comma-separated values;
    2) an asterix as hours-part means <every hour>;
  - added svn revision number to version string (commands <-v> and <-V>,
    startup log entry);
    svn revision is automatically read from svn-repository on each build;
  - added estimated remaining time and better distinguishing of server state
    in command <--list/-L>;
  - added new return code (93) for post-processing script to indicate
    successful processing; that results in cleaning up of download queue
    if option <ParCleanupQueue> is active;
  - added readonly options <AppBin>, <ConfigFile> and <Version> for usage
    in processing scripts (options are available as environment variables
    <NZBOP_APPBIN>, <NZBOP_CONFIGFILE> and <NZBOP_VERSION>);
  - renamed ParStatus constant <FAILED> to <FAILURE> for a consistence with
    ScriptStatus constant <FAILURE>, that also affects the results of
    RPC-command <history>;
  - added a new return code <95/POSTPROCESS_NONE> for post-processing scripts
    for cases when pp-script skips all post-processing work (typically upon
    a user's request via a pp-parameter);
    modified the example post-processing script to return the new code
    instead of a error code when a pp-parameter <PostProcess> was set to <no>;
  - added field <PostTime> to result of RPC-Command <listfiles> and fields
    <MinPostTime> and <MaxPostTime> for command <listgroups>;
  - in <curses> and <colored> output-modes the download speed is now printed
    with one decimal digit when the speed is lower than 10 KB/s;
  - improvement in example post-processing script: added check for existence
    of <unrar> and command <wc>;
  - added shell batch file for windows (nzbget-shell.bat);
    thanks to orbisvicis (orbisvicis@users.sourceforge.net) for the script;
  - added debian style init script (nzbgetd);
    thanks to orbisvicis (orbisvicis@users.sourceforge.net) for the script;
  - added the returning of a proper HTTP error code if the authorization was
    failed on RPC-calls;
    thanks to jdembski (jdembski@users.sourceforge.net) for the patch;
  - changed the sleep-time during the throttling of bandwidth from 200ms to
    10ms in order to achieve better uniformity;
  - modified example postprocessing script to not use the command <dirname>,
    which is not always available; 
    thanks to Ger Teunis for the patch;
  - improved example post-processing script: added the check for existence
    of destination directory to return a proper ERROR-code (important for
    reprocessing of history items);
  - by saving the queue to disk now using relative paths for the list of
    compeled files to reduce the file's size;
  - eliminated few compiler warnings on GCC;
  - fixed: when option <DaemonUserName> was specified and nzbget was
    started as root, the lockfile was not removed;
  - fixed: nothing was downloaded when the option <Retries> was set to <0>;
  - fixed: base64 decoding function used by RPC-method <append> sometimes
    failed, in particular when called from Ruby-language;
  - fixed: JSON-RPC-commands failed, if parameters were placed before method
    name in the request;
  - fixed: RPC-method <append> did not work properly on Posix systems
    (it worked only on Windows);
  - fixed compilation error when using native curses library on OpenSolaris;
  - fixed linking error on OpenSolaris when using GnuTLS;
  - fixed: option <ContinuePartial> did not work;
  - fixed: seg. fault in service mode on program start (Windows only);
  - fixed: environment block was not passed correctly to child process,
    what could result in seg faults (windows only);
  - fixed: returning the postprocessing exit code <92 - par-check all 
    collections> when there were no par-files results in endless calling
    of postprocessing script;
  - fixed compatibility issues with OS/2.

nzbget-0.6.0:
  - added scheduler; new options <TaskX.Time>, <TaskX.WeekDays>,
    <TaskX.Command>, <TaskX.DownloadRate> and <TaskX.Process>;
  - added support for postprocess-parameters; new subcommand <O> of remote
    command <E> to add/modify pp-parameter for group (nzb-file); new
    XML-/JSON-RPC-subcommand <GroupSetParameter> of method <editqueue> for
    the same purpose; updated example configuration file and example
    postprocess-script to indicate new method of passing arguments via
    environment variables;
  - added subcommands <F>, <G> and `<S>` to command line switch <-L/--list>,
    which prints list of files, groups or only status info respectively;
    extended binary communication protocol to transfer nzb-infos in addition
    to file-infos;
  - added new subcommand <M> to edit-command <E> for merging of two (or more)
    groups (useful after adding pars from a separate nzb-file);
  - added option <MergeNzb> to automatically merge nzb-files with the same
    filename (useful by adding pars from a different source);
  - added script-processing of files in incoming directory to allow automatic
    unpacking and queueing of compressed nzb-files; new option <NzbProcess>;
  - added the printing of post-process-parameters for groups in command
    <--list G>;
  - added the printing of nzbget version into the log-file on start;
  - added option <DeleteCleanupDisk> to automatically delete already downloaded
    files from disk if nzb-file was deleted from queue (the download was
    cancelled);
  - added option <ParTimeLimit> to define the max time allowed for par-repair;
  - added command <--scan/-S> to execute the scan of nzb-directory on remote
    server;
  - changed the method to pass arguments to postprocess/nzbprocess: now using
    environment variables (old method is still supported for compatibility with
    existing scripts);
  - added the passing of nzbget-options to postprocess/nzbprocess scripts as
    environment variables;
  - extended the communication between nzbget and post-process-script: 
    collections are now detected even if parcheck is disabled;
  - added support for delayed par-check/repair: post-process-script can request
    par-check/repair using special exit codes to repair current collection or
    all collections;
  - implemented the normalizing of option names and values in option list; the
    command <-p> also prints normalized names and values now; that makes the
    parsing of output of command <-p> for external scripts easier;
  - replaced option <PostLogKind> with new option <ProcessLogKind> which is now
    used by all scripts (PostProcess, NzbProcess, TaskX.Process);
  - improved entering to paused state on connection errors (do not retry failed
    downloads if pause was activated);
  - improved error reporting on decoding failures;
  - improved compatibility of yenc-decoder;
  - improved the speed of deleting of groups from download queue (by avoiding
    the saving of queue after the deleting of each individual file);
  - updated configure-script for better compatibility with FreeBSD;
  - cleaning up of download queue (option <ParCleanupQueue>) and deletion of
    source nzb-file (option <NzbCleanupDisk>) after par-repair now works also
    if par-repair was cancelled (option <ParTimeLimit>); since required
    par-files were already downloaded the repair in an external tool is
    possible;
  - added workaround to avoid hangs in child processes (by starting of
    postprocess or nzbprocess), observed on uClibC based systems;
  - fixed: TLS/SSL didn't work in standalone mode;
  - fixed compatibility issues with Mac OS X;
  - fixed: not all necessary par2-files were unpaused on first request for
    par-blocks (although harmless, because additional files were unpaused
    later anyway);
  - fixed small memory leak appeared if process-script could not be started;
  - fixed: configure-script could not detect the right syntax for function
    <ctime_r> on OpenSolaris.
  - fixed: files downloaded with disabled decoder (option decode=no) sometimes
    were malformed and could not be decoded;
  - fixed: empty string parameters did not always work in XML-RPC.

nzbget-0.5.1:
  - improved the check of server responses to prevent unnecessary retrying
    if the article does not exist on server;
  - fixed: seg.fault in standalone mode if used without specifying the
    category (e.g. without switch <-K>);
  - fixed: download speed indicator could report not-null values in
    standby-mode (when paused);
  - fixed: parameter <category> in JSON/XML-RPC was not properly decoded by
    server, making the setting of a nested category (containing slash or
    backslash character) via nzbgetweb not possible;

nzbget-0.5.0:
  - added TLS/SSL-support for encrypted communication with news-servers;
  - added IPv6-support for communication with news-servers as well as for
    communication between nzbget-server and nzbget-client;
  - added support for categories to organize downloaded files;
  - new option <AppendCategoryDir> to create the subdirectory for each category;
  - new switch <-K> for usage with switch <-A> to define a category during
    the adding a file to download queue;
  - new command <K> in switch <-E> to change the category of nzb-file in
    download queue; the already downloaded files are automatically moved to new
    directory if the option <AppendCategoryDir> is active;
  - new parameter <Category> in XML-/JSON-RPC-command <editqueue> to allow the
    changing of category via those protocols;
  - new parameter in a call to post-process-script with category name; 
  - scanning of subdirectories inside incoming nzb-directory to automatically 
    assign category names; nested categories are supported;
  - added option <ServerX.JoinGroup> to connect to servers, that do not accept
    <GROUP>-command;
  - added example post-process script for unraring of downloaded files
    (POSIX only);
  - added options <ParPauseQueue> and <PostPauseQueue> useful on slow CPUs;
  - added option <NzbCleanupDisk> to delete source nzb-file after successful 
    download and parcheck;
  - switch <-P> can now be used together with switches <-s> and <-D> to start
    server/daemon in paused state;
  - changed the type of messages logged in a case of connection errors from
    <DEBUG> to <ERROR> to provide better error reporting;
  - now using OS-specific line-endings in log-file and brokenlog-file: LF on 
    Posix and CRLF on Windows;
  - added detection of adjusting of system clock to correct uptime/download 
    time (for NAS-devices, that do not have internal clock and set time from
    internet after booting, while nzbget may be already running);
  - added the printing of stack on segmentation faults (if configured with 
    <--enable-debug>, POSIX only);
  - added option <DumpCore> for better debugging on Linux in a case of abnormal
    program termination;
  - fixed: configure-script could not automatically find libsigc++ on 64-bit
    systems;
  - few other small fixes; 

nzbget-0.4.1:
  - to avoid accidental deletion of file in curses-frontend the key <D>
    now must be pressed in uppercase;
  - options <username> and <password> in news-server's configuration are now 
    optional;
  - added the server's name to the detail-log-message, displayed on the start 
    of article's download;
  - added the option <AllowReProcess> to help to post-process-scripts, which 
    make par-check/-repair on it's own;
  - improved download-speed-meter: it uses now a little bit less cpu and
    calculates the speed for the last 30 seconds (instead of 5 seconds),
    providing better accuracy; Thanks to ydrol <ydrol@users.sourceforge.net>
    for the patch;
  - reduced CPU-usage in curses-outputmode; Thanks to ydrol for the patch 
    <ydrol@users.sourceforge.net>;
  - fixed: line-endings in windows-style (CR-LF) in config-file were not 
    read properly;
  - fixed: trailing spaces in nzb-filenames (before the file's extension) 
    caused errors on windows. Now they will be trimmed;
  - fixed: XML-RPC and JSON-RPC did not work on Big-Endian-CPUs (ARM, PPC, etc), 
    preventing the using of web-interface;
  - fixed: umask-option did not allow to enable write-permissions for <group> 
    and <others>;
  - fixed: in curses-outputmode the remote-client showed on first screen-update 
    only one item of queue;
  - fixed: edit-commands with negative offset did not work via XML-RPC 
    (but worked via JSON-RPC);
  - fixed incompatibility issues with gcc 4.3; Thanks to Paul Bredbury 
    <brebs@users.sourceforge.net> for the patch;
  - fixed: segmentation fault if a file listed in nzb-file does not have any 
    segments (articles);

nzbget-0.4.0:
  - added the support for XML-RPC and JSON-RPC to easier control the server 
    from other applications; 
  - added web-interface - it is available for download from nzbget-project's 
    home page as a separate package "web-interface";
  - added the automatic cleaning up of the download queue (deletion of unneeded 
    paused par-files) after successful par-check/repair - new 
    option <ParCleanupQueue>;
  - added option <DetailTarget> to allow to filter the (not so important) 
    log-messages from articles' downloads (they have now the type <detail> 
    instead of <info>);
  - added the gathering of progress-information during par-check; it is 
    available via XML-RPC or JSON-RPC; it is also showed in web-interface;
  - improvements in internal decoder: added support for yEnc-files without 
    ypart-statement (sometimes used for small files); added support for 
    UU-format;
  - removed support for uulib-decoder (it did not work well anyway); 
  - replaced the option <decoder (yenc, uulib, none)> with the option 
    <decode (yes, no)>;
  - added detection of errors <server busy> and <remote server not available> 
    (special case for NNTPCache-server) to consider them as connect-errors 
    (and therefore not count as retries); 
  - added check for incomplete articles (also mostly for NNTPCache-server) to 
    differ such errors from CrcErrors (better error reporting);
  - improved error-reporting on moving of completed files from tmp- to 
    dst-directory and added code to move files across drives if renaming fails;
  - improved handling of nzb-files with multiple collections in par-checker;
  - improved the parchecker: added the detection and processing of files 
    splitted after parring;
  - added the queueing of post-process-scripts and waiting for script's 
    completion before starting of a next job in postprocessor (par-job or 
    script) to provide more balanced cpu utilization;
  - added the redirecting of post-process-script's output to log; new option 
    <PostLogKind> to specify the default message-kind for unformatted 
    log-messages;
  - added the returning of script-output by command <postqueue> via XML-RPC 
    and JSON-RPC; the script-output is also showed in web-interface;
  - added the saving and restoring of the post-processor-queue (if server was 
    stopped before all items were processed); new option <ReloadPostQueue>;
  - added new parameter to postprocess-script to indicate if any of par-jobs 
    for the same nzb-file failed;
  - added remote command (switch O/--post) to request the post-processor-queue 
    from server;
  - added remote command (switch -W/--write) to write messages to server's log;
  - added option <DiskSpace> to automatically pause the download on low disk 
    space;
  - fixed few incompatibility-issues with unslung-platform on nslu2 (ARM);
  - fixed: articles with trailing text after binary data caused the decode 
    failures and the reporting of CRC-errors;
  - fixed: dupecheck could cause seg.faults when all articles for a file failed;
  - fixed: by dupe-checking of files contained in nzb-file the files with the 
    same size were ignored (not deleted from queue);
  - updated libpar2-patch for msvc to fix a segfault in libpar2 (windows only);
  - fixed: by registering the service on windows the fullpath to nzbget.exe 
    was not always added to service's exename, making the registered service 
    unusable;
  - fixed: the pausing of a group could cause the start of post-processing for 
    that group;
  - fixed: too many info-messages <Need more N blocks> could be printed during 
    par-check (appeared on posix only);

nzbget-0.3.1:
  - Greatly reduced the memory consumption by keeping articles' info on disk 
    until the file download starts;
  - Implemented decode-on-the-fly-technique to reduce disk-io; downloaded
    and decoded data can also be saved directly to the destination file 
    (without any intermediate files at all); this eliminates the necessity
    of joining of articles later (option "DirectWrite");
  - Improved communication with news-servers: connections are now keeped open 
    until all files are downloaded (or server paused); this eliminates the 
    need for establishing of connections and authorizations for each 
    article and improves overal download speed;
  - Significantly better download speed is now possible on fast connection; 
    it was limited by delays on starting of new articles' downloads; 
    the synchronisation mechanism was reworked to fix this issue;
  - Download speed meter is much more accurate, especially on fast connections;
    this also means better speed throttling;
  - Speed optimisations in internal decoder (up to 25% faster);
  - CRC-calculation can be bypassed to increase performance on slow CPUs
    (option "CrcCheck");
  - Improved parsing of artcile's subject for better extracting of filename 
    part from it and implemented a fallback-option if the parsing was incorrect; 
  - Improved dupe check for files from the same nzb-request to detect reposted 
    files and download only the best from them (if option "DupeCheck" is on); 
  - Articles with incorrect CRC can be treated as "possibly recoverable errors"
    and relaunched for download (option "RetryOnCrcError"), it is useful if 
    multiple servers are available;
  - Improved error-check for downloaded articles (crc-check and check for
    received message-id) decreases the number of broken files;
  - Extensions in curses-outputmode: added group-view-mode (key "G") to show
    items in download queue as groups, where one group represents all files 
    from the same nzb-file; the editing of queue works also in group-mode 
    (for all files in this group): pause/unpause/delete/move of groups; 
  - Other extensions in curses-outputmode: key "T" toggles timestamps in log;
    added output of statistical data: uptime, download-time, average session 
    download speed;
  - Edit-command accepts more than one ID or range of IDs.
    E.g: "nzbget -E P 2,6-10,33-39"; The switch "-I" is not used anymore;
  - Move-actions in Edit-command affect files in a smart order to guarantee 
    that the relative order of files in queue is not changed after the moving;
  - Extended syntax of edit-command to edit groups (pause/unpause/delete/move 
    of groups). E.g: "nzbget -E G P 2";
  - Added option "DaemonUserName" to set the user that the daemon (POSIX only) 
    normally runs at. This allows nzbget daemon to be launched in rc.local 
    (at boot), and download items as a specific user id; Thanks to Thierry 
    MERLE <merlum@users.sourceforge.net> for the patch;
  - Added option "UMask" to specify permissions for newly created files and dirs 
    (POSIX only);
  - Communication protocol used between server and client was revised to define 
    the byte order for transferred data. This allows hosts with different 
    endianness to communicate with each other;
  - Added options "CursesNzbName", "CursesGroup" and "CursesTime" to define
    initial state of curses-outputmode;
  - Added option "UpdateInterval" to adjust update interval for Frontend-output
    (useful in remote-mode to reduce network usage);
  - Added option "WriteBufferSize" to reduce disk-io (but it could slightly 
    increase memory usage and therefore disabled by default);
  - List-command prints additional statistical info: uptime, download-time,
    total amount of downloaded data and average session download speed;
  - The creation of necessary directories on program's start was extended 
    with automatic creation of all parent directories or error reporting 
    if it was not possible;
  - Printed messages are now translated to oem-codepage to correctly print 
    filenames with non-english characters (windows only);
  - Added remote-command "-V (--serverversion)" to print the server's version;
  - Added option "ThreadLimit" to prevent program from crash if it wants to 
    create too many threads (sometimes could occur in special cases);
  - Added options "NzbDirInterval" and "NzbDirFileAge" to adjust interval and
    delay by monitoring of incoming-directory for new nzb-files;
  - Fixed error on parsing of nzb-files containing percent and other special 
    characters in their names (bug appeared on windows only);
  - Reformated sample configuration file and changed default optionnames 
    from lowercase to MixedCase for better readability; 
  - Few bugs (seg faults) were fixed.

nzbget-0.3.0:
  - The download queue now contains newsgroup-files to be downloaded instead of 
    nzb-jobs. By adding a new job, the nzb-file is immediately parsed and each 
    newsgroup-file is added to download queue. Each file can therefore be 
    managed separately (paused, deleted or moved);
  - Current queue state is saved after every change (file is completed or the 
    queue is changed - entries paused, deleted or moved). The state is saved on
    disk using internal format, which allows fast loading on next program start
    (no need to parse xml-files again);
  - The remaining download-size is updated after every article is completed to
    indicate the correct remaining size and time for total files in queue;
  - Downloaded articles, which are saved in temp-directory, can be reused on
    next program start, if the file was not completed (option "continuepartial"
    in config-file); 
  - Along with uulib the program has internal decoder for yEnc-format. This
    decoder was necessary, because uulib is so slow, that it prevents using of
    the program on not so powerful systems like linux-routers (MIPSEL CPU 200
    MHz). The new decoder is very fast. It is controlled over option "decoder" 
    in config-file;
  - The decoder can be completely disabled. In this case all downloaded articles
    are saved in unaltered form and can be joined with an external program;
    UUDeview is one of them;
  - If download of article fails, the program attempts to download it again so
    many times, what the option "retries" in config-file says. This works even
    if no servers with level higher than "0" defined. After each retry the next
    server-level is used, if there are no more levels, the program switches to
    level "0" again. The pause between retries can be set with config-option
    "retryinterval";
  - If despite of a stated connection-timeout (it can be changed via
    config-option "connectiontimeout") connection hangs, the program tries to
    cancel the connection (after "terminatetimeout" seconds). If it doesn't
    work the download thread is killed and the article will be redownloaded in
    a new thread. This ensures, that there are no long-time hanging connections
    and all articles are downloaded, when a time to rejoin file comes;
  - Automatic par-checking and repairing. Only reuired par-files are downloaded.
    The program uses libpar2 and does not require any external tools. The big
    advantage of library is, that it allows to continue par-check after new
    par-blocks were downloaded. This were not possible with external 
    par2cmdline-tool;
  - There is a daemon-mode now (command-line switch "-D" (--daemon)). In this
    mode a lock-file (default location "/tmp/nzbget.lock", can be changed via
    option "lockfile") contains PID of daemon;
  - The format of configuration-file was changed from xml to more common
    text-format. It allows also using of variables like 
    "tempdir=${MAINDIR}/tmp";
  - Any option of config-file can be overwritten via command-line switch
    "-o" (--option). This includes also the definition of servers. 
    This means that the program can now be started without a configuration-file 
    at all (all required options must be passed via command-line);
  - The command-line switches were revised. The dedicated switches to change
    options in config-file were eliminated, since any option can now be changed
    via switch "-o" (--option);
  - If the name of configuration-file was not passed via command-line the
    program search it  in following locations: "~/.nzbget", "/etc/nzbget.conf", 
    "/usr/etc/nzbget.conf", "/usr/local/etc/nzbget.conf", 
    "/opt/etc/nzbget.conf";
  - The new command-line switch "-n" (--noconfigfile) prevents the loading of
    a config-file. All required config-options must be passed via command-line
    (switch "-o" (--option));
  - To start the program in server mode either "-s" (--server) or 
    "-D" (--daemon) switch must be used. If the program started without any
    parameters it prints help-screen. There is no a dedicated switch to start
    in a standalone mode. If switches "-s" and "-D" are omitted and none of
    client-request-switches used the standalone mode is default. This usage
    of switches is more common to programs like "wget". To add a file to 
    server's download queue use switch "-A" (--append) and a name of nzb-file
    as last command-line parameter;
  - There is a new switch "-Q" (--quit) to gracefully stop server. BTW the
    SIGKIL-signal is now handled appropriately, so "killall nzbget" is also OK,
    where "killall -9 nzbget" terminates server immediately (helpful if it
    hangs, but it shouldn't);
  - With new switch "-T" (--top) the file will be added to the top of download
    queue. Use it with switch "-A" (--append);
  - The download queue can be edited via switch "-E" (--edit). It is possible
    to pause, unpause, delete and move files in queue. The IDs of file(s)
    to be affected are passed via switch "-I" (fileid), either one ID or a 
    range in a form "IDForm-IDTo". This also means, that every file in queue
    have ID now;
  - The switch "-L" (--list) prints IDs of files consequently. It prints also
    name, size, percentage of completing and state (paused or not) of each file. 
    Plus summary info: number of files, total remaining size and size of
    paused files, server state (paused or running), number of threads on 
    server, current speed limit;
  - With new switch "-G" (--log) the last N lines printed to server's 
    screen-log, can be printed on client. The max number of lines which can
    be returned from servers depends on option "logbuffersize";
  - The redesigned Frontends (known as outputmodes "loggable", "colored" and
    "curses") can connect to (remote) server and behave as if you were running
    server-instance of program itself (command-line switch "-C" (--connect)). 
    The log-output updates constantly and even all control-functions in 
    ncurses-mode works: pause/unpause server, set download rate limit, edit of
    queue (pause/unpause, delete, move entries). The number of connected 
    clients is not limited. The "outputmode" on a client can be set
    independently from server. The client-mode is especially useful if the
    server runs as a daemon;
  - The writing to log-file can be disabled via option "createlog". 
    The location of log-file controls the option "log-file";
  - Switch "-p" (--printconfig) prints the name of configuration file being
    used and all option/value-pairs, taking into account all used 
    "-o" (--option) - switches;
  - The communication protocol between server and client was optimized to
    minimize the size of transferred data. Instead of fixing the size for
    Filenames in data-structures to 512 bytes only in fact used data
    are transferred;
  - Extensions in ncurses-outputmode: scrolling in queue-list works better, 
    navigation in queue with keys Up, Down, PgUp, PgDn, Home, End. 
    Keys to move entries are "U" (move up), "N" (move down), "T" (move to top),
    "B" (move to bottom). "P" to pause/unpause file. The size, percentage 
    of completing and state (paused or not) for every file is printed. 
    The header of queue shows number of total files, number of unpaused 
    files and size for all and unpaused files. Better using of screen estate
    space - no more empty lines and separate header for status (total seven
    lines gain). The messages are printed on several lines (if they not fill
    in one line) without trimming now;
  - configure.ac-file updated to work with recent versions of autoconf/automake. 
    There are new configure-options now: "--disable-uulib" to compile the 
    program without uulib; "--disable-ncurses" to disable ncurses-support 
    (eliminates necessity of ncurses-libs), useful on embedded systems with 
    little resources; "--disable-parcheck" to compile without par-check;
  - The algorithm for parsing of nzb-files now uses XMLReader instead of 
    DOM-Parser to minimize memory usage (no mor needs to build complete DOM-tree
    in memory). Thanks to Thierry MERLE <merlum@users.sourceforge.net> for 
    the patch;
  - The log-file contains now thread-ID for all entry-types and additionally 
    for debug-entries: filename, line number and function's name of source
    code, where the message was printed. Debug-messages can be disabled in 
    config-file (option "debugtarget") like other messages;
  - The program is now compatible with windows. Project file for MS Visual 
    C++ 2005 is included. Use "nzbget -install" and "nzbget -remove" to
    install/remove nzbget-Service. Servers and clients can run on diferrent
    operating systems;
  - Improved compatibility with POSIX systems; Tested on:
        - Linux Debian 3.1 on x86;
        - Linux BusyBox with uClibc on MIPSEL;
        - PC-BSD 1.4 (based on FreeBSD 6.2) on x86;
        - Solaris 10 on x86;
  - Many memory-leaks and thread issues were fixed;
  - The program was thoroughly worked over. Almost every line of code was
    revised.

nzbget-0.2.3
  - Fixed problem with losing connection to newsserver after too long idle time
  - Added functionality for dumping lots of debug info

nzbget-0.2.2
  - Added Florian Penzkofers fix for FreeBSD, exchanging base functionality in 
    SingleServerPool.cpp with a more elegant solution
  - Added functionality for forcing answer to reloading queue upon startup of 
    server
    + use -y option to force from command-line
	+ use "reloadqueue" option in nzbget.cfg to control behavior
  - Added nzbget.cfg options to control where info, warnings and errors get 
    directed to (either screen, log or both)
  - Added option "createbrokenfilelog" in nzbget.cfg

nzbget-0.2.1
  - Changed and extended the TCP client/server interface
  - Added timeout on sockets which prevents certain types of nzbget hanging
  - Added Kristian Hermansen's patch for renaming broken files

nzbget-0.2.0
  - Moved 0.1.2-alt4 to a official release as 0.2.0
  - Small fixes

nzbget-0.1.2-alt4
  - implemented tcp/ip communication between client & server (removing the 
    rather defunct System V IPC)
  - added queue editing functionality in server-mode

nzbget-0.1.2-alt1
  - added new ncurses frontend
  - added server/client-mode (using System V IPC)
  - added functionality for queueing download requests 

nzbget-0.1.2
  - performance-improvements
  - commandline-options
  - fixes

nzbget-0.1.1
  - new output
  - fixes

nzbget-0.1.0a
  - compiling-fixes

nzbget-0.1.0
  - initial release
