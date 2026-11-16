# Stack buffer overflow from an attacker-chosen Object Id (nanomodbus.c, `recv_read_device_identification_res`)

**Where:** `recv_read_device_identification_res()` (nanomodbus.c, around line 889) in the client's Read Device
Identification (FC 43/14) response parser.

**Cause:** for `nmbs_read_device_identification_basic()` / `_regular()` the destination slot is
`buf_index = order[object_id]`, where `object_id` is a byte taken from the (untrusted) server response and `order`
is a 3- or 4-element stack array; the result then indexes `buffers_out[]` without any range check. The response
value is also written with `buffers_out[buf_index][object_length] = 0`, with `object_length` (up to 244) unrelated
to the caller's `buffers_length`, so a long object overruns a short caller buffer too.

**Reproducer:** `device-id-stack-overflow.bin`, fed to `fuzz_client` (first byte `0x36` selects TCP +
`nmbs_read_device_identification_basic`): ASan `stack-buffer-overflow ... READ of size 1 ... in
recv_read_device_identification_res`.

**Impact:** a malicious or faulty Modbus server can make the client read and write outside its stack arrays.

**Fix:** reject `object_id >= order_length` (and ids not requested) with `NMBS_ERROR_INVALID_RESPONSE`, and clamp
the terminator write to `buffers_length - 1`.

## All `fuzz_client` findings, by root cause

A 5-minute run reports 16 defects. They are three bugs, all in `nanomodbus.c`, all reached through correct
harness calls (128-byte buffers with a matching `buffers_length`):

| Root cause | Defects | Notes |
| --- | --- | --- |
| `recv_read_device_identification_res`: unchecked `object_id` / `object_length` (this file) | 14 | Same bug seen through the `_basic`, `_regular`, `_extended` and single-object entry points at lines 871, 872, 882, 885, 889, 892 and 896, as different CWEs |
| `recv_read_file_record_res`: unbounded sub-response length (`README-read-file-record-response-oob.md`) | 1 | `get_2` out-of-bounds read in `recv_msg_footer` |
| `recv_write_file_record_res`: unbounded record length (`README-write-file-record-response-oob.md`) | 1 | `nmbs_crc_calc` out-of-bounds read in `recv_msg_footer` |
