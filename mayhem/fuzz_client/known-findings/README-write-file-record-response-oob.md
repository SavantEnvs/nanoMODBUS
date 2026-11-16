# Unbounded record length in `recv_write_file_record_res` (nanomodbus.c)

**Where:** `recv_write_file_record_res()` (nanomodbus.c, around line 795) in the client's Write File Record (FC 21)
response parser.

**Cause:** `subreq_data_size = subreq_record_length * 2` uses the 16-bit record length from the (untrusted) server
reply, wraps in `uint16_t`, and is passed to `get_n()` without comparison to `response_size` or to the 260-byte
message buffer. `buf_idx` then points past the buffer, and `recv_msg_footer()` runs `nmbs_crc_calc(buf, buf_idx, ...)`
over that many bytes, reading far outside the client's stack object. (The caller's expected `count` is only compared
after the footer has been read.)

**Reproducer:** `write-file-record-response-oob.bin`, fed to `fuzz_client` (mode byte `0x12` = RTU +
`nmbs_write_file_record`; record length `0x0400`). ASan reports `stack-buffer-overflow ... READ of size 1 in
nmbs_crc_calc` (nanomodbus.c:285) called from `recv_msg_footer` <- `recv_write_file_record_res`; this is Mayhem
defect #3625256.

**Impact:** a malicious or faulty Modbus server makes the client read outside its stack.

**Fix:** validate `subreq_record_length * 2` against `response_size` (and the message buffer) before `get_n()`, and
compare it to the expected `count` first.
