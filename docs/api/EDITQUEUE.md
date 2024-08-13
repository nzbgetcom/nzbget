## API-method `editqueue`

### Signature

### Since 
`v18.0`
``` c++
bool editqueue(string Command, string Param, int[] IDs);
```

### Till 
`v18.0`
```c++
bool editqueue(string Command, int Offset, string Param, int[] IDs);
```

### Description
Edit items in download queue or in history.

### Arguments
- **Command** `(string)` - one of the following commands:
  - **FileMoveOffset** - Move files relative to the current position in queue. `v18.0` Param contains offset. ~~`v18.0`~~ Offset is passed in `Offset`.
  - **FileMoveTop** - Move files to top of queue.
  - **FileMoveBottom** - Move files to bottom of queue.
  - **FilePause** - Pause files.
  - **FileResume** - Resume (unpause) files.
  - **FileDelete** - Delete files.
  - **FilePauseAllPars** - Pause only pars (does not affect other files).
  - **FilePauseExtraPars** - Pause only pars, except main par-file (does not affect other files).
  - **FileSetPriority** - ~~`v13.0`~~ Deprecated, use GroupSetPriority instead.
  - **FileReorder** - Reorder files in the group. The list of IDs may include files only from one group.
  - **FileSplit** - Split nzb-file. The list of IDs contains the files to move into new download item.
  - **GroupMoveOffset** - Move groups relative to the current position in queue. `v18.0` Param contains offset. ~~`v18.0`~~ Offset is passed in Offset.
  - **GroupMoveTop** - Move groups to top of queue.
  - **GroupMoveBottom** - Move groups to bottom of queue.
  - **GroupPause** - Pause groups.
  - **GroupResume** - Resume (unpause) groups.
  - **GroupDelete** - Delete groups and put to history.
  - **GroupDupeDelete** - Delete groups, put to history and mark as duplicate.
  - **GroupFinalDelete** - Delete groups without adding to history.
  - **GroupPauseAllPars** - Pause only pars (does not affect other files).
  - **GroupPauseExtraPars** - Pause only pars, except main par-file (does not affect other files).
  - **GroupSetPriority** - Set priority for all files in group. Param contains priority value.
  - **GroupSetCategory** - Set category for group. Param contains category name.
  - **GroupApplyCategory** - Set or change category for groups and reassign pp-params according to category settings. `Param` contains category name.
  - **GroupMerge** - Merge groups.
  - **GroupSetParameter** - Set post-processing parameter for group. `Param` contains string in form of `Paramname=Paramvalue`.
  - **GroupSetName** - Rename group. Param contains new name.
  - **GroupSetDupeKey** - Set duplicate key. Param contains duplicate key. See RSS _(comming soon)_.
  - **GroupSetDupeScore** - Set duplicate score. Param contains duplicate score. See RSS _(comming soon)_.
  - **GroupSetDupeMode** - Set duplicate mode. Param contains one of `SCORE`, `ALL`, `FORCE`. See RSS _(comming soon)_.
  - **GroupSort** - `v15.0` Sort selected or all groups. Parameter `Param` must be one of: `name`, `priority`, `category`, `size`, `left`; add character `+` or `-` to sort to explicitly define ascending or descending order (for example `name-`); if none of these characters is used the auto-mode is active: the items are sorted in ascending order first, if nothing changed - they are sorted again in descending order. `Parameter IDs` contains the list of groups to sort; pass empty array to sort all groups.
  - **PostMoveOffset** - ~~`v13.0`~~ Deprecated, use `GroupMoveOffset` instead.
  - **PostMoveTop** - ~~`v13.0`~~ Deprecated, use `GroupMoveTop` instead.
  - **PostMoveBottom** - ~~`v13.0`~~ Deprecated, use `GroupMoveBottom instead.
  - **PostDelete** - Delete post-jobs.
  - **HistoryDelete** - Hide history items (mark as hidden).
  - **HistoryFinalDelete** - Delete history items.
  - **HistoryReturn** - Return history items back to download queue.
  - **HistoryProcess** - Post-process history items again.
  - **HistoryRedownload** - Move history items back to download queue for redownload.
  - **HistorySetName** - `v15.0` Rename history item. `Param` contains new name.
  - **HistorySetCategory** - `v15.0` Set category for history item. Param contains category name.
  - **HistorySetParameter** - Set post-processing parameter for history items. `Param` contains string in form of `Paramname=Paramvalue`.
  - **HistorySetDupeKey** - Set duplicate key. `Param` contains duplicate key. See RSS _(comming soon)_.
  - **HistorySetDupeScore** - Set duplicate score. `Param` contains duplicate score. See RSS _(comming soon)_.
  - **HistorySetDupeMode** - Set duplicate mode. `Param` contains one of `SCORE`, `ALL`, `FORCE`. See RSS _(comming soon)_.
  - **HistorySetDupeBackup** - Set `use as duplicate backup-flag` for history items. `Param` contains `0` or `1`. See RSS _(comming soon)_.
  - **HistoryMarkBad** - Mark history item as bad (and download other duplicate). See RSS _(comming soon)_.
  - **HistoryMarkGood** - Mark history item as good. See RSS _(comming soon)_.
  - **HistoryMarkSuccess** - `v15.0` Mark history item as success. See RSS _(comming soon)_.
- **Offset (int)** - ~~`v18.0`~~ offset for commands `FileMoveOffset` and `GroupMoveOffset`. For all other commands must be `0`. `v18.0` Offset is passed in `Param` and parameter `Offset` should not be passed at all.
- **Param `(string)` - additional parameter if mentioned in the command description, otherwise an empty string.
- **IDs** `(struct[])` - array of IDs (as integers).
  - File-commands (FileXXX) need ID of file.
  - All other commands need `NZBID`.

### Return value
`true` on success or `false` on failure.
