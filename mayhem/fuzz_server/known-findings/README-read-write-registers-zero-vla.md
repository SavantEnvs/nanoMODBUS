# Zero-length variable-length array in `handle_read_write_registers` (nanomodbus.c)

**Where:** `handle_read_write_registers()` (nanomodbus.c, around line 1565) declares
`uint16_t registers[byte_count_write / 2]` right after reading the request's write byte count, before the
read/write quantities are validated.

**Reproducer:** `read-write-registers-zero-vla.bin` fed to `fuzz_server` (TCP, FC 23, write byte count 0): UBSan
`variable length array bound evaluates to non-positive value 0` at `nanomodbus.c:1565` (Mayhem defect #3625239).

**Impact:** undefined behaviour reachable from any client request with a Read/Write Multiple Registers byte count of 0
or 1 (the validation that would return an exception runs after the declaration). Same class as
`README-read-file-record-zero-vla.md`.

**Fix:** declare the array with its fixed maximum (0x7B entries), or validate `byte_count_write` before declaring it.
