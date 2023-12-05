# Synology nzbget packages

## Package versions

We provide native packages for most Synology platforms (DSM 7.x). To select a correct package for yours platform please find Synology model in [Synology NAS list](https://kb.synology.com/en-us/DSM/tutorial/What_kind_of_CPU_does_my_NAS_have) and select package based on `Package Arch` field:

| Package Arch   | NZBGet package name
|:---------------|:-
| alpine         | nzbget-armv7-*.spk
| alpine4k       | nzbget-armv7-*.spk
| apollolake     | nzbget-x86_64-*.spk
| armada370      | nzbget-armada370-*.spk
| armada375      | nzbget-armada375-*.spk
| armada37xx     | nzbget-armv8-*.spk
| armadaxp       | nzbget-armadaxp-*.spk
| avoton         | nzbget-x86_64-*.spk
| braswell       | nzbget-x86_64-*.spk
| broadwell      | nzbget-x86_64-*.spk
| broadwellnk    | nzbget-x86_64-*.spk
| broadwellntb   | nzbget-x86_64-*.spk
| broadwellntbap | nzbget-x86_64-*.spk
| bromolow       | nzbget-x86_64-*.spk
| cedarview      | nzbget-x86_64-*.spk
| coffeelake     | nzbget-x86_64-*.spk
| comcerto2k     | `not supported`
| denverton      | nzbget-x86_64-*.spk
| evansport      | nzbget-i686-*.spk
| geminilake     | nzbget-x86_64-*.spk
| grantley       | nzbget-x86_64-*.spk
| kvmx64         | nzbget-x86_64-*.spk
| monaco         | nzbget-monaco-*.spk
| purley         | nzbget-x86_64-*.spk
| rtd1296        | nzbget-armv8-*.spk
| rtd1619        | nzbget-armv8-*.spk
| skylaked       | nzbget-x86_64-*.spk
| v1000          | nzbget-x86_64-*.spk

## Installing / upgrading / uninstalling / reinstalling

To install NZBGet package from Synology Package Center press Manual Install and select downloaded package -> Next -> Agree -> Select shared folder and download directory -> Credentials for web interface -> Done.
After installation NZBGet web interface will be availabe from http://[Synology NAS IP or Hostname]:6789 with provided during installation username/password (nzbget/nzbget by default). Also this link available from package center "Open" button on installed NZBGet package icon.

To upgrade nzbget package - do the same thing with new package. NZBGet settings will be keeped.

Uninstall - from Package center select NZBGet package and hit uninstall in action combo box. You can keep existing config files or cleanup all package data. If you keep existing config files, all settings selected in next package installations will be ignored, old config files settings will be preferred.

## Shared folders permissions and nzbget

When installed, the package adds all the necessary permissions for the selected Shared folder to work correcty. If you want to change the download path to another shared folder, you must manually add permissions for the nzbget user. For example - you changed MainDir to /volume2/some_shared_folder/some_download_directory. You must add r/w permission to `nzbget` user via Control Panel -> Shared Folder -> Select `some_shared_folder` -> Edit -> Permissions tab -> select from combobox `System internal user` -> nzbget -> Read/Write -> Save.

## Extensions

You can put custom extension in `ScriptDir` directory. During installation this directory appears in `selected_shared_folder\selected_download_directory\scripts` and populates with default scripts (Email and Logger). Synology DSM 7.x bundled with python 3.8, so you must make sure that the script you are installing supports it. Our forks of VideoSort/FailureLink/FakeDetector are tested and working.
