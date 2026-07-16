dmc_unrar README
================

dmc_unrar is a dependency-free, single-file FLOSS library for unpacking
and decompressing [RAR](https://en.wikipedia.org/wiki/RAR_(file_format))
archives. dmc_unrar is licensed under the terms of the [GNU General
Public License version 2](https://www.gnu.org/licenses/gpl-2.0.html) (or
later).

Please see `dmc_unrar.c` for details.


Configuration defines
---------------------

All compile-time configuration goes through `DMC_UNRAR_*` defines.
Set them with `-D` flags or by `#define`-ing them before including
`dmc_unrar.c`. Every define has a sensible default — you only need to
touch them to opt out of a default capability, opt in to a
defense-in-depth check, or tune a cap.

### Build and integration

| Define | Default | Effect |
|---|---|---|
| `DMC_UNRAR_HEADER_FILE_ONLY` | unset | When defined, only the header portion of `dmc_unrar.c` compiles. Useful for splitting the single file across a `.h` / `.c` boundary. |
| `DMC_UNRAR_32BIT` / `DMC_UNRAR_64BIT` | autodetect | Force the CPU-width assumption if the autodetector misfires on an exotic target. |
| `DMC_UNRAR_SIZE_T` | `uint64_t` | Internal size type. Override on targets where a smaller type is required. |
| `DMC_UNRAR_OFFSET_T` | `int64_t` | Internal signed offset type. Override alongside `DMC_UNRAR_SIZE_T`. |
| `DMC_HAS_LARGE_FILE_SUPPORT` | autodetect | Force on/off support for files and archives ≥ 2 GiB. Without it, the library returns `DMC_UNRAR_FILE_UNSUPPORTED_LARGE` for large entries. Note: missing the `UNRAR` infix for historical reasons. |
| `DMC_UNRAR_USE_FSEEKO_FTELLO` | autodetect | Force `fseeko`/`ftello` instead of `fseek`/`ftell` for file IO. Autodetected on 32-bit macOS and glibc. |

### Feature toggles (opt out of default-on capabilities)

| Define | Default | Effect |
|---|---|---|
| `DMC_UNRAR_DISABLE_STDIO` | `0` | Disable all `FILE*` / path-based APIs. Only memory-based APIs remain. |
| `DMC_UNRAR_DISABLE_WIN32` | autodetect | Don't use the Win32 UTF-16 IO path. On Windows this drops path support to plain ASCII; elsewhere the flag has no effect. |
| `DMC_UNRAR_DISABLE_MALLOC` | `0` | Remove all internal `malloc` / `realloc` / `free` calls. Callers must then supply allocator callbacks or use only static-buffer paths. |
| `DMC_UNRAR_DISABLE_BE32TOH_BE64TOH` | `0` | Don't include `<endian.h>`. Needed on targets that lack it (e.g. strict C89, some non-glibc libcs). |
| `DMC_UNRAR_DISABLE_PPMD` | `0` | Drop the PPMd decoder. Entries compressed with PPMd return `DMC_UNRAR_30_DISABLED_FEATURE_PPMD`. |
| `DMC_UNRAR_DISABLE_FILTERS` | `0` | Drop RAR filter support. Filtered entries return `DMC_UNRAR_30_DISABLED_FEATURE_FILTERS` / `DMC_UNRAR_50_DISABLED_FEATURE_FILTERS`. |

### Safety and hardening

These control checks that protect apps extracting untrusted archives.
`REJECT_*` defines are opt-in; `DISABLE_*` defines are opt-outs of a
default-on check.

| Define | Default | Effect |
|---|---|---|
| `DMC_UNRAR_DISABLE_HEADER_CRC_CHECK` | `0` (i.e. enforcement on) | When set, accept RAR4 block headers whose stored CRC doesn't match. Otherwise a bad CRC fails archive open with `DMC_UNRAR_INVALID_DATA`. |
| `DMC_UNRAR_REJECT_NON_UTF8_PATHS` | `0` | Make `dmc_unrar_extract_file_to_path()` reject archive names that aren't valid UTF-8. Returns `DMC_UNRAR_FILE_UNSAFE_PATH`. The `_unsafe` variant skips this check. |
| `DMC_UNRAR_REJECT_WINDOWS_RESERVED_NAMES` | on under `_WIN32`, off elsewhere (overridable) | Reject archive names whose components are unsafe on Windows: reserved device names (`CON`, `PRN`, `AUX`, `NUL`, `COM1-9`, `LPT1-9`), illegal characters (`< > : " \| ? *` and ASCII controls), or trailing spaces/dots. Device-name checks are case-insensitive; `CON.txt` is rejected alongside bare `CON`. Returns `DMC_UNRAR_FILE_UNSAFE_PATH`. The `_unsafe` variant skips this check. |
| `DMC_UNRAR_REJECT_OVERWRITE` | `0` | Make `dmc_unrar_extract_file_to_path()` refuse to write when the target path already exists. Returns `DMC_UNRAR_FILE_EXISTS` before decompressing if the target exists up front, or during final no-replace publish if another writer creates it mid-extraction. Applies to both the safe and `_unsafe` variants because it concerns the caller-provided target path, not the archive name. |

#### Per-call opt-out: `dmc_unrar_extract_file_to_path_unsafe()`

The safe default `dmc_unrar_extract_file_to_path()` runs a path-safety
check on every extraction and honors the opt-in `REJECT_*` defines
above. For the rare case where the caller has already validated the
archive filename externally, or is routing output to a target path that
is computed independently of archive contents (e.g. a caller-chosen
name with the archive entry looked up by index), there's a sibling:

```c
dmc_unrar_return dmc_unrar_extract_file_to_path_unsafe(
    dmc_unrar_archive *archive, dmc_unrar_size_t index,
    const char *path, dmc_unrar_size_t *uncompressed_size,
    bool validate_crc);
```

What it bypasses:

- The built-in path-safety check (traversal `..`, absolute paths, drive
  prefixes like `C:`, UNC prefixes like `//server/share`, surviving
  backslashes).
- `DMC_UNRAR_REJECT_NON_UTF8_PATHS` — an archive-name check.
- `DMC_UNRAR_REJECT_WINDOWS_RESERVED_NAMES` — an archive-name check.

What it still enforces:

- `DMC_UNRAR_REJECT_OVERWRITE` — this concerns the caller-provided
  target path, not the archive name, so both variants honor it.
- All atomic-extraction guarantees (exclusive temp-file + publish;
  nothing written at `path` on failure).
- All resource caps and the cancellation callback.

Rule of thumb: if you are feeding `dmc_unrar_get_filename()` straight
into the target path, use the safe default. Use `_unsafe` only when
you've taken responsibility for filename validation yourself.

### Resource caps (archive-bomb mitigation)

All caps fire deterministically at archive open or extract time with
`DMC_UNRAR_INVALID_DATA` when exceeded. Defaults are generous; tune
lower if your workload has a narrower plausible envelope.

| Define | Default | What it limits |
|---|---|---|
| `DMC_UNRAR_MAX_FILE_COUNT` | 1,000,000 | Declared entries per archive. |
| `DMC_UNRAR_MAX_FILE_SIZE` | 16 GiB | Declared uncompressed size for any single entry. |
| `DMC_UNRAR_MAX_TOTAL_SIZE` | 256 GiB | Cumulative declared uncompressed size across all entries. |
| `DMC_UNRAR_MAX_COMPRESSION_RATIO` | 1000:1 | Per-entry uncompressed/compressed ratio. Stored entries (1:1) always pass. |
| `DMC_UNRAR_MAX_DICT_SIZE` | 256 MiB | LZSS sliding-window size the decompressor will allocate per extraction. |
| `DMC_UNRAR_MAX_PPMD_SIZE_MB` | 32 | Archive-declared PPMd heap size (in MiB). Only enforced when the archive explicitly declares a heap size. |

### Tuning knobs

These trade memory for speed. Defaults are a reasonable compromise.

| Define | Default | Effect |
|---|---|---|
| `DMC_UNRAR_ARRAY_INITIAL_CAPACITY` | `8` | Initial capacity of the internal file-entry array. Higher = fewer reallocations during open; wasted memory on small archives. |
| `DMC_UNRAR_BS_BUFFER_SIZE` | `4096` | Bitstream buffer size in bytes. Must be a multiple of 8. Higher = faster, more memory. |
| `DMC_UNRAR_HUFF_MAX_TABLE_DEPTH` | `10` | Huffman direct-lookup table depth. Memory scales as `2^depth * 4` bytes; longer codes fall through to a binary tree. |

### Cooperative cancellation

Not a `#define` but worth mentioning alongside the caps: set
`dmc_unrar_cancel` on the archive struct to a callback that the library
polls at block-collect, extract, and decompressor loop points. Returning
`false` unwinds with `DMC_UNRAR_USER_CANCEL`. See the archive struct
definition in `dmc_unrar.c` for details.
