/* Fuzzes the nanoMODBUS server: the input is the byte stream a client sends to it.
 * Byte 0, bit 0 selects the transport (0 = RTU, 1 = TCP); the rest is the stream. */
#include <stddef.h>
#include <stdint.h>

#include "server_model.h"

static struct {
    const uint8_t* data;
    size_t size;
    size_t pos;
} stream;

static int32_t stream_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    (void) byte_timeout_ms;
    (void) arg;
    size_t left = stream.size - stream.pos;
    size_t n = count < left ? count : left;
    memcpy(buf, stream.data + stream.pos, n);
    stream.pos += n;
    return (int32_t) n;
}

static int32_t stream_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    (void) buf;
    (void) byte_timeout_ms;
    (void) arg;
    return count;
}

// Keep the rest of the stream after a rejected frame, so later frames are still parsed.
static void stream_flush(nmbs_t* nmbs, void* arg) {
    (void) nmbs;
    (void) arg;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1)
        return -1;

    stream.data = data + 1;
    stream.size = size - 1;
    stream.pos = 0;
    model_reset();

    nmbs_platform_conf conf;
    nmbs_platform_conf_create(&conf);
    conf.transport = (data[0] & 1) ? NMBS_TRANSPORT_TCP : NMBS_TRANSPORT_RTU;
    conf.read = stream_read;
    conf.write = stream_write;
    conf.flush = stream_flush;

    nmbs_callbacks callbacks;
    model_callbacks(&callbacks);

    nmbs_t server;
    if (nmbs_server_create(&server, 1, &conf, &callbacks) != NMBS_ERROR_NONE)
        return -1;

    while (stream.pos < stream.size)
        nmbs_server_poll(&server);

    return 0;
}
