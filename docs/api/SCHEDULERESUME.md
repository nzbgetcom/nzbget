## API-method `scheduleresume`

### Signature
``` c++
bool scheduleresume(int Seconds);
```

_Schedule resuming of all activities after expiring of wait interval_

### Arguments
- `Seconds (int)` - Amount of seconds to wait before resuming.

### Return value
Always `true`.

### Remarks
Method `scheduleresume` activates a wait timer. When the timer fires then all pausable activities resume: download, post-processing and scan of incoming directory. This method doesn’t pause anything, activities must be paused before calling this method.

Calling any of methods `pausedownload`, `resumedownload`, `pausepost`, `resumepost`, `pausescan`, `resumescan` deactivates the wait timer.
