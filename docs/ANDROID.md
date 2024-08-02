# Installation on Android

## Installation
All steps to perform on your Android device:
- using web-browser download NZBGet installer app https://github.com/nzbgetcom/android/releases

  **NOTE**: this app is only a frontend to NZBGet downloader background process, which
  the app installs and launches. The app isn't updated often but it always installs the latest version of nzbget by downloading it from NZBGet download page.

  **NOTE**: the installer app requires Android 5 or later.

- install downloaded **apk-file**. You may need to enable a setting allowing installation of apps outside of Play store
- launch NZBGet app
- click **INSTALL DAEMON**
- choose version to install
- the installer app downloads the Android installer package of NZBGet and performs the installation
- the success of installation is confirmed with message "NZBGet daemon has been successfully installed"

## Usage
- to start downloader process launch NZBGet app and choose **START DAEMON**. You get a confirmation about successful start.
- in NZBGet app choose **Show web-interface**; that opens a web-browser. Alternatively open a web-browser manually and navigate to **http://localhost:6789**. Alternatively open a web-browser on your PC or any other device on the same network and navigate to **http://ip-of-android-device:6789**.
- at this point you can close NZBGet app, the daemon process remains running in background;
- use web-interface to control NZBGet daemon.

## Accessing downloaded files

On the external storage, Android apps are only allowed to write into the app specific folder (e. g. '/storage/XXXX-XXXX/Android/data/net.nzbget.nzbget').

To work around this, you may choose any path on the external storage using the **CHOOSE PATHS** button in installer app. Downloads will be moved to these paths after they have finished successfully. All downloads will be moved to the default path unless a path for the category of a specific download is chosen, in which case it will be moved there instead.

**NOTE**: This feature will only move finished downloads from the DestDir you chose in the NZBGet Settings, so don't remove your DestDir in the web interface.

## Post-processing scripts
Most scripts are written in Python. Unfortunately there is no Python for Android. That means most scripts will not work. Bash scripts may work but you may need to install BusyBox from Play store. You also must configure option **ScriptDir** to system partition (scripts cannot be launched from sdcard). Post-processing scripts do not work properly due to OS limitations.
