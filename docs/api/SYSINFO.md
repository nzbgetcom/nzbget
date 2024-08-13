## API-method `sysinfo`

## Since 
`v24.2`

### Signature
``` c++
struct sysinfo();
```

### Description
returns information about the user's environment and hardware.


### Return value
This method returns an array of structures with following fields:

- **OS** `(struct)`
  - **Name** `(string)` - OS name.
  - **Version** `(string)` - OS version.
- **CPU** `(struct)`
  - **Model** `(string)` - CPU model.
  - **Arch** `(string)` - CPU arch.
- **Network** `(struct)`
  - **PublicIP** `(string)` - User's public IP.
  - **PrivateIP** `(string)` - User's private IP.
- **Tools** `(struct[])`
  - **Name** `(string)` - Tool name, e.g. `unrar`.
  - **Version** `(string)` - Tool version, e.g. `7.00`.
  - **Path** `(string)` - Tool path, e.g. `C:\ProgramData\NZBGet\unrar`.
- **Libraries** `(struct[])` - Libraries
  - **Name** `(string)` - Library name, e.g. `OpenSSL`.
  - **Version** `(string)` - Library version, e.g. `3.3.1`.
