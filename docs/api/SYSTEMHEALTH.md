## API-method `systemhealth`

## Since
`v26.0`

### Status
**Experimental (Not recommended for use)**

### Signature
``` c++
struct systemhealth();
```

### Description
Performs a diagnostic validation of the running configuration. The result is a structured report grouping findings by section, option and subsection.

### Return value
This method returns a structure with the following fields:

- **Status** `(String)` - Overall health severity: **Ok**, **Info**, **Warning**, **Error**.

- **Alerts** `(struct[])` - Flat list of problematic configuration options across all sections.
    - **Name** `(String)` - Option name (e.g. `ControlPassword`, `MainDir`).
    - **Status** `(struct)` - The status object with `Severity` and `Message`.

- **Sections** `(struct[])` - Detailed per-section reports. Each `SectionReport` contains:
    - **Name** `(string)` - Section name (e.g. `Paths`, `Unpack`).
    - **Alerts** `(struct[])` - General alerts for the section.
    - **Options** `(struct[])` - Per-option checks:
        - **Name** `(string)` - Option name.
        - **Status** `(struct)` - `Severity` and `Message`.
    - **Subsections** `(struct[])` - Nested reports (e.g. individual news servers, categories).

### Sections
The health check covers multiple sections (representative list):

- `Paths` — Main directories and writable/readable checks.
- `NewsServers` — Per-server configuration checks.
- `Security` — Authentication, control port/usability, TLS cert/key.
- `Categories` — Category definitions and paths.
- `Feeds` — RSS/Feed configuration checks.
- `IncomingNzb` — Incoming NZB directory options.
- `DownloadQueue` — Cache, write buffer, direct-write checks.
- `Connection` — Connection and proxy related options.
- `Logging` — Log file and rotation settings.
- `SchedulerTasks` — Scheduler-related checks.
- `CheckAndRepair` — PAR/repair and CRC checks.
- `Unpack` — Unpacker/extension handling.
- `ExtensionScripts` — Script availability and permissions.

### Example
```json
{
    "Status": "Warning",
    "Alerts": [ 
        { 
            "Name": "MainDir", 
            "Status": 
            { 
                "Severity": "Error", 
                "Message": "MainDir cannot be empty." 
            } 
        } 
    ],
    "Sections": 
    [ 
        { 
            "Name": "Paths", 
            "Alerts": [], 
            "Options": 
            [
                { 
                    "Name": "MainDir", 
                    "Status": 
                    { 
                        "Severity": "Error", 
                        "Message": "MainDir cannot be empty." 
                    } 
                } 
            ], 
            "Subsections": [] 
        },
        {
            "Name": "NewsServers",
            "Alerts": [],
            "Options": [],
            "Subsections": [
                {
                    "Name": "Server1",
                    "Options": 
                    [
                        {
                            "Name": "Host",
                            "Status":
                            {
                                "Severity": "Error",
                                "Message": "Hostname cannot be empty."
                            }
                        }
                    ]
                }
            ]
        }
    ]
}
```
