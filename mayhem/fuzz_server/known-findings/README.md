# Misaligned `uint16_t` access in the file-record paths (nanomodbus.c)

**Where:** `handle_read_file_record` / `handle_write_file_record` (server) and `recv_read_file_record_res` /
`recv_write_file_record_res` (client) cast `get_n(nmbs, ...)`, a pointer into the byte message buffer at an odd
offset (after the 1-byte response length, 1-byte sub-length and 1-byte reference type), to `uint16_t*`, then pass
it to the user callback and to `swap_regs()` / `put_regs()`.

**Reproducer:** `misaligned-read-file-record.bin` (a client `Read File Record` request fed to `fuzz_server`) and
`misaligned-read-file-record-response.bin` (the matching response fed to `fuzz_client`). Built with
alignment checking on (`-fsanitize=alignment`), UBSan reports `store to misaligned address ... for type
'uint16_t'` at `nanomodbus.c:142` (`swap_regs`).

**Impact:** undefined behaviour in C. Harmless on x86/ARMv7-A, but nanoMODBUS targets microcontrollers
(Cortex-M0/M0+, AVR-class cores) where an unaligned 16-bit access faults.

**Fix:** keep the file-record data in a properly aligned scratch array and `memcpy` it to/from the message buffer
while byte-swapping, instead of aliasing the message buffer.

**Harness:** `-fno-sanitize=alignment` is applied in `mayhem/build.sh`; every other check stays fatal.
