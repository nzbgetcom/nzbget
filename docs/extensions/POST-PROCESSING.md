## About

After the download of nzb-file is completed NZBGet can call post-processing extensions (pp-extensions). 
The extensions can perform further processing of downloaded files such es delete unwanted files (*.url, etc.), 
send an e-mail notification, transfer the files to other application and do any other things. 
Please note that the par-check/repair and unpack are performed by NZBGet internally and are not part of 
post-processing extensions. You can activate par-check/repair and unpack without using of any post-processing extensions.

Please note that these settings define "defaults" for nzb-file. When you deactivate/activate the extension in the Extension Manager
this has no effect on already enqueued downloads. However it’s possible to alter the assigned pp-extensions 
for each nzb-file individually. To do so click on the nzb-file in the list of downloads, then click on Postprocess.

If you use more than one extension for one nzb-file it can be important to set the correct order of execution 
in the Extension Manager.

## Writing post-processing extensions

> Please read [Extensions](EXTENSIONS.md) for general information about extensions first.

## Nzb-file information

### The information about nzb-file which is currently processed:

- **NZBPP_DIRECTORY** - Path to destination dir for downloaded files.
- **NZBPP_FINALDIR** - Final directory, if set by one of previous scripts (see below).
- **NZBPP_NZBNAME** - User-friendly name of processed nzb-file as it is displayed by the program. 
 The file path and extension are removed. If download was renamed, this parameter reflects the new name.
- **NZBPP_NZBFILENAME** - Name of processed nzb-file. If the file was added from incoming nzb-directory, 
 this is a full file name, including path and extension. If the file was added from web-interface, 
 it’s only the file name with extension. If the file was added via RPC-API (method append), 
 this can be any string but the use of actual file name is recommended for developers.
- **NZBPP_CATEGORY** - Category assigned to nzb-file (can be empty string).
- **NZBPP_TOTALSTATUS** - `v13.0` Total status of nzb-file:
  - **SUCCESS** - everything `OK`;
  - **WARNING** - download is damaged but probably can be repaired; user intervention is required;
  - **FAILURE** - download has failed or a serious error occurred during post-processing (unpack, par);
  - **DELETED** - download was deleted; post-processing scripts are usually not called in this case; 
 however it’s possible to force calling scripts with command "post-process again".
- **NZBPP_STATUS** - Complete status info for nzb-file: it consists of total status and status detail separated with slash. 
 There are many combinations. Just few examples:
  - **FAILURE/HEALTH**;
  - **FAILURE/PAR**;
  - **FAILURE/UNPACK**;
  - **WARNING/REPAIRABLE**;
  - **WARNING/SPACE**;
  - **WARNING/PASSWORD**;
  - **SUCCESS/ALL**;
  - **SUCCESS/UNPACK**.
For the complete list see description of [API-Method "history"](../api/HISTORY.md).
  - **NOTE** one difference to the status returned by method history is that `NZBPP_STATUS` assumes all scripts are ended successfully. Even if one of the scripts executed before the current one has failed the status will not be set to `WARNING/SCRIPT` as method history will do. For example for a successful download the status would be `SUCCESS/ALL` instead. Because most scripts don’t depend on other scripts they shouldn’t assume the download has failed if any of the previous scripts (such as a notification script) has failed. The scripts interested in that info still can use parameter `NZBPP_SCRIPTSTATUS`.
- **NZBPP_SCRIPTSTATUS** - `v13.0` Summary status of the scripts executed before the current one:
  - **NONE** - no other scripts were executed yet or all of them have ended with exit code `NONE`;
  - **SUCCESS** - all other scripts have ended with exit code "SUCCESS" ;
  - **FAILURE** - at least one of the script has failed.
- **NZBPP_PARSTATUS** - `v13.0` Result of par-check:
  - **0** = not checked: par-check is disabled or nzb-file does not contain any par-files;
  - **1** = checked and failed to repair;
  - **2** = checked and successfully repaired;
  - **3** = checked and can be repaired but repair is disabled;
  - **4** = par-check needed but skipped (option ParCheck=manual).
  - `Deprecated, use NZBPP_STATUS and NZBPP_TOTALSTATUS instead.`
- **NZBPP_UNPACKSTATUS** - `v13.0` Result of unpack:
  - **0** = unpack is disabled or was skipped due to nzb-file properties or due to errors during par-check;
  - **1** = unpack failed;
  - **2** = unpack successful;
  - **3** = write error (usually not enough disk space);
  - **4** = wrong password (only for rar5 archives).
  - `Deprecated, use NZBPP_STATUS and NZBPP_TOTALSTATUS instead.`

### Example: test if download failed
```sh
if [ "$NZBPP_TOTALSTATUS" != "SUCCESS" ]; then
    echo "[ERROR] This nzb-file was not processed correctly, terminating the script"
    exit 95
fi
```

## Exit codes
For post-processing extensions NZBGet checks the exit code of the script and sets the status in history item accordingly. 
Extensions can use following exit codes:
- **93** - Post-process successful (`status = SUCCESS`).
- **94** - Post-process failed (`status = FAILURE`).
- **95** - Post-process skipped (`status = NONE`). Use this code when you script terminates immediately without doing any job and when this is not a failure termination.
- **92** - Request NZBGet to do par-check/repair for current nzb-file. This code can be used by pp-scripts doing unpack on their own.
Extensions MUST return one of the listed status codes. 
If any other code (including 0) is returned the history item is marked with status `FAILURE`.

Example: exit code
```sh
POSTPROCESS_SUCCESS = 93
POSTPROCESS_ERROR = 94
echo "Hello from test script"
exit $POSTPROCESS_SUCCESS
```

## Control commands

pp-extensions which move downloaded files can inform NZBGet about that by printing special 
messages into standard output (which is processed by NZBGet). 
This allows the extensions called thereafter to process the files in the new location. Use command `DIRECTORY`:
```sh
echo "[NZB] DIRECTORY=/path/to/new/location"
```
The extensions executed after your script assume the files in the directory belong to the current download. 
If your extension moves files into a non empty directory which already contains other files, your should not use that command. 
Otherwise the other extensions may process all files in the directory, even the files existed before. 
In that case use command `FINALDIR` instead (which has less consequences):
```sh
echo "[NZB] FINALDIR=/path/to/new/location"
```
