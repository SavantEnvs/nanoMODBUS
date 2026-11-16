/* One call of every nanoMODBUS client function, selected by a mode byte.
 * bit 0: transport (0 = RTU, 1 = TCP), bits 1-4: function, bits 5-7: size selector. */
#ifndef MAYHEM_CLIENT_CALLS_H
#define MAYHEM_CLIENT_CALLS_H

#include <stdint.h>
#include <string.h>

#include "nanomodbus.h"

static nmbs_error client_call(nmbs_t* nmbs, uint8_t mode) {
    static const uint16_t coil_quantities[8] = {1, 2, 8, 9, 16, 100, 1000, 2000};
    static const uint16_t reg_quantities[8] = {1, 2, 3, 8, 17, 64, 100, 125};
    static const uint16_t file_counts[8] = {1, 2, 3, 4, 8, 32, 100, 124};

    const unsigned fn = (mode >> 1) & 15;
    const unsigned q = mode >> 5;
    const uint8_t object_id = q < 7 ? (uint8_t) q : 0x80;

    nmbs_bitfield bits;
    uint16_t regs[125];
    uint16_t regs_in[125];
    memset(bits, 0xA5, sizeof(bits));
    memset(regs, 0, sizeof(regs));
    for (unsigned i = 0; i < 125; i++)
        regs_in[i] = (uint16_t) (0x4000 + i);

    switch (fn) {
        case 0:
            return nmbs_read_coils(nmbs, 0x13, coil_quantities[q], bits);
        case 1:
            return nmbs_read_discrete_inputs(nmbs, 0x13, coil_quantities[q], bits);
        case 2:
            return nmbs_read_holding_registers(nmbs, 0x13, reg_quantities[q], regs);
        case 3:
            return nmbs_read_input_registers(nmbs, 0x13, reg_quantities[q], regs);
        case 4:
            return nmbs_write_single_coil(nmbs, 0x13, q & 1);
        case 5:
            return nmbs_write_single_register(nmbs, 0x13, (uint16_t) (0x234 + 0x1000 * q));
        case 6:
            return nmbs_write_multiple_coils(nmbs, 0x13, coil_quantities[q], bits);
        case 7:
            return nmbs_write_multiple_registers(nmbs, 0x13, reg_quantities[q], regs_in);
        case 8:
            return nmbs_read_file_record(nmbs, (uint16_t) (1 + (q & 3)), (uint16_t) (2 * q), regs, file_counts[q]);
        case 9:
            return nmbs_write_file_record(nmbs, (uint16_t) (1 + (q & 3)), (uint16_t) (2 * q), regs_in, file_counts[q]);
        case 10:
            return nmbs_read_write_registers(nmbs, 0x13, reg_quantities[q], regs, 0x20, reg_quantities[(q + 3) & 7],
                                             regs_in);
        case 11: {
            char a[128], b[128], c[128];
            return nmbs_read_device_identification_basic(nmbs, a, b, c, sizeof(a));
        }
        case 12: {
            char a[128], b[128], c[128], d[128];
            return nmbs_read_device_identification_regular(nmbs, a, b, c, d, sizeof(a));
        }
        case 13: {
            char mem[16][128];
            char* buffers[16];
            uint8_t ids[16];
            uint8_t count = 0;
            for (unsigned i = 0; i < 16; i++)
                buffers[i] = mem[i];
            return nmbs_read_device_identification_extended(nmbs, q < 4 ? (uint8_t) (0x80 + 0x11 * q) : (uint8_t) q, ids, buffers, 16, 128, &count);
        }
        case 14: {
            char buf[128];
            return nmbs_read_device_identification(nmbs, object_id, buf, sizeof(buf));
        }
        default: {
            uint8_t out[5];
            nmbs_error err = nmbs_send_raw_pdu(nmbs, 3, (const uint8_t[]){0x00, 0x13, 0x00, 0x02}, 4);
            if (err != NMBS_ERROR_NONE)
                return err;
            return nmbs_receive_raw_pdu_response(nmbs, out, sizeof(out));
        }
    }
}

#endif
