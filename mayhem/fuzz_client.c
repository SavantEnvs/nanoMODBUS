/* Fuzzes the nanoMODBUS client: the input is the byte stream a server answers with.
 * Byte 0 is the mode byte of client_calls.h (transport, function, size); the rest is the response. */
#include <stddef.h>
#include <stdint.h>

#include "client_calls.h"

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

// The default flush drains the whole response stream before the request goes out.
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

    nmbs_platform_conf conf;
    nmbs_platform_conf_create(&conf);
    conf.transport = (data[0] & 1) ? NMBS_TRANSPORT_TCP : NMBS_TRANSPORT_RTU;
    conf.read = stream_read;
    conf.write = stream_write;
    conf.flush = stream_flush;

    nmbs_t client;
    if (nmbs_client_create(&client, &conf) != NMBS_ERROR_NONE)
        return -1;
    nmbs_set_destination_rtu_address(&client, 1);

    (void) nmbs_strerror(client_call(&client, data[0]));
    return 0;
}
