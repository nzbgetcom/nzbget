## About ciphers

When an encrypted communication with news server (option **ServerX.Encryption**) is active NZBGet and the news server use TLS/SSL protocol to exchange data in a secure way.

TLS/SSL specification defines many possible ciphers which can be used to encrypt data. The ciphers differ in algorithms and key lengths.

When a TLS/SSL connection is created the so called **TLS Handshake** process is performed. During TLS Handshake the client (NZBGet) and the server (news server) choose the cipher for communication. By default, the most secure cipher is selected. This cipher, however, may not give the best performance on your particular computer.

To maximize your download speed, you can configure NZBGet to use the fastest and most secure encryption cipher available on your system.

A cipher suite is a collection of algorithms that secure a network connection. For performance, we're most interested in the bulk encryption algorithm (Enc), as it handles the continuous encryption of downloaded data.

NZBGet provides an option **ServerX.Cipher** to manually set a cipher.

## Optimizing NZBGet Speed by Choosing the Best Cipher

### Default Behavior (Empty Cipher Option)
If you leave the Cipher option empty, NZBGet will use the default cipher list provided by your system's OpenSSL library. On a modern, up-to-date system, this default list is generally secure and reasonably performant.

However, for maximum performance, it's better to explicitly set the cipher list yourself. This gives you full control and guarantees that you are using the fastest cipher your hardware can support, rather than relying on a generic default.

### Installing the OpenSSL Tools

NZBGet uses the [OpenSSL](https://openssl-library.org/) library for its TLS/SSL connections, which means we can use OpenSSL's built-in tools to test encryption speed.

  - On Linux, you can typically install it via your package manager e.g., for Debian/Ubuntu
  ```text
  apt install openssl
  ```

  - On macOS, the easiest way is with Homebrew:
  ```text
  brew install openssl
  ```

  - On Windows, you may need to download an installer from a trusted source or use a package manager like Chocolatey:
  ```text
  choco install openssl
  ```

### Finding the Fastest Cipher on Your System

In the terminal execute the command

```text
openssl ciphers -v
```

it prints the list of supported ciphers. Example:

```text
TLS_AES_256_GCM_SHA384         TLSv1.3 Kx=any      Au=any   Enc=AESGCM(256)            Mac=AEAD
TLS_CHACHA20_POLY1305_SHA256   TLSv1.3 Kx=any      Au=any   Enc=CHACHA20/POLY1305(256) Mac=AEAD
TLS_AES_128_GCM_SHA256         TLSv1.3 Kx=any      Au=any   Enc=AESGCM(128)            Mac=AEAD
ECDHE-ECDSA-AES256-GCM-SHA384  TLSv1.2 Kx=ECDH     Au=ECDSA Enc=AESGCM(256)            Mac=AEAD
ECDHE-RSA-AES256-GCM-SHA384    TLSv1.2 Kx=ECDH     Au=RSA   Enc=AESGCM(256)            Mac=AEAD
ECDHE-ECDSA-AES256-SHA384      TLSv1.2 Kx=ECDH     Au=ECDSA Enc=AES(256)               Mac=SHA384
ECDHE-RSA-AES256-SHA384        TLSv1.2 Kx=ECDH     Au=RSA   Enc=AES(256)               Mac=SHA384
DHE-RSA-AES256-SHA256          TLSv1.2 Kx=DH       Au=RSA   Enc=AES(256)               Mac=SHA256
ECDHE-ECDSA-AES128-SHA256      TLSv1.2 Kx=ECDH     Au=ECDSA Enc=AES(128)               Mac=SHA256
ECDHE-RSA-AES128-SHA256        TLSv1.2 Kx=ECDH     Au=RSA   Enc=AES(128)               Mac=SHA256
DHE-RSA-AES128-SHA256          TLSv1.2 Kx=DH       Au=RSA   Enc=AES(128)               Mac=SHA256
ECDHE-ECDSA-AES256-SHA         TLSv1   Kx=ECDH     Au=ECDSA Enc=AES(256)               Mac=SHA1
ECDHE-RSA-AES256-SHA           TLSv1   Kx=ECDH     Au=RSA   Enc=AES(256)               Mac=SHA1
DHE-RSA-AES256-SHA             SSLv3   Kx=DH       Au=RSA   Enc=AES(256)               Mac=SHA1
ECDHE-ECDSA-AES128-SHA         TLSv1   Kx=ECDH     Au=ECDSA Enc=AES(128)               Mac=SHA1
ECDHE-RSA-AES128-SHA           TLSv1   Kx=ECDH     Au=RSA   Enc=AES(128)               Mac=SHA1
DHE-RSA-AES128-SHA             SSLv3   Kx=DH       Au=RSA   Enc=AES(128)               Mac=SHA1
RSA-PSK-AES256-GCM-SHA384      TLSv1.2 Kx=RSAPSK   Au=RSA   Enc=AESGCM(256)            Mac=AEAD
DHE-PSK-AES256-GCM-SHA384      TLSv1.2 Kx=DHEPSK   Au=PSK   Enc=AESGCM(256)            Mac=AEAD
RSA-PSK-CHACHA20-POLY1305      TLSv1.2 Kx=RSAPSK   Au=RSA   Enc=CHACHA20/POLY1305(256) Mac=AEAD
DHE-PSK-CHACHA20-POLY1305      TLSv1.2 Kx=DHEPSK   Au=PSK   Enc=CHACHA20/POLY1305(256) Mac=AEAD
ECDHE-PSK-CHACHA20-POLY1305    TLSv1.2 Kx=ECDHEPSK Au=PSK   Enc=CHACHA20/POLY1305(256) Mac=AEAD
AES256-GCM-SHA384              TLSv1.2 Kx=RSA      Au=RSA   Enc=AESGCM(256)            Mac=AEAD
PSK-AES256-GCM-SHA384          TLSv1.2 Kx=PSK      Au=PSK   Enc=AESGCM(256)            Mac=AEAD
PSK-CHACHA20-POLY1305          TLSv1.2 Kx=PSK      Au=PSK   Enc=CHACHA20/POLY1305(256) Mac=AEAD
AES256-SHA256                  TLSv1.2 Kx=RSA      Au=RSA   Enc=AES(256)               Mac=SHA256
AES128-SHA256                  TLSv1.2 Kx=RSA      Au=RSA   Enc=AES(128)               Mac=SHA256
ECDHE-PSK-AES256-CBC-SHA384    TLSv1   Kx=ECDHEPSK Au=PSK   Enc=AES(256)               Mac=SHA384
ECDHE-PSK-AES256-CBC-SHA       TLSv1   Kx=ECDHEPSK Au=PSK   Enc=AES(256)               Mac=SHA1
SRP-RSA-AES-256-CBC-SHA        SSLv3   Kx=SRP      Au=RSA   Enc=AES(256)               Mac=SHA1
SRP-AES-256-CBC-SHA            SSLv3   Kx=SRP      Au=SRP   Enc=AES(256)               Mac=SHA1
```

First column is the cipher suite name. This name must be used in NZBGet in option **ServerX.Cipher**.

The second column indicates the SSL/TLS version where the cipher suite can be used. 

Column **Kx** is the algorithm used for key exchange.

Column **Au** is the algorithm for authentication.

Column **Enc** is the bulk encryption algorithm used to encrypt the message stream.

Column **Mac** is the message authentication code (MAC) algorithm used to create the message digest, a cryptographic hash of each block of the message stream.

**Kx** (key exchange) and **Au** (authentication) are performed only during the establishing connection and are therefore not relevant for performance.

**Enc** (encryption algorithm) and **Mac** (message authentication code algorithm) are used to encrypt/decrypt the data stream and have direct impact on performance.

To achieve the best performance, we must choose a cipher suite with the fastest algorithm. In modern TLS 1.3, the encryption (Enc) and message authentication (Mac) are combined into a single, efficient algorithm known as an AEAD (Authenticated Encryption with Associated Data).

**While some cipher suites might have historically supported SSL versions, using TLS is now the preferred and recommended practice.** 

**Why TLS 1.3 is Best**

They are inherently more secure and efficient than their TLS 1.2 predecessors because they:

  - **Remove outdated algorithms**: Weak and complex options from TLS 1.2 are eliminated.

  - **Use AEAD ciphers only**: Authenticated Encryption with Associated Data (AEAD) ciphers like AES-GCM and CHACHA20-POLY1305 combine encryption and message authentication. This is more secure and efficient than the older approach of using separate encryption and MAC (Message Authentication Code) algorithms (e.g., CBC mode ciphers).

  - **Simplify the handshake**: The TLS 1.3 handshake is faster, requiring fewer round-trips between the client and server.

The primary **AEAD** ciphers available for TLS 1.3 are:

  - **Enc**: AES-GCM (256-bit), ChaCha20-Poly1305 (256-bit), and AES-GCM (128-bit). 
It's important to note that key lengths below 128 bits are not secure, which is why modern protocols like TLS 1.3 have made them mandatory.

  - **Mac**: Handled by the integrated AEAD mechanism.

## Testing Cipher Speed
Cipher speed is highly dependent on your CPU. The best way to find the fastest one is to benchmark it directly on your machine.

Run these commands in your terminal to test the relevant algorithms. For accurate results, ensure your system is idle during the test.

```text
openssl speed -evp AES-256-GCM
openssl speed -evp ChaCha20-Poly1305
openssl speed -evp AES-128-GCM
```

The **openssl speed** command is a built-in benchmarking tool that measures how fast your CPU can perform cryptographic operations. Following the **-evp** flag, we list the specific algorithms to be tested, allowing for a direct comparison of their performance on your system.

The commands test each cipher and print a summary table. Below is a sample result:

```text
<STRIPPED INDIVIDUAL TEST RESULTS, SHOWING ONLY THE SUMMARY>
The 'numbers' are in 1000s of bytes per second processed.
type               16 bytes       64 bytes     256 bytes    1024 bytes    8192 bytes  16384 bytes
AES-256-GCM       57701.90k      222771.20k   712733.82k   1770800.37k   3399842.19k  3581903.40k
ChaCha20-Poly1305 270367.98k     528003.11k   1088896.68k  2009559.59k   2135840.09k  2166984.01k
AES-128-GCM       63705.82k      240874.77k   825188.52k   2158119.38k   4476657.66k  4844403.59k
```

### Analyzing the Results
  - **Focus on the Largest Block Size**: Usenet downloads consist of large, continuous data streams. TLS encrypts this data in records up to 16 KB (16384 bytes). Therefore, the performance number in the 16384 bytes column is the most important metric for download speed.

  - **Hardware Matters (AES-NI)**: On most modern desktop CPUs (Intel/AMD), **AES-128-GCM** will be the fastest. This is due to AES-NI, a set of hardware instructions that dramatically accelerates AES encryption. On other devices, like a Raspberry Pi, **ChaCha20-Poly1305** might be faster as it doesn't rely on this specialized hardware.

Based on the test results, we can determine the best choice. On most modern desktop hardware, **AES-128-GCM** will be the winner. 
Therefore, choosing either **TLS_AES_128_GCM_SHA256** or **TLS_AES_256_GCM_SHA384** will likely give you the best performance.

### Recommended Configuration
Based on your benchmark results, create a colon-separated list starting with your fastest cipher. For most users, **AES-128-GCM** will be the winner.

The following string is recommended for the vast majority of systems. 
It lists the fastest ciphers first, but the server makes the final choice based on what it supports.

```text
ServerX.Cipher=TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256
```

### A Note on Cipher Priority

If you provide a list that mixes **TLS 1.3** and older **TLS 1.2** ciphers, **OpenSSL** will always prefer **TLS 1.3** during the handshake. 
The server will choose a **TLS 1.3** cipher if possible, regardless of its order in your list.
