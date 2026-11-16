/* Known-answer probe for mayhem/test.sh: drives the server and the client with fixed Modbus frames and prints the
 * bytes/values the library produced. test.sh compares every line against independently derived values. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server_model.h"

static uint8_t in_buf[1024];
static size_t in_len, in_pos;
static uint8_t out_buf[1024];
static size_t out_len;

static int32_t kat_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    (void) byte_timeout_ms;
    (void) arg;
    size_t left = in_len - in_pos;
    size_t n = count < left ? count : left;
    memcpy(buf, in_buf + in_pos, n);
    in_pos += n;
    return (int32_t) n;
}

static int32_t kat_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    (void) byte_timeout_ms;
    (void) arg;
    memcpy(out_buf + out_len, buf, count);
    out_len += count;
    return count;
}

static void kat_flush(nmbs_t* nmbs, void* arg) {
    (void) nmbs;
    (void) arg;
}

static void load(const char* hex) {
    in_len = in_pos = out_len = 0;
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
        char pair[3] = {hex[i], hex[i + 1], 0};
        in_buf[in_len++] = (uint8_t) strtoul(pair, NULL, 16);
    }
}

static void print_hex(const uint8_t* b, size_t n) {
    if (n == 0)
        printf("none");
    for (size_t i = 0; i < n; i++)
        printf("%02x", b[i]);
}

static void conf_create(nmbs_platform_conf* conf, nmbs_transport t) {
    nmbs_platform_conf_create(conf);
    conf->transport = t;
    conf->read = kat_read;
    conf->write = kat_write;
    conf->flush = kat_flush;
}

static void server_case(const char* name, nmbs_transport t, const char* request_hex, int polls) {
    model_reset();
    load(request_hex);
    nmbs_platform_conf conf;
    conf_create(&conf, t);
    nmbs_callbacks cb;
    model_callbacks(&cb);
    nmbs_t srv;
    if (nmbs_server_create(&srv, 1, &conf, &cb) != NMBS_ERROR_NONE) {
        printf("%s: create-failed\n", name);
        return;
    }
    for (int i = 0; i < polls; i++)
        nmbs_server_poll(&srv);
    printf("%s: ", name);
    print_hex(out_buf, out_len);
    printf("\n");
}

static void client_case(const char* name, nmbs_transport t, const char* response_hex, uint8_t mode_fn_q) {
    load(response_hex);
    nmbs_platform_conf conf;
    conf_create(&conf, t);
    nmbs_t cli;
    nmbs_client_create(&cli, &conf);
    nmbs_set_destination_rtu_address(&cli, 1);

    uint16_t regs[8] = {0};
    nmbs_error err;
    if (mode_fn_q == 0)
        err = nmbs_read_holding_registers(&cli, 2, 2, regs);
    else if (mode_fn_q == 1)
        err = nmbs_read_holding_registers(&cli, 0, 1, regs);
    else
        err = nmbs_read_holding_registers(&cli, 600, 1, regs);
    printf("%s: req=", name);
    print_hex(out_buf, out_len);
    if (err == NMBS_ERROR_NONE)
        printf(" err=0 regs=%04x,%04x\n", regs[0], regs[1]);
    else
        printf(" err=%d\n", (int) err);
}

int main(void) {
    server_case("srv-tcp-read-holding", NMBS_TRANSPORT_TCP, "000100000006010300020002", 1);
    server_case("srv-tcp-write-then-read", NMBS_TRANSPORT_TCP, "00020000000601060005beef" "000300000006010300050001", 2);
    server_case("srv-tcp-exception", NMBS_TRANSPORT_TCP, "000400000006010302580001", 1);
    server_case("srv-tcp-read-coils", NMBS_TRANSPORT_TCP, "000500000006010100000008", 1);
    server_case("srv-tcp-read-input", NMBS_TRANSPORT_TCP, "000600000006010400000001", 1);
    server_case("srv-tcp-write-multiple-then-read", NMBS_TRANSPORT_TCP,
                "00070000000b0110001000020400010002" "000800000006010300100002", 2);
    server_case("srv-rtu-read-holding", NMBS_TRANSPORT_RTU, "010300000001840a", 1);
    server_case("srv-rtu-write-single", NMBS_TRANSPORT_RTU, "01060005beefa9e7", 1);
    server_case("srv-rtu-bad-crc", NMBS_TRANSPORT_RTU, "010300000001840b", 1);
    server_case("srv-rtu-other-unit", NMBS_TRANSPORT_RTU, "0203000000018439", 1);

    client_case("cli-tcp-read-holding", NMBS_TRANSPORT_TCP, "00010000000701030414361537", 0);
    client_case("cli-rtu-read-holding", NMBS_TRANSPORT_RTU, "0103021234b533", 1);
    client_case("cli-rtu-bad-crc", NMBS_TRANSPORT_RTU, "0103021234b534", 1);
    client_case("cli-tcp-exception", NMBS_TRANSPORT_TCP, "000100000003018302", 2);
    return 0;
}
