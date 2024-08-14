## API-method `config`

### Signature
``` c++
struct[] config();
```

### Description
Returns current configuration loaded into program. Please note that the configuration file on disk may differ from the loaded configuration. This may occur if the configuration file on disk was changed after the program was launched or the program may get some options passed via command line.

### Return value
This method returns array of structures with following fields:

- **Name** `(string)` - Option name.
- **Value** `(string)` - Option value.

### Remarks
- For options with predefined possible values (yes/no, etc.) the values are returned always in lower case.
- If the option has variable references (e. g. `${MainDir}/dst`) the returned value has all variables substituted (e. g. `/home/user/downloads/dst`).
