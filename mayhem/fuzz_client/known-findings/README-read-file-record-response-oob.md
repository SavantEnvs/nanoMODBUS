# Unbounded data size in `recv_read_file_record_res` (nanomodbus.c)

**Where:** `recv_read_file_record_res()` (nanomodbus.c, around line 744) in the client's Read File Record (FC 20)
response parser.

**Cause:** `subreq_data_size = get_1(nmbs) - 1` takes the sub-response length byte from the (untrusted) server reply.
A byte of 0 wraps the `uint8_t` to 255, and `get_n(nmbs, subreq_data_size)` advances `buf_idx` to the end of the
260-byte message buffer without checking it against the bytes actually received (`response_size`). On RTU the
following `recv(nmbs, 2)` for the CRC then writes into the `buf_idx` member that sits right after `buf[260]`, so the
reply chooses `buf_idx`, and `get_2()` in `recv_msg_footer()` reads `buf[buf_idx]` out of bounds.

**Reproducer:** `read-file-record-response-oob.bin`, fed to `fuzz_client` (mode byte `0x10` = RTU +
`nmbs_read_file_record`; sub-response length 0; the two CRC bytes `00 40` become `buf_idx = 0x4000`). UBSan reports
`nanomodbus.c:72: index 16384 out of bounds for type 'uint8_t [260]'`; this is Mayhem defect #3625273.

**Impact:** a malicious or faulty Modbus server makes the client read outside its message buffer.

**Fix:** reject a sub-response length of 0 and require `subreq_data_size + 2 <= response_size` (and the total within
the buffer) before calling `get_n()`; return `NMBS_ERROR_INVALID_RESPONSE` otherwise.
