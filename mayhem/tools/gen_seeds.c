/* Regenerates the seed corpora: gcc -I. -Imayhem mayhem/tools/gen_seeds.c nanomodbus.c -o gen_seeds && ./gen_seeds mayhem
 * Server seeds are the requests the library's own client emits; client seeds are the responses the library's own
 * server (on the shared data model) gives to those requests. Every client seed is checked to parse without error. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client_calls.h"
#include "server_model.h"

static uint8_t in_buf[4096];
static size_t in_len, in_pos;
static uint8_t out_buf[4096];
static size_t out_len;

static int32_t g_read(uint8_t* buf, uint16_t count, int32_t t, void* a) {
    (void) t;
    (void) a;
    size_t left = in_len - in_pos;
    size_t n = count < left ? count : left;
    memcpy(buf, in_buf + in_pos, n);
    in_pos += n;
    return (int32_t) n;
}

static int32_t g_write(const uint8_t* buf, uint16_t count, int32_t t, void* a) {
    (void) t;
    (void) a;
    memcpy(out_buf + out_len, buf, count);
    out_len += count;
    return count;
}

static void g_flush(nmbs_t* n, void* a) {
    (void) n;
    (void) a;
}

static void conf_create(nmbs_platform_conf* c, int tcp) {
    nmbs_platform_conf_create(c);
    c->transport = tcp ? NMBS_TRANSPORT_TCP : NMBS_TRANSPORT_RTU;
    c->read = g_read;
    c->write = g_write;
    c->flush = g_flush;
}

static void save(const char* dir, const char* target, const char* name, const uint8_t* data, size_t n) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s/testsuite/%s", dir, target, name);
    FILE* f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fwrite(data, 1, n, f);
    fclose(f);
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : "mayhem";
    static const uint8_t qs[] = {1, 2};
    uint8_t mixed[2][1024];
    size_t mixed_len[2] = {0, 0};
    int failures = 0;

    for (int tcp = 0; tcp < 2; tcp++) {
        mixed[tcp][0] = (uint8_t) tcp;
        mixed_len[tcp] = 1;
        for (unsigned fn = 0; fn < 16; fn++) {
            for (unsigned qi = 0; qi < sizeof(qs); qi++) {
                const uint8_t mode = (uint8_t) (tcp | (fn << 1) | (qs[qi] << 5));
                char name[64];
                snprintf(name, sizeof(name), "%s_fn%02u_q%u", tcp ? "tcp" : "rtu", fn, qs[qi]);

                // 1) capture the client's request (no response available: the call times out)
                nmbs_platform_conf conf;
                conf_create(&conf, tcp);
                nmbs_t cli;
                nmbs_client_create(&cli, &conf);
                nmbs_set_destination_rtu_address(&cli, 1);
                in_len = in_pos = out_len = 0;
                client_call(&cli, mode);
                uint8_t request[512];
                size_t request_len = out_len;
                memcpy(request, out_buf, request_len);

                uint8_t seed[2500];
                seed[0] = (uint8_t) tcp;
                memcpy(seed + 1, request, request_len);
                save(dir, "fuzz_server", name, seed, 1 + request_len);
                if (qi == 0 && mixed_len[tcp] + request_len < sizeof(mixed[tcp])) {
                    memcpy(mixed[tcp] + mixed_len[tcp], request, request_len);
                    mixed_len[tcp] += request_len;
                }

                // 2) answer every request the call emits (multi-request calls need several rounds)
                uint8_t response[2400];
                size_t response_len = 0;
                size_t answered = 0;
                nmbs_error err = NMBS_ERROR_TIMEOUT;
                for (int round = 0; round < 8 && err != NMBS_ERROR_NONE; round++) {
                    nmbs_platform_conf conf2;
                    conf_create(&conf2, tcp);
                    nmbs_t cli2;
                    nmbs_client_create(&cli2, &conf2);
                    nmbs_set_destination_rtu_address(&cli2, 1);
                    memcpy(in_buf, response, response_len);
                    in_len = response_len;
                    in_pos = out_len = 0;
                    err = client_call(&cli2, mode);
                    if (err == NMBS_ERROR_NONE || out_len <= answered)
                        break;

                    model_reset();
                    nmbs_platform_conf sconf;
                    conf_create(&sconf, tcp);
                    nmbs_callbacks cb;
                    model_callbacks(&cb);
                    nmbs_t srv;
                    nmbs_server_create(&srv, 1, &sconf, &cb);
                    size_t rl = out_len - answered;
                    uint8_t req[512];
                    memcpy(req, out_buf + answered, rl);
                    answered = out_len;
                    memcpy(in_buf, req, rl);
                    in_len = rl;
                    in_pos = out_len = 0;
                    nmbs_server_poll(&srv);
                    memcpy(response + response_len, out_buf, out_len);
                    response_len += out_len;
                }
                if (err != NMBS_ERROR_NONE) {
                    fprintf(stderr, "%s: client rejected the server response (%s)\n", name, nmbs_strerror(err));
                    failures++;
                }
                seed[0] = mode;
                memcpy(seed + 1, response, response_len);
                save(dir, "fuzz_client", name, seed, 1 + response_len);
            }
        }
    }
    save(dir, "fuzz_server", "rtu_mixed", mixed[0], mixed_len[0]);
    save(dir, "fuzz_server", "tcp_mixed", mixed[1], mixed_len[1]);

    static const uint8_t bad_fc[] = {1, 0, 1, 0, 0, 0, 2, 1, 0x63};
    save(dir, "fuzz_server", "tcp_unknown_fc", bad_fc, sizeof(bad_fc));
    static const uint8_t bad_len[] = {1, 0, 1, 0, 0, 0xFF, 0xFF, 1, 3, 0, 0, 0, 1};
    save(dir, "fuzz_server", "tcp_bad_length", bad_len, sizeof(bad_len));
    static const uint8_t exc[] = {1, 1, 0, 1, 1, 0, 0, 3, 1, 0x83, 2};
    save(dir, "fuzz_client", "tcp_exception", (const uint8_t[]){5, 0, 1, 0, 0, 0, 3, 1, 0x83, 2}, 10);
    (void) exc;
    return failures ? 1 : 0;
}
