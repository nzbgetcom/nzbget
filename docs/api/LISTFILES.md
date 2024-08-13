## API-method `listfiles`

### Signature
``` c++
struct[] listfiles(int IDFrom, int IDTo, int NZBID);
```

### Description
Request for file's list of a group (nzb-file).

### Arguments
- **IDFrom** `(int)` - ~~`v12.0`~~ Deprecated, must be 0.
- **IDTo** `(int)` - ~~`v12.0`~~ Deprecated, must be 0.
- **NZBID** `(int)` - NZBID of the group to be returned.

To get the list of all files from all groups set all parameters to 0.

### Return value
This method returns an array of structures with following fields:

- **ID** `(int)` - ID of file.
- **NZBID** `(int)` - ID of NZB-file.
- **NZBFilename** `(string)` - Name of nzb-file. The filename could include fullpath (if client sent it by adding the file to queue).
- **NZBName** `(string)` - The name of nzb-file without path and extension. Ready for user-friendly output.
- **NZBNicename** `(string)` - ~~`v12.0`~~ Deprecated, use `NZBName` instead.
- **Subject** `(string)` - Subject of article (read from nzb-file).
- **Filename** `(string)` - Filename parsed from subject. It could be incorrect since the subject not always correct formated. After the first article for file is read, the correct filename is read from article body.
- **FilenameConfirmed** `(bool)` - `true` if filename was already read from article’s body. `false` if the name was parsed from subject. For confirmed filenames the destination file on disk will be exactly as specified in field `filename`. For unconfirmed filenames the name could change later.
- **DestDir** `(string)` - Destination directory for output file.
- **FileSizeLo** `(int)` - Filesize in bytes, Low 32-bits of 64-bit value.
- **FileSizeHi** `(int)` - Filesize in bytes, High 32-bits of 64-bit value.
- **RemainingSizeLo** `(int)` - Remaining size in bytes, Low 32-bits of 64-bit value.
- **RemainingSizeHi** `(int)` - Remaining size in bytes, High 32-bits of 64-bit value.
- **Paused** `(bool)` - `true` if file is paused.
- **PostTime** `(int)` - Date/time when the file was posted to newsgroup (Time is in C/Unix format).
- **Category** `(string)` - Category for group or empty string if none category is assigned.
- **Priority** `(int)` - ~~`v13.0`~~ Deprecated, use MaxPriority of the group (method [listgroups](LISTGROUPS.md)) instead.
- **ActiveDownloads** `(int)` - Number of active downloads for the file. With this filed can be determined what file(s) is (are) being currently downloaded.
- **Progress** `(int) `- `v15.0` Download progress, a number in the range 0..1000. Divide it to 10 to get percent-value.
