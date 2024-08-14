## API-method `configtemplates`

### Signature
``` c++
struct[] configtemplates(bool LoadFromDisk)
```

### Description
Returns NZBGet configuration file template and also extracts configuration sections from all post-processing files. This information is for example used by web-interface to build settings page or page `Postprocess` in download details dialog.

### Arguments
- **LoadFromDisk** `(bool)` - `v15.0` `true` - load templates from disk, `false` - give templates loaded on program start.

## Since 
`v23.0`

### Return value
This method returns array of structures with a field:

- **Template** `(string)` - Content of the configuration template (multiple lines).

## Till 
`v23.0`

### Return value
This method returns array of structures with following fields:

- **Name** `(string)` - Post-processing script name. For example `videosort/VideoSort.py`. This field is empty in the first record, which holds the config template of the program itself.
- **DisplayName** `(string)` - Nice script name ready for displaying. For example `VideoSort`.
- **PostScript** `(bool)` - `true` for post-processing scripts.
- **ScanScript** `(bool)` - `true` for scan scripts.
- **QueueScript** `(bool)` - `true` for queue scripts.
- **SchedulerScript** `(bool)` - `true` for scheduler scripts.
- **Template** `(string)` - Content of the configuration template (multiple lines).
