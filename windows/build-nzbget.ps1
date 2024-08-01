#
#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2024 phnzb <pavel@nzbget.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# build params
Param (
    [switch]$BuildDebug=$False,
    [switch]$BuildRelease=$False,
    [switch]$Build32=$False,
    [switch]$Build64=$False,
    [switch]$BuildSetup=$False,
    [switch]$BuildTesting=$False,
    [switch]$DownloadUnpackers=$False
)

If (-not $BuildDebug -and
    -not $BuildRelease -and
    -not $Build32 -and
    -not $Build64 -and
    -not $BuildSetup -and
    -not $BuildTesting -and
    -not $DownloadUnpackers) {
    # script running without params
    $BuildRelease=$True
    $Build32=$True
    $Build64=$True
    $BuildSetup=$True
}

# tools locations
$ToolsRoot="C:\nzbget"
$Sed="$ToolsRoot\sed\sed.exe"
$Nsis="$ToolsRoot\nsis"
$VcpkgDir="c:\vcpkg"

# global ps params
# stop on error
$ErrorActionPreference = "Stop"
#  do not display progress bars
$ProgressPreference = "SilentlyContinue"

# download 7z/unrar 32/64 bit to $ToolsRoot\image
Function DownloadUnpackers {
    $UrlUnrar64="https://www.rarlab.com/rar/unrarw64.exe"
    $UrlRar32="https://www.rarlab.com/rar/winrar-x32-700.exe"
    $UrlRar64="https://www.rarlab.com/rar/winrar-x64-700.exe"
    $Url7Z="https://www.7-zip.org/a/7z2301-extra.7z"

    $ImageDir="$ToolsRoot\image"
    Write-Host "Downloading unpackers to $ImageDir"

    $UnpackDir="$BuildDir\unpack"
    New-Item -ItemType Directory $UnpackDir | Out-Null

    # download unrar64
    Invoke-WebRequest -Uri $UrlUnrar64 -OutFile $UnpackDir\unrarw64.exe
    Start-Process -NoNewWindow -Wait "$UnpackDir\unrarw64.exe" -ArgumentList -d"$UnpackDir\unrar64",-s

    # download specific releases of winrar 32/64 bit
    Invoke-WebRequest -Uri $UrlRar32 -OutFile $UnpackDir\rar32.exe
    New-Item -ItemType Directory "$UnpackDir\rar32" | Out-Null
    Start-Process -NoNewWindow -Wait "$UnpackDir\unrar64\unrar.exe" -ArgumentList x,"$UnpackDir\rar32.exe","$UnpackDir\rar32\"

    Invoke-WebRequest -Uri $UrlRar64 -OutFile $UnpackDir\rar64.exe
    New-Item -ItemType Directory "$UnpackDir\rar64" | Out-Null
    Start-Process -NoNewWindow -Wait "$UnpackDir\unrar64\unrar.exe" -ArgumentList x,"$UnpackDir\rar64.exe","$UnpackDir\rar64\"

    # 7zip
    Invoke-WebRequest -Uri $Url7Z -OutFile $UnpackDir\7zip.7z
    New-Item -ItemType Directory "$UnpackDir\7zip" | Out-Null
    Start-Process -NoNewWindow -Wait "$UnpackDir\rar64\winrar.exe" -ArgumentList x,"$UnpackDir\7zip.7z","$UnpackDir\7zip"

    # copy needed files
    If (Test-Path $ImageDir) {
        Remove-Item $ImageDir -Force -Recurse
    }
    New-Item -ItemType Directory "$ImageDir\32" | Out-Null
    New-Item -ItemType Directory "$ImageDir\64" | Out-Null
    Copy-Item "$UnpackDir\rar32\unrar.exe" "$ImageDir\32\unrar.exe"
    Copy-Item "$UnpackDir\rar64\unrar.exe" "$ImageDir\64\unrar.exe"
    Copy-Item "$UnpackDir\7zip\7za.exe" "$ImageDir\32\7za.exe"
    Copy-Item "$UnpackDir\7zip\x64\7za.exe" "$ImageDir\64\7za.exe"

    # cleanup
    Remove-Item $UnpackDir -Force -Recurse
}

# prepare package files
Function PrepareFiles {
    If (-not (Test-Path "$ToolsRoot\image")) {
        Write-Host "Cannot find extra files for setup. Exiting ..."
        Exit 1
    }

    Write-Host "Preparing files for setup"
    Remove-Item $DistribDir -Force -Recurse -ErrorAction Ignore
    New-Item -Path $DistribDir -ItemType Directory -Force | Out-Null
    New-Item -Path "$PackageDir\32" -ItemType Directory -Force | Out-Null
    New-Item -Path "$PackageDir\64" -ItemType Directory -Force | Out-Null
    If (-not $Build32) {
        Write-Output "This test setup doesn't include binaries for 32 bit platform" | Out-File "$PackageDir\32\README-WARNING.txt"
    }
    If (-not $Build64) {
        Write-Output "This test setup doesn't include binaries for 64 bit platform" | Out-File "$PackageDir\64\README-WARNING.txt"
    }

    Copy-Item windows\nzbget-command-shell.bat $PackageDir
    Copy-Item windows\install-update.bat $PackageDir
    Copy-Item windows\README-WINDOWS.txt $PackageDir
    Copy-Item ChangeLog.md $PackageDir
    # not needeed anymore
    # Copy-Item INSTALLATION.md $PackageDir
    Copy-Item COPYING $PackageDir
    Copy-Item pubkey.pem $PackageDir
    Copy-Item webui $PackageDir -Recurse
    Copy-Item windows\package-info.json $PackageDir\webui

    Write-Host "Updating root certificates"
    Invoke-WebRequest -Uri "https://curl.se/ca/cacert.pem" -OutFile "$PackageDir\cacert.pem"

    Write-Host "Adjusting config file"
    $Config="$PackageDir\nzbget.conf.template"
    Copy-Item "$SrcDir\nzbget.conf" $Config
    & $Sed -e 's|MainDir=.*|MainDir=${AppDir}\\downloads|' -i $Config
    & $Sed -e 's|DestDir=.*|DestDir=${MainDir}\\complete|' -i $Config
    & $Sed -e 's|InterDir=.*|InterDir=${MainDir}\\intermediate|' -i $Config
    & $Sed -e 's|ScriptDir=.*|ScriptDir=${MainDir}\\scripts|' -i $Config
    & $Sed -e 's|LogFile=.*|LogFile=${MainDir}\\nzbget.log|' -i $Config
    & $Sed -e 's|AuthorizedIP=.*|AuthorizedIP=127.0.0.1|' -i $Config
    & $Sed -e 's|UnrarCmd=.*|UnrarCmd=${AppDir}\\unrar.exe|' -i $Config
    & $Sed -e 's|SevenZipCmd=.*|SevenZipCmd=${AppDir}\\7za.exe|' -i $Config
    & $Sed -e 's|ArticleCache=.*|ArticleCache=250|' -i $Config
    & $Sed -e 's|ParBuffer=.*|ParBuffer=250|' -i $Config
    & $Sed -e 's|WriteBuffer=.*|WriteBuffer=1024|' -i $Config
    & $Sed -e 's|CertStore=.*|CertStore=${AppDir}\\cacert.pem|' -i $Config
    & $Sed -e 's|CertCheck=.*|CertCheck=yes|' -i $Config
    & $Sed -e 's|DirectRename=.*|DirectRename=yes|' -i $Config
    & $Sed -e 's|DirectUnpack=.*|DirectUnpack=yes|' -i $Config
    # Hide certain options from web-interface settings page
    & $Sed -e 's|WebDir=.*|# WebDir=${AppDir}\\webui|' -i $Config
    & $Sed -e 's|LockFile=.*|# LockFile=|' -i $Config
    & $Sed -e 's|ConfigTemplate=.*|# ConfigTemplate=${AppDir}\\nzbget.conf.template|' -i $Config
    & $Sed -e 's|DaemonUsername=.*|# DaemonUsername=|' -i $Config
    & $Sed -e 's|UMask=.*|# UMask=|' -i $Config

    New-Item -ItemType Directory "$PackageDir\scripts" | Out-Null
    Copy-Item "scripts\*" "$PackageDir\scripts"

    Copy-Item "$ToolsRoot\image\32\*" "$PackageDir\32"
    Copy-Item "$ToolsRoot\image\64\*" "$PackageDir\64"
}

# build nzbget binary - release/debug 32/64 bit
Function BuildTarget($Type, $Bits) {

    If ($Bits -eq "32") {
        $Arch="x86"
        $Platform="Win32"
    } Else {
        $Arch="x64"
        $Platform="x64"
    }

    New-Item -Path "$BuildDir\$Type$Bits" -ItemType Directory -Force | Out-Null
    Set-Location "$BuildDir\$Type$Bits"

    $CMakeCmd="cmake ..\.. -DCMAKE_TOOLCHAIN_FILE=$VcpkgDir\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=$Arch-windows-static -A $Platform"

    If ($Type -eq "Debug" ) {
        $CMakeCmd="$CMakeCmd -DCMAKE_BUILD_TYPE=Debug"
    }

    if ($BuildTesting) {
        $CMakeCmd="$CMakeCmd -DVERSION_SUFFIX=$VersionSuffix"
    }

    Write-Host "Building nzbget binary for $Type-$Arch$VersionSuffix"
    Write-Host $CMakeCmd

    Invoke-Expression "& $CMakeCmd"
    If (-not $?) { Set-Location $SrcDir; Exit 1 }

    & cmake --build . --config $Type
    If (-not $?) { Set-Location $SrcDir; Exit 1 }

    Copy-Item "$Type\nzbget.exe" "$SrcDir\$PackageDir\$Bits\"
    Set-Location $SrcDir
}

# build nsis installer
Function BuildSetup($Type) {
    Write-Host "Building NSIS setup package ($Type)"

    Copy-Item -Recurse -Force windows\resources $DistribDir
    If ($Build32) {
        $Bits="32"
    } Else {
        $Bits="64"
    }
    Copy-Item "windows\nzbget-setup.nsi" $DistribDir
    Copy-Item "$BuildDir\$Type$Bits\version.nsi" $DistribDir
    Copy-Item "$BuildDir\$Type$Bits\version-uninstall.nsi" $DistribDir
    Set-Location $DistribDir
    & $Nsis\makensis.exe nzbget-setup.nsi
    If (-not $?) { Set-Location $SrcDir; Exit 1 }
    Set-Location $SrcDir

    If ($Type -eq "Release") {
        $InstallerFile="nzbget-$Version$VersionSuffix-bin-windows-setup.exe"
    } Else {
        $InstallerFile="nzbget-$Version$VersionSuffix-bin-windows-debug-setup.exe"
    }

    Move-Item "$DistribDir\nzbget-setup.exe" "$BuildDir\$InstallerFile" -Force
}

# build package release/debug
Function Build($Type) {
    PrepareFiles
    if ($Build32) {
        BuildTarget $Type "32"
    }
    if ($Build64) {
        BuildTarget $Type "64"
    }
    if ($BuildSetup) {
        BuildSetup $Type
    }
}

# script begins here
$SrcDir=Get-Location | Select-Object -ExpandProperty Path
$BuildDir="build"
$DistribDir="$BuildDir\distrib"
$PackageDir="$DistribDir\nzbget"

If ($BuildTesting) {
    $VersionSuffix="-testing-$(Get-Date -Format "yyyyMMdd")"
}

$Version = ((Select-String -Path CMakeLists.txt -Pattern "set\(VERSION ")[0] -split('"'))[1]

# clean build folder
If (Test-Path $BuildDir) {
    Remove-Item $BuildDir -Force -Recurse
}
New-Item -ItemType Directory $BuildDir | Out-Null

# download 7z/unrar
if ($DownloadUnpackers) {
    DownloadUnpackers
}

if ($BuildRelease -or $BuildDebug) {
    Write-Host "Building nzbget version $Version (Release:$BuildRelease Debug:$BuildDebug 32-bit:$Build32 64-bit:$Build64 Setup:$BuildSetup Testing:$BuildTesting)"
    New-Item -ItemType Directory $PackageDir | Out-Null
}

# build
if ($BuildRelease) {
    Build("Release")
}

if ($BuildDebug) {
    Build("Debug")
}
