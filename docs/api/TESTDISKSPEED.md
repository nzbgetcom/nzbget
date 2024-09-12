## API-method `testdiskspeed`

## Since 
`v24.3`

### Signature
``` c++
bool testdiskspeed(
    string dirPath, 
    int writeBufferSize, 
    int maxFileSize,
    int timeout,
);
```

### Description
The function writes data to a file until either the maximum file size is reached or the timeout period expires.

### Arguments
- **dirPath** `(string)` - The path to the directory where the test file will be created.
- **writeBuffer** `(int)` - The size of the buffer used for writing data to the file (in KiB).
- **maxFileSize** `(int)` - The maximum size of the file to be created (in GiB).
- **timeout** `(int)` - Test timeout (in seconds).

### Return value
- **SizeMB** `(int)` - Written data size (in MiB).
- **DurationMS** `(int)` - Test duration (in milliseconds).
