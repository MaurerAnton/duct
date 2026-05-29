# duct

**The shortest path between two files is a duct.**

`duct` moves raw bytes from one machine to another over TCP. No protocol, no handshake, no metadata — just bytes in, bytes out. Two modes, one binary, zero bloat.

```
# on the receiver: listen on port 9999, write to file
duct 9999 > backup.tar.gz

# on the sender: connect and push data
duct 10.0.0.5 9999 < backup.tar.gz
# or give the file path directly — same result
duct 10.0.0.5 9999 backup.tar.gz
```

---

## Why duct exists

Every file transfer tool makes you choose between power and simplicity. `duct` rejects the tradeoff. Here is how it compares to every known alternative:

### vs netcat (`nc`)

`nc` is the Swiss Army knife — 1500+ lines, dozens of flags (`-k -l -p -u -w -q -n -v -z -X -x -s -P ...`), GNU vs BSD vs Nmap variants with mutually incompatible option syntax. You spend 30 seconds recalling whether it's `nc -l -p 9999` or `nc -l 9999`. And after that, you're carrying a full protocol toolkit for a one-byte job.

`duct` has no flags. `duct 9999 > file` or `duct host 9999 < file`. You'll never open a man page again.

### vs sph-mn/tcpcat

The only other C program to use the stdout-pipe model. But it allocates 10 megabytes on the heap (`malloc`), requires `isatty()` for mode detection (breakable with `</dev/null` edge cases), and bundles IPv4, IPv6, and Unix sockets into one binary for all users regardless of need.

`duct` uses a stack buffer. No `malloc`. No heap at all. Works in recovery environments where `libc` malloc is unavailable, corrupted, or stripped. Mode detection is deterministic: one positional argument means listen, two means connect — no guessing, no `isatty` magic.

### vs nikhilroxtomar/File-Transfer-using-TCP-Socket-in-C (41 ★)

Hardcoded filenames (`send.txt`, `recv.txt`), hardcoded IP (`127.0.0.1`), hardcoded port (`8080`). Uses `fgets`/`fprintf` — text mode only, corrupts binary files on the first null byte or newline inside data. Works in one scenario: two terminals on localhost with a text file named `send.txt`. Change anything — recompile.

`duct` takes host, port, and file from command-line arguments. Uses `read()`/`write()` on raw file descriptors — handles any binary payload without interpretation. No recompilation for different targets.

### vs scalpelx/Transfer_File (15 ★) and Ikaros-521 (16 ★)

Both send a filename as a separate protocol header before the data. The receiver opens a file by that name and writes to disk. You cannot pipe the output — the protocol has already consumed the filename bytes and the receiver insists on owning the destination file.

`duct` has no protocol. The receiver writes to `stdout`. You decide where the bytes go — a file, a pipe, a block device, a decompressor, a media player. The sender has no opinion about the receiver's storage.

### vs zerotier/toss (407 ★)

A well-engineered tool — but ~2000 lines of C with cryptographic hashing (Speck), IP scope detection, multi-file support. Great for ZeroTier networks, overkill for 99% of LAN file transfers.

`duct` is ~40 lines. You can read the entire source in 30 seconds and know exactly what it does.

### vs `python3 -m http.server`

Requires Python. Sends HTTP headers (~300 bytes of overhead per request). Multiplexes unrelated HTTP concerns (MIME types, status codes, directory listing) onto a file transfer. Browser-centric — command-line use requires `wget` with flags to strip `index.html`.

`duct` has zero protocol overhead. The first byte on the socket is the first byte of your file. No HTTP, no Python, no runtime.

### vs `rsync` / `scp`

Both require SSH on both ends. Both authenticate. Both negotiate. Perfect for incremental sync and secure remote access. Wrong tool for: "I just want to beam this tarball to the Raspberry Pi next to me on the LAN."

`duct` has no authentication, no encryption, no negotiation. It is the wrong tool for WAN. It is exactly the right tool for trusted LANs at maximum speed with minimum ceremony.

### vs curl

`curl file://` is local-only. `curl -T` uploads via FTP/HTTP and requires a matching server on the other end. `curl` is ~300 KB binary and links against 4+ shared libraries.

`duct` is one statically-linked binary under 16 KB. No shared libraries beyond `libc`.

---

## Design invariants

These will never change. They are the contract.

1. **No protocol.** Bytes sent are bytes received. The wire is transparent.
2. **stdout on receive.** The receiver does not write files. It writes to stdout. Pipe the result wherever you want.
3. **Stack-only.** No `malloc`, no heap allocation. Works in environments with a broken or absent allocator.
4. **No flags.** Mode is determined by argument count — no `-l`, no `-p`, no option parsing.
5. **One file per source.** The sender reads from a single file descriptor (`stdin` or a named file). No directory traversal, no globbing, no multi-file manifests.
6. **TCP only.** No UDP, no Unix sockets, no IPv6. One transport, done well.

---

## Why the name

A duct is the simplest possible conduit — a hollow tube that moves something from one end to the other without changing it. No valves, no filters, no sensors. Just a path.

---

## Build

```sh
cc -O2 -s -o duct duct.c
```

That's it. No `./configure`, no `cmake`, no `Makefile`. GCC, Clang, tcc, cproc, cosmocc — any C99 compiler works.

Statically linked with musl for a truly portable binary:

```sh
musl-gcc -O2 -static -s -o duct duct.c
```

Result: ~12 KB, runs on any Linux kernel 2.6+.

The 2.6 floor is set by musl libc, not by duct itself. duct uses only
seven POSIX syscalls — `socket`, `bind`, `listen`, `accept`, `connect`,
`read`, `write` — all of which exist since Linux 1.0. If you link against
an older libc (glibc, dietlibc, klibc), duct runs on kernels as far back
as Linux 1.0. The 2.6 constraint is purely about which static libc you
choose at compile time.

---

## Integration examples

```sh
# Stream a disk image directly to a block device, no temp file
duct 9999 > /dev/sdb1             # receiver
duct 10.0.0.5 9999 < disk.img    # sender

# Transfer, decompress, and verify in one pipeline
duct 9999 | zstd -d | tar xf -    # receiver
tar cf - ./dir | zstd -c | duct 10.0.0.5 9999   # sender

# Tunnel through SSH for encryption when crossing untrusted networks
ssh user@host duct 9999 > local-file
duct host 9999 < remote-file & ssh -R 9999:localhost:9999 bridge

# Stream video without buffering to disk
duct 9999 | mpv -                  # receiver
duct 10.0.0.5 9999 < video.mkv   # sender
```

---

## Non-goals

- **Encryption.** Use `ssh -L` or `wireguard` to tunnel `duct`. Encryption belongs at the network layer, not inside a 40-line file transfer tool.
- **Resume / delta transfer.** Use `rsync`. `duct` is for whole files on reliable links.
- **Directory sync.** Tar the directory and pipe it. `duct` moves one byte stream.
- **IPv6, UDP, Unix sockets.** 99% of LAN transfers are TCP/IPv4. The other transports add lines of code for features you'll never use.
- **Progress bars.** Pipe through `pv`: `duct 10.0.0.5 9999 < file | pv -s $(stat -c%s file) > /dev/null` (receiver). `pv file | duct 10.0.0.5 9999` (sender).
- **Multiple connections.** One file, one connection, one shot. Need to serve multiple receivers? Loop it in a shell script.

---

## Competitor summary

| Tool | Lines | Binary size | Protocol overhead | Binary output | Pipe to stdout | no malloc | no flags | IP/port configurable |
|---|---|---|---|---|---|---|---|---|
| **duct** | ~40 | ~12 KB | 0 bytes | ✓ | ✓ | ✓ | ✓ | runtime args |
| nc (netcat) | ~1500 | ~40 KB | 0 bytes | ✓ | ✓ | ✗ | ✗ | runtime args |
| tcpcat | ~60 | ~16 KB | 0 bytes | ✓ | ✓ | ✗ | ✓ | runtime args |
| nikhilroxtomar | ~80 | ~16 KB | 0 bytes | ✗ text-only | ✗ | ✓ | ✗ | hardcoded |
| scalpelx | ~100 | ~16 KB | filename header | ✓ | ✗ | ✓ | ✗ | hardcoded port |
| zerotier/toss | ~2000 | ~80 KB | crypto header | ✓ | ✗ | ✗ | ✗ | runtime args |
| python http.server | N/A | N/A | HTTP headers | ✓ | ✗ | N/A | ✗ | runtime args |
| rsync | ~50000 | ~500 KB | delta protocol | ✓ | ✗ | ✗ | ✗ | runtime args |
| scp | ~15000 | ~200 KB | SSH protocol | ✓ | ✗ | ✗ | ✗ | runtime args |

---

## License

MIT. One file. Take it anywhere.
