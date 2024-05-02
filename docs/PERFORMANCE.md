# NZBGet performance tips (WIP)

## Unrar

Unrar from version 7 provides improved performance when extracting encrypted archives, especially when built with optimized march.

Several tests (`time unrar -p<password> x <archive>`, archive size ~1.6Gb):

- arm64 (Mac mini M2)
  - unrar 6.24
    ```
    real    0m 27.71s
    user    0m 25.00s
    sys     0m 1.41s
    ```
  - unrar 7.0 march=armv8-a
    ```
    real    0m 11.04s
    user    0m 9.28s
    sys     0m 1.19s
    ```
  - unrar 7.0 march=armv8-a+crypto+crc
    ```
    real    0m 6.04s
    user    0m 3.59s
    sys     0m 1.48s
    ```

- x86_64 (Intel Xeon E3-1265L V2)
  - unrar 6.24
    ```
    real    0m 13.51s
    user    0m 12.08s
    sys     0m 1.27s
    ```
  - unrar7 (march=x86-64-v2)
    ```
    real    0m 6.13s
    user    0m 5.34s
    sys     0m 1.90s
    ```

Extraction speed is increased by ~2x and ~4.5x when using the optimized binary.
The NZBGet docker container includes unrar6 (used by default, for most compatibility), and an optimized unrar7 (can be enabled in the settings). More information can be found in [Docker readme](../docker/README.md#unrar-7-support)
