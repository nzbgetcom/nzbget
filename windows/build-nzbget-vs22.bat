@echo off

rem
rem  This file is part of nzbget
rem
rem  Copyright (C) 2013-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
rem
rem  This program is free software; you can redistribute it and/or modify
rem  it under the terms of the GNU General Public License as published by
rem  the Free Software Foundation; either version 2 of the License, or
rem  (at your option) any later version.
rem
rem  This program is distributed in the hope that it will be useful,
rem  but WITHOUT ANY WARRANTY; without even the implied warranty of
rem  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem  GNU General Public License for more details.
rem
rem  You should have received a copy of the GNU General Public License
rem  along with this program.  If not, see <http://www.gnu.org/licenses/>.
rem

rem ####################### CONFIG SECTION #######################

set BUILD_DEBUG=1
set BUILD_RELEASE=1
set BUILD_32=1
set BUILD_64=1

rem expression "%~dp0" means the location of an executing batch file
set SRCDIR=%cd%
rem set ROOTDIR=%~dp0
set ROOTDIR=C:\nzbget\build\
rem remove trailing slash from ROOTDIR
set ROOTDIR=%ROOTDIR:~0,-1%
set LIBDIR=%ROOTDIR%\lib
set ZIP=%ROOTDIR%\tools\7-Zip\7z.exe
set SED=%ROOTDIR%\tools\sed\sed.exe
set CURL=%ROOTDIR%\tools\curl\src\curl.exe
set NSIS=%ROOTDIR%\tools\nsis
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set BUILD_SETUP=1

rem ####################### END OF CONFIG SECTION #######################

title Building NZBGet

rem Try to detect version from nzbget.vcxproj
for /f "tokens=3 delims=;" %%n in ('findstr VERSION nzbget.vcxproj') do (
    for /f "tokens=2 delims==" %%m in ("%%n") do (
        set "VERSION=%%~m"
    )
)
set FULLVERSION=%VERSION%

cd %ROOTDIR%
set UseEnv=true
echo Building nzbget

if exist tmp (rmdir /S /Q tmp)
if errorlevel 1 goto BUILD_FAILED
mkdir tmp
if errorlevel 1 goto BUILD_FAILED

echo Version: [%VERSION%]/[%FULLVERSION%]

if not exist image (
	echo Cannot find extra files for setup. Create directory '%ROOTDIR%\image' and put all extra files there: unrar.exe, etc.
	goto BUILD_FAILED
)

rem Prepare output directory
if exist output (rmdir /S /Q output)
if errorlevel 1 goto BUILD_FAILED
mkdir output
if errorlevel 1 goto BUILD_FAILED

rem Copy source tree to build
robocopy %SRCDIR% %ROOTDIR%\tmp\nzbget-%VERSION% /MIR /NFL /NDL /NJH /NJS /NC /NS /NP

rem Update ca root certificates
echo Updating root certificates
cd image
%CURL% --remote-name --time-cond cacert.pem https://curl.se/ca/cacert.pem
if errorlevel 1 goto BUILD_FAILED
rem remove expired DST Root CA X3 certificate
%SED% -e "/^DST Root CA X3$/,/^-----END CERTIFICATE-----$/d;" -i cacert.pem
cd ..

cd tmp\nzbget-%VERSION%

:TARGET_DEBUG
rem Build debug binaries
if %BUILD_DEBUG%==0 goto TARGET_RELEASE
cd nzbget-%VERSION%
call:PrepareFiles
if %BUILD_32%==1 ( call:BuildTarget Debug x86 x86 32 )
if %BUILD_64%==1 ( call:BuildTarget Debug x64 amd64 64 )
cd ..

if %BUILD_SETUP%==1 (
	rem Build debug setup
	cd distrib
	call:BuildSetup
	if errorlevel 1 goto BUILD_FAILED
	move nzbget-setup.exe ..\..\output\nzbget-%FULLVERSION%-bin-windows-debug-setup.exe
	cd ..
)

:TARGET_RELEASE
rem Build release binaries
if %BUILD_RELEASE%==0 goto END
cd nzbget-%VERSION%
call:PrepareFiles
if %BUILD_32%==1 ( call:BuildTarget Release x86 x86 32 )
if %BUILD_64%==1 ( call:BuildTarget Release x64 amd64 64 )
cd ..

if %BUILD_SETUP%==1 (
	rem Build release setup
	cd distrib
	call:BuildSetup
	if errorlevel 1 goto BUILD_FAILED
	move nzbget-setup.exe ..\..\output\nzbget-%FULLVERSION%-bin-windows-setup.exe
	cd ..
)

goto END


:BUILD_FAILED
echo ********************************
echo Build failed
echo ********************************
exit 1

:END
exit 0

rem END OF SCRIPT


rem Build one binary (for specified configuration and platofrm)
:BuildTarget
echo ***** Building %~1 %~2 (%~3 %~4)
if exist ..\bin (rmdir /S /Q ..\bin)
set INCLUDE=
set LIB=
call %VCVARS% %~3
set INCLUDE=%LIBDIR%\include;%INCLUDE%
set LIB=%LIBDIR%\%~2;%LIB%
msbuild.exe nzbget.vcxproj /t:Rebuild /p:Configuration=%~1 /p:Platform=%~2
if errorlevel 1 goto BUILD_FAILED
copy ..\bin\nzbget.exe ..\distrib\NZBGet\%~4
if errorlevel 1 goto BUILD_FAILED
if %~1==Debug (
	copy ..\bin\nzbget.pdb ..\distrib\NZBGet\%~4
	if errorlevel 1 goto BUILD_FAILED
)
GOTO:EOF


rem Prepare files for setup
:PrepareFiles
if exist ..\distrib (rmdir /S /Q ..\distrib)
mkdir ..\distrib\NZBGet

mkdir ..\distrib\NZBGet\32
mkdir ..\distrib\NZBGet\64
if %BUILD_32%==0 (
	echo This test setup doesn't include binaries for 32 bit platform > ..\distrib\NZBGet\32\README-WARNING.txt
)
if %BUILD_64%==0 (
	echo This test setup doesn't include binaries for 64 bit platform > ..\distrib\NZBGet\64\README-WARNING.txt
)

copy windows\nzbget-command-shell.bat ..\distrib\NZBGet
if errorlevel 1 goto BUILD_FAILED
copy windows\install-update.bat ..\distrib\NZBGet
if errorlevel 1 goto BUILD_FAILED
copy windows\README-WINDOWS.txt ..\distrib\NZBGet
if errorlevel 1 goto BUILD_FAILED
copy ChangeLog.md ..\distrib\NZBGet
if errorlevel 1 goto BUILD_FAILED
copy INSTALLATION.md ..\distrib\NZBGet
if errorlevel 1 goto BUILD_FAILED
copy COPYING ..\distrib\NZBGet
if errorlevel 1 goto BUILD_FAILED
copy pubkey.pem ..\distrib\NZBGet
if errorlevel 1 goto BUILD_FAILED

mkdir ..\distrib\NZBGet\webui
xcopy /E webui ..\distrib\NZBGet\webui
if errorlevel 1 goto BUILD_FAILED

copy windows\package-info.json ..\distrib\NZBGet\webui
if errorlevel 1 goto BUILD_FAILED

rem Adjust config file
set CONFFILE=..\distrib\NZBGet\nzbget.conf.template
copy nzbget.conf %CONFFILE%
if errorlevel 1 goto BUILD_FAILED
%SED% -e "s|MainDir=.*|MainDir=${AppDir}\\downloads|" -i %CONFFILE%
%SED% -e "s|DestDir=.*|DestDir=${MainDir}\\complete|" -i %CONFFILE%
%SED% -e "s|InterDir=.*|InterDir=${MainDir}\\intermediate|" -i %CONFFILE%
%SED% -e "s|ScriptDir=.*|ScriptDir=${MainDir}\\scripts|" -i %CONFFILE%
%SED% -e "s|LogFile=.*|LogFile=${MainDir}\\nzbget.log|" -i %CONFFILE%
%SED% -e "s|AuthorizedIP=.*|AuthorizedIP=127.0.0.1|" -i %CONFFILE%
%SED% -e "s|UnrarCmd=.*|UnrarCmd=${AppDir}\\unrar.exe|" -i %CONFFILE%
%SED% -e "s|SevenZipCmd=.*|SevenZipCmd=${AppDir}\\7za.exe|" -i %CONFFILE%
%SED% -e "s|ArticleCache=.*|ArticleCache=250|" -i %CONFFILE%
%SED% -e "s|ParBuffer=.*|ParBuffer=250|" -i %CONFFILE%
%SED% -e "s|WriteBuffer=.*|WriteBuffer=1024|" -i %CONFFILE%
%SED% -e "s|CertStore=.*|CertStore=${AppDir}\\cacert.pem|" -i %CONFFILE%
%SED% -e "s|CertCheck=.*|CertCheck=yes|" -i %CONFFILE%
%SED% -e "s|DirectRename=.*|DirectRename=yes|" -i %CONFFILE%
%SED% -e "s|DirectUnpack=.*|DirectUnpack=yes|" -i %CONFFILE%
rem Hide certain options from web-interface settings page
%SED% -e "s|WebDir=.*|# WebDir=${AppDir}\\webui|" -i %CONFFILE%
%SED% -e "s|LockFile=.*|# LockFile=|" -i %CONFFILE%
%SED% -e "s|ConfigTemplate=.*|# ConfigTemplate=${AppDir}\\nzbget.conf.template|" -i %CONFFILE%
%SED% -e "s|DaemonUsername=.*|# DaemonUsername=|" -i %CONFFILE%
%SED% -e "s|UMask=.*|# UMask=|" -i %CONFFILE%

mkdir ..\distrib\NZBGet\scripts
xcopy /E scripts ..\distrib\NZBGet\scripts
if errorlevel 1 goto BUILD_FAILED

copy ..\..\image\* ..\distrib\NZBGet
copy ..\..\image\32\* ..\distrib\NZBGet\32
copy ..\..\image\64\* ..\distrib\NZBGet\64
if errorlevel 1 goto BUILD_FAILED

del /S ..\distrib\NZBGet\_*.*

GOTO:EOF


rem Build one setup for current configuration (debug or release)
:BuildSetup
if not exist resources (
	mkdir resources
	xcopy /E ..\nzbget-%VERSION%\windows\resources resources
	if errorlevel 1 goto BUILD_FAILED
)

copy ..\nzbget-%VERSION%\windows\nzbget-setup.nsi .
if errorlevel 1 goto BUILD_FAILED

%NSIS%\makensis.exe nzbget-setup.nsi
if errorlevel 1 goto BUILD_FAILED

GOTO:EOF
