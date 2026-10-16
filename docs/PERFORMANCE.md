# NZBGet performance

This document describes the options that matter most for download, repair and unpack throughput, and how to find the bottleneck.

## 1. Find the bottleneck first

NZBGet is rarely the limiting factor itself. Look for the component that is saturated:

| Symptom | Likely bottleneck |
|---|---|
| Speed does not grow with more connections, CPU well below 100% | News provider or the network path |
| Speed collapses after some time and does not recover | `InterDir` disk (write cache exhausted) |
| Speed drops while repairing or unpacking | CPU, or the disk holding `DestDir` |
| Stable speed, but below the link rate | Connection count, cipher, or the provider |

Measure before changing anything:

- **CPU** — `top`/`htop` on the machine running NZBGet. Add up all NZBGet threads.
- **Network** — the WebUI shows the current download speed; compare it with the link speed of the network interface.
- **Disks** — use the built-in disk speed test (section 2).

## 2. Disk I/O

- Put `InterDir` on the fastest disk available (NVMe/SSD). During download NZBGet writes only to `InterDir`; the files are moved to `DestDir` when the download completes.
- Do not use a network filesystem (NFS, SMB) for `InterDir`, and avoid filesystems with compression or deduplication enabled.
- Using separate physical drives for `InterDir` and `DestDir` improves unpack performance.
- When `DirectRename` is enabled, `InterDir` and `DestDir` must be on the same file system.
- Do not judge an SSD by its interface: once the write cache (SLC) of the drive is exhausted, sustained write speed can fall to a fraction of the peak, and a partially filled drive has a smaller cache and fewer free blocks.

### Testing disk speed

The **Status** page has a speed test button for `InterDir` and for `DestDir`. The test writes and reads a temporary file in the selected directory and reports an average speed.

Note the limits of that dialog: at most 16 GiB and 5 seconds, and the result is a single averaged value, so a drop in speed after the first seconds stays invisible. For a longer test, call the RPC method directly with larger values, for example 32 GiB and 60 seconds:

```
testdiskspeed(dirPath, writeBufferSize, maxFileSize, timeout)
```

See [testdiskspeed](api/TESTDISKSPEED.md) for the parameters. `fio` or `dd` with 30–50 GB written to the same directories work as well.

## 3. Memory

| Option | Value in `nzbget.conf` | Notes |
|---|---|---|
| `ArticleCache` | 0 | Cache for downloaded articles (MB). With `DirectWrite` enabled, 50 MB is enough to reduce disk fragmentation. Without `DirectWrite` the cache should hold a complete file (typically up to 200 MB, sometimes 500 MB), otherwise articles go to the temporary directory and performance drops. |
| `DirectWrite` | yes | Writes articles directly into the destination file using sparse files, instead of writing them to a temporary file and copying them later. Recommended. |
| `WriteBuffer` | 0 | Write buffer **per connection** (KB). 1024 is recommended; 0 uses the operating system default, which is usually too small (1–4 KB). |
| `ParBuffer` | 16 | RAM the par-checker may use during repair (MB). The closer the buffer is to the total size of all damaged blocks, the faster the repair; with plenty of RAM a few hundred MB are worth it. |
| `ParThreads` | 0 | Threads used during par-repair (0–99). On multi-core CPUs use the number of logical cores, and one or two more to compensate for synchronisation waits. |

`WriteBuffer` is set per connection, so the total memory for write buffers is `WriteBuffer` × the sum of all `ServerX.Connections` values. For example, 1024 KB with 100 connections is about 100 MB, while 38400 KB with 100 connections is about 3.8 GB.

## 4. News servers and connections

- `ServerX.Connections` is the maximum number of simultaneous connections to that server (0–999). The total is the sum over all active servers.
- Each connection costs one thread and its own write buffer (section 3).
- More connections reduce the effect of the round-trip time to the provider. Once the line is saturated, additional connections no longer add speed and only cost CPU and memory.
- To check whether the connection count is limiting, change it and measure again.
- The TLS cipher affects the CPU cost of every connection; see [Optimizing NZBGet speed by choosing the best TLS cipher](FINDING_BEST_TLS_CIPHER.md).
- `DownloadRate` (KB/s, 0 = unlimited) caps the download speed by design — make sure it is 0 unless a limit is wanted.

## 5. Post-processing

- `ParCheck` / `ParRepair` / `ParQuick` control verification and repair; `ParBuffer` and `ParThreads` control their speed (section 3).
- `DirectUnpack` unpacks files while they are still being downloaded. RAR only (7z and other formats are not supported); requires `Unpack` to be enabled, and works best together with `DirectRename` and `ReorderFiles`.
- `UnpackPauseQueue`, `ParPauseQueue` and `ScriptPauseQueue` pause the download queue to give the CPU to unpacking, repairing and extension scripts. Useful on slow CPUs.
- `UnpackCleanupDisk` deletes the archive files after a successful unpack; disabling it keeps them and uses more disk space.
- For unpack-speed tuning see [Optimizing the unpacking speed](UNPACKING.md).

## 6. Check the configuration

- NZBGet checks the most important options on startup and reports suboptimal values in the WebUI (option `SystemHealthCheck`, RPC method `systemhealth`, see [systemhealth](api/SYSTEMHEALTH.md)).
- `nzbget -p` prints the configuration that the daemon will use, including all defaults and values inherited from other options.
