## API-method `servervolumes`

### Signature
``` c++
struct[] servervolumes();
```

### Description
Returns download volume statistics per news-server.

### Return value
This method returns an array of structures with following fields:

- **ServerID** `(int)` - ID of news server.
- **DataTime** `(int)` - Date/time when the data was last updated (time is in C/Unix format).
- **TotalSizeLo** `(int)` - Total amount of downloaded data since program installation, low 32-bits of 64-bit value.
- **TotalSizeHi** `(int)` - Total amount of downloaded data since program installation, high 32-bits of 64-bit value.
- **TotalSizeMB** `(int)` - Total amount of downloaded data since program installation, in MiB.
- **CustomSizeLo** `(int)` - Amount of downloaded data since last reset of custom counter, low 32-bits of 64-bit value.
- **CustomSizeHi** `(int)` - Amount of downloaded data since last reset of custom counter, high 32-bits of 64-bit value.
- **CustomSizeMB** `(int)` - Amount of downloaded data since last reset of custom counter, in MiB.
- **CustomTime `(int)` - Date/time of the last reset of custom counter (time is in C/Unix format).
- **BytesPerSeconds** `(struct[])` - Per-second amount of data downloaded in last 60 seconds. See below.
- **BytesPerMinutes** `(struct[])` - Per-minute amount of data downloaded in last 60 minutes. See below.
- **BytesPerHours** `(struct[])` - Per-hour amount of data downloaded in last 24 hours. See below.
- **BytesPerDays** `(struct[])` - Per-day amount of data downloaded since program installation. See below.
- **SecSlot** `(int)` - The current second slot of field `BytesPerSeconds` the program writes into.
- **MinSlot** `(int)` - The current minute slot of field `BytesPerMinutes` the program writes into.
- **HourSlot** `(int)` - The current hour slot of field `BytesPerHours` the program writes into.
- **DaySlot** `(int)` - The current day slot of field `BytesPerDays` the program writes into.
- **FirstDay** `(int)` - Indicates which calendar day the very first slot of `BytesPerDays` corresponds to. Details see below.

**NOTE**: The first record (serverid=0) are totals for all servers.

### Field BytesPerSeconds, BytesPerMinutes, BytesPerHours, BytesPerDays
Contains an array of structs with following fields:

- **SizeLo** `(int)` - Amount of downloaded data, low 32-bits of 64-bit value.
- **SizeHi** `(int)` - Amount of downloaded data, high 32-bits of 64-bit value.
- **SizeMB** `(int)` - Amount of downloaded data, in MiB.

### Seconds, minutes and hours slots
These slots are arrays of fixed sizes (60, 60 and 24) which contain data for the last 60 seconds, 60 minutes and 24 hours. For example if current time “16:00:21” when the current slots would be:

- `SecSlot = 21`;
- `MinSlot = 0`;
- `HourSlot = 16`.
Element 21 of `BytesPerSeconds` contains the amount of data downloaded in current second. Element 20 - in the previous second and element 22 - data downloaded 59 seconds ago.

Similarly for minutes `(BytesPerMinutes)` and hours `(BytesPerHours)` arrays.

### Daily slots
Daily slots are counted from the installation date, more precisely - from the date the program was used the first time after the installation. Or the first time it was used after deleting of statistics data, which is stored in directory pointed by option `QueueDir`.

Therefore the first element of `BytesPerDays` array contains the amount of data downloaded at the first day of program usage. The second element - on the second day. The sub-sequential slots are created for each day, regardless of the fact if the program was running on this day or not.

Field `DaySlot` shows into which day slot the data is currently being written.

To find out the day slot for a specific day:

- get the timestamp for any time (hour/minute/second) of this day, in C/Unix format. The C/Unix time format is defined as the number of seconds that have elapsed since 00:00:00, Thursday, 1 January 1970.
- divide this number by the number of seconds in one day `(24*60*60=86400)`, take the integral part.
- subtract the number contained in field `FirstDay`.

For example, if the program was just installed and today is `17 March 2017` the field `FirstDay` will have value `17242` (timestamp 1489788000 divided by 86400). To find out the slot for `17 March 2017` we take the timestamp for any second of this day (for example [1489788000](http://www.unixtimestamp.com/)) and divide it by 86400 and subtract value of `FirstDay` (17242). We get number `0`, indicating slot 0. For `18 March 2017` the formula gives slot number `1` (which BTW doesn’t exist on 17 March yet).

### Monthly and yearly data
You need to calculate the slot numbers for the first and the last days of a certain month, year or any other period and then sum the data from all slots belonging to the period. As an example see the source code of web-interface in file `status.js`.

### Summary
As you see this method returns the raw data from the statistics meter and you have to perform calculations to represent the data in a user friendly format such as: downloaded today, yesterday, this or previous week or month etc.
