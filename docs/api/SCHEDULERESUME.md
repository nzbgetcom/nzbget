## API-method `scheduleresume`

### Signature
``` c++
bool scheduleresume(int Seconds);
```

### Description
Schedule resuming of all activities after expiring of wait interval.

### Arguments
- **Seconds** `(int)` - Amount of seconds to wait before resuming.

### Return value
`true` on success or `failure` result on error.

### Remarks
Method `scheduleresume` activates a wait timer. When the timer fires then all pausable activities resume: download, post-processing and scan of incoming directory. This method doesn’t pause anything, activities must be paused before calling this method.

Calling any of methods `pausedownload`, `resumedownload`, `pausepost`, `resumepost`, `pausescan`, `resumescan` deactivates the wait timer.
