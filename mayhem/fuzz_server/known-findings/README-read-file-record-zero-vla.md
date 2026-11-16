# Zero-length variable-length array in `handle_read_file_record` (nanomodbus.c)

**Where:** `handle_read_file_record()` declares `subreq[subreq_count]` with `subreq_count = request_size / 7`
*before* validating `request_size`.

**Reproducer:** `read-file-record-zero-vla.bin` fed to `fuzz_server` (TCP, FC 20, byte count 0): UBSan
`variable length array bound evaluates to non-positive value 0`.

**Impact:** undefined behaviour reachable from any client request with a Read File Record byte count below 7
(the later `request_size < 0x07` check that returns an exception runs too late).

**Fix:** validate `request_size` (`>= 7`, `<= 245`) before declaring the array, or always declare it with the
fixed maximum of 35 entries.
