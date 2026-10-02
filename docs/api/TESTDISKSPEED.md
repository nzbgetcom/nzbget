## API-method `testdiskspeed`

## Since 
`v24.3`

### Signature
``` c++
struct testdiskspeed(
    string dirPath, 
    int writeBufferSize, 
    int maxFileSize,
    int timeout,
);
```

### Description
The function runs a sequential write test followed by a sequential read test on a temporary file created in the given directory. The test uses regular buffered file I/O, so the result reflects the throughput the program itself gets from the file system, including the OS file cache. Each phase ends when the maximum file size is reached or the timeout period expires. The test file is always removed afterwards.

Downloads are paused while the test is running and are resumed afterwards (unless they were already paused before the test).

The test file size is limited so that at least 100 MiB of free space always remains on the disk; if there is not enough free space the method fails.

### Arguments
- **dirPath** `(string)` - The path to the directory where the test file will be created (UTF-8).
- **writeBuffer** `(int)` - The size of the block used for writing and reading (in KiB). `0` selects the default of 1 MiB.
- **maxFileSize** `(int)` - The maximum size of the file to be created (in GiB).
- **timeout** `(int)` - Timeout of each test phase, write and read (in seconds).

### Return value
- **SizeMB** `(int)` - Written data size (in MiB).
- **DurationMS** `(int)` - Write phase duration (in milliseconds).
- **ReadSizeMB** `(int)` - Read data size (in MiB). Since `v27.0`.
- **ReadDurationMS** `(int)` - Read phase duration (in milliseconds). Since `v27.0`.
