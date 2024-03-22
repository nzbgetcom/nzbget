# build params
Param (
    [switch]$BuildDebug=$False,
    [switch]$BuildRelease=$False,
    [switch]$Build32=$False,
    [switch]$Build64=$False,
    [switch]$BuildSetup=$False,
    [switch]$BuildTesting=$False
)

If (-not $BuildDebug -and -not $BuildRelease -and -not $Build32 -and -not $Build64 -and -not $BuildSetup -and -not $BuildTesting) {
    # script running without params
    $BuildRelease=$True
    $Build32=$True
    $Build64=$True
    $BuildSetup=$True
}

# tools locations
$Root="C:\nzbget\build"
$Sed="$Root\tools\sed\sed.exe"
$Curl="$Root\tools\curl\curl.exe"
$Nsis="$Root\tools\nsis"
$VcpkgDir="c:\vcpkg"

# stop on error
$ErrorActionPreference = "Stop"

# prepare package files
Function PrepareFiles {
    If (-not (Test-Path "$Root\image")) {
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
    Copy-Item ChangeLog $PackageDir
    # not needeed anymore
    # Copy-Item INSTALLATION.md $PackageDir
    Copy-Item COPYING $PackageDir
    Copy-Item pubkey.pem $PackageDir
    Copy-Item webui $PackageDir -Recurse
    Copy-Item windows\package-info.json $PackageDir\webui

    Write-Host "Updating root certificates"
    & $Curl -s -o "$DistribDir\cacert.pem" https://curl.se/ca/cacert.pem
    If (-not $?) { Exit 1 }

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

    Copy-Item "$Root\image\32\*" "$PackageDir\32"
    Copy-Item "$Root\image\64\*" "$PackageDir\64"
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

# script begins here
$SrcDir=Get-Location | Select-Object -ExpandProperty Path
$BuildDir="build"
$DistribDir="$BuildDir\distrib"
$PackageDir="$DistribDir\nzbget"

If ($BuildTesting) {
    $VersionSuffix="-testing-$(Get-Date -Format "yyyyMMdd")"
}

$Version = ((Select-String -Path CMakeLists.txt -Pattern "set\(VERSION ")[0] -split('"'))[1]
Write-Host "Building nzbget version $Version (Release:$BuildRelease Debug:$BuildDebug 32-bit:$Build32 64-bit:$Build64 Setup:$BuildSetup Testing:$BuildTesting)"

# clean build folder
Remove-Item $BuildDir -Force -Recurse
New-Item -ItemType Directory $PackageDir | Out-Null

# release actions
if ($BuildRelease) {
    PrepareFiles
    if ($Build32) {
        BuildTarget "Release" "32"
    }
    if ($Build64) {
        BuildTarget "Release" "64"
    }
    if ($BuildSetup) {
        BuildSetup "Release"
    }
}

# debug actions
if ($BuildDebug) {
    PrepareFiles
    if ($Build32) {
        BuildTarget "Debug" "32"
    }
    if ($Build64) {
        BuildTarget "Debug" "64"
    }
    if ($BuildSetup) {
        BuildSetup "Debug"
    }
}
