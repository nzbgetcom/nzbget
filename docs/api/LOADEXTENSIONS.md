## API-method `loadextensions`

## Since 
`v23.0`

### Signature
``` c++
struct[] loadextensions(bool LoadFromDisk);
```

### Description
Reads extensions from the disk.

### Arguments
- **LoadFromDisk** `(bool)` - `true` - load extensions from disk, `false` - give extensions loaded on program start.

### Return value
This method returns array of structures.

- **Entry** `(string)`
- **Location** `(string)`
- **RootDir** `(string)`
- **Name** `(string)`
- **DisplayName** `(string)`
- **About** `(string)`
- **Author** `(string)`
- **Homepage** `(string)`
- **License** `(string)`
- **Version** `(string)`
- **NZBGetMinVersion** `(string)`
- **PostScript** `(bool)`
- **ScanScript** `(bool)`
- **QueueScript** `(bool)`
- **SchedulerScript** `(bool)`
- **FeedScript** `(bool)`
- **QueueEvents** `(string)`
- **TaskTime** `(string)`
- **Description** `(string[])`
- **Requirements** `(string[])`
- **Options** `(struct[])`
  - **Name** `(string)`
  - **DisplayName** `(string)`
  - **Description** `(string[])`
  - **Select** `(string[])`
- **Commands** `(struct[])`
  - **Name** `(string)`
  - **Action** `(string)`
  - **DisplayName** `(string)`
  - **Description** `(struct[])`
- **Sections** `(struct[])`
  - **Name** `(string)`
  - **Prefix** `(string)`
  - **Multi** `(bool)`

See [Extensions](../extensions/EXTENSIONS.md).
