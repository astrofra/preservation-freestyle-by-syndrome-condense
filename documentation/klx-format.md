# FreeStyle KLX / FXLK Format

## Header

Unsigned 32-bit little-endian integers; no additional alignment padding.

| Offset | Size | Field | Value in FreeStyle |
|---:|---:|---|---:|
| `0x00` | 4 | ASCII signature `FXLK` (`0x4b4c5846`) | `FXLK` |
| `0x04` | 4 | Uncompressed index size | 6,103 |
| `0x08` | 4 | Stored index size | 2,324 |
| `0x0c` | 2,324 | LZARI index | |
| `0x920` | variable | File payloads, in index order | |

The archive-opening routine always passes the index to the LZARI decoder.
It contains neither an entry count nor a sentinel: its decoded size marks
the end of the table.

Each entry in the decoded index contains:

1. A null-terminated Windows-1252 path.
2. The uncompressed size, little-endian `uint32`.
3. The stored size, little-endian `uint32`.

A file's offset is `12 + stored_index_size + sum_of_previous_stored_sizes`.
The last file ends exactly at byte 981,601, the archive's size.
There is no original checksum or timestamp metadata.

## Payloads

If the stored and uncompressed sizes are equal, each byte is decoded using
`byte ^ 0x9a`. This applies to two files: `chromball.jpg` and `PseudoNULL.lwo`.

Otherwise, the payload is an LZARI stream without its own size header.
The other 159 files and the index use this decoder:

- A 4,096-byte LZ window, with matches from 3 to 60 bytes long.
- The first 4,036 bytes of the window are initialized to `0x20`, the last 60
  to zero; the initial cursor position is 4,036.
- An alphabet of 314 symbols: 0–255 are literals; 256–313 are matches whose
  length is `symbol - 253`.
- Adaptive arithmetic coding, with initial bounds `[0, 0x20000)`, an initial
  17-bit code, and bits read most significant first.
- Character frequencies initialized to 1; reordering by frequency,
  with frequencies halved when their sum reaches `0x7fff`.
- A fixed position model: `cum[4096] = 0`, then
  `cum[i-1] = cum[i] + 10000 / (i + 200)` for `i` from 4096 down to 1, using integer division.
- The decoded distance ranges from 1 to 4,096. Overlapping copies reuse
  bytes written into the window.
- Decoding stops at the declared uncompressed size, with no end-of-file marker.

The compressor terminates its arithmetic coding, then writes seven zero bits.
The decoder's lookahead may extend beyond the last stored byte. The C
implementation allows at most two virtual zero bytes for this termination
and rejects reads beyond that limit. The entire archive was checked against
the original routine with this handling in place.

## Reconstruction Method

`freestyle.exe` is packed with an old version of UPX. UPX 5.1.1 and 3.09
refused to unpack it, reporting an obsolete version. The stub was therefore
emulated in x86 with Unicorn, from `0x004a9d00` to `0x004a9e0a`, stopping after
the code and relative calls had been restored, but before Windows imports
were resolved. The graphical application was not launched.

Useful virtual addresses in the image based at `0x00400000`:

| Address | Observed role |
|---|---|
| `0x00401290` | Opening and reading the archive |
| `0x00401319` | Comparing the signature against `0x4b4c5846` |
| `0x00401338` | Reading the sizes and compressed index |
| `0x004014c0` | Looking up a file and reading its payload |
| `0x00401687` | XOR `0x9a` loop |
| `0x00401c80` | Initializing the arithmetic models |
| `0x00401d10` | Updating frequencies and reordering |
| `0x00402230` | Decoding a character / length symbol |
| `0x004023c0` | Decoding a position |
| `0x00402550` | Original compressor, used for test fixtures |
| `0x00402930` | Complete LZARI decompressor |
| `0x00402ad0` | Configuring the input buffer and output size |
| `0x00402b00` | Initializing the codec object |

To obtain a reference independent of the C port, the original LZARI and XOR
routines were emulated for each payload. Only `malloc` (`0x00436280`) was
replaced with a working buffer. The index and all 161 payloads match the
Python prototype byte for byte. The C outputs in turn match the 161 SHA-256
hashes from this original decoding, recorded in `freestyle-manifest.json`
along with the hash of the source executable.

The scripts and working dump are kept in `analysis/klx/`
(ignored by Git). The C implementation does not use the `tools/unpack_klx.py`
prototype: `klx_unpack.c` contains everything needed for extraction.

## Validation and Limitations

- 72 JPEG images fully decoded with Pillow during validation.
- 64 `FORM/LWOB` objects: FORM sizes and all chunk boundaries verified.
- 11 scenes with the `LWSC` header, version 1.
- 12 files with the `MOA3` signature, preserved without full interpretation.
- `Mush.mo3` music with the `MO3` signature, extracted without audio validation or conversion.
- Windows x64 Release build with MSVC `/W4 /WX` and `/MT`; only
  `KERNEL32.dll` in the import table.

The extractor requires a new destination, validates the index, and decodes
all payloads before creating any files. It rejects path traversal,
unrecognized absolute path aliases, common Windows reserved names,
case-insensitive Windows-1252 name collisions, and file/directory conflicts.
Archive paths such as `D:\...` become `D/...` under the destination.
UTF-8 paths are converted to UTF-16 for Windows operations.

The tool limits the archive, the uncompressed index, and the total decoded
file size to 256 MiB each, with at most 100,000 entries and 4,096 bytes per name.
`--list` validates the table and payload boundaries without decoding the payloads.
Without an original checksum, some stream corruptions may produce data of
the expected size: the supplied hashes identify the exact files in this
archive. Compatibility with other KLX variants has not been established.
