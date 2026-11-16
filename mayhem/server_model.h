/* In-memory Modbus server data model shared by the fuzz harness, the KAT probe and the seed generator. */
#ifndef MAYHEM_SERVER_MODEL_H
#define MAYHEM_SERVER_MODEL_H

#include <stdint.h>
#include <string.h>

#include "nanomodbus.h"

#define MODEL_REGS 512
#define MODEL_FILES 4
#define MODEL_FILE_REGS 256

static struct {
    nmbs_bitfield coils;
    nmbs_bitfield discrete;
    uint16_t holding[MODEL_REGS];
    uint16_t input[MODEL_REGS];
    uint16_t files[MODEL_FILES][MODEL_FILE_REGS];
} model;

static void model_reset(void) {
    memset(&model, 0, sizeof(model));
    for (unsigned i = 0; i < NMBS_BITFIELD_BYTES_MAX; i++) {
        model.coils[i] = (uint8_t) (i * 37 + 5);
        model.discrete[i] = (uint8_t) (i * 91 + 3);
    }
    for (unsigned i = 0; i < MODEL_REGS; i++) {
        model.holding[i] = (uint16_t) (0x1234 + i * 257);
        model.input[i] = (uint16_t) (0xABCD - i * 7);
    }
    for (unsigned f = 0; f < MODEL_FILES; f++)
        for (unsigned i = 0; i < MODEL_FILE_REGS; i++)
            model.files[f][i] = (uint16_t) (f * 0x1000 + i);
}

static nmbs_error model_read_coils(uint16_t address, uint16_t quantity, nmbs_bitfield out, uint8_t unit_id, void* arg) {
    (void) unit_id;
    (void) arg;
    if ((uint32_t) address + quantity > NMBS_BITFIELD_MAX)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    for (uint16_t i = 0; i < quantity; i++)
        nmbs_bitfield_write(out, i, nmbs_bitfield_read(model.coils, address + i));
    return NMBS_ERROR_NONE;
}

static nmbs_error model_read_discrete(uint16_t address, uint16_t quantity, nmbs_bitfield out, uint8_t unit_id,
                                      void* arg) {
    (void) unit_id;
    (void) arg;
    if ((uint32_t) address + quantity > NMBS_BITFIELD_MAX)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    for (uint16_t i = 0; i < quantity; i++)
        nmbs_bitfield_write(out, i, nmbs_bitfield_read(model.discrete, address + i));
    return NMBS_ERROR_NONE;
}

static nmbs_error model_read_holding(uint16_t address, uint16_t quantity, uint16_t* out, uint8_t unit_id, void* arg) {
    (void) unit_id;
    (void) arg;
    if ((uint32_t) address + quantity > MODEL_REGS)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    memcpy(out, &model.holding[address], quantity * sizeof(uint16_t));
    return NMBS_ERROR_NONE;
}

static nmbs_error model_read_input(uint16_t address, uint16_t quantity, uint16_t* out, uint8_t unit_id, void* arg) {
    (void) unit_id;
    (void) arg;
    if ((uint32_t) address + quantity > MODEL_REGS)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    memcpy(out, &model.input[address], quantity * sizeof(uint16_t));
    return NMBS_ERROR_NONE;
}

static nmbs_error model_write_single_coil(uint16_t address, bool value, uint8_t unit_id, void* arg) {
    (void) unit_id;
    (void) arg;
    if (address >= NMBS_BITFIELD_MAX)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    nmbs_bitfield_write(model.coils, address, value);
    return NMBS_ERROR_NONE;
}

static nmbs_error model_write_single_register(uint16_t address, uint16_t value, uint8_t unit_id, void* arg) {
    (void) unit_id;
    (void) arg;
    if (address >= MODEL_REGS)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    model.holding[address] = value;
    return NMBS_ERROR_NONE;
}

static nmbs_error model_write_multiple_coils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils,
                                             uint8_t unit_id, void* arg) {
    (void) unit_id;
    (void) arg;
    if ((uint32_t) address + quantity > NMBS_BITFIELD_MAX)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    for (uint16_t i = 0; i < quantity; i++)
        nmbs_bitfield_write(model.coils, address + i, nmbs_bitfield_read(coils, i));
    return NMBS_ERROR_NONE;
}

static nmbs_error model_write_multiple_registers(uint16_t address, uint16_t quantity, const uint16_t* registers,
                                                 uint8_t unit_id, void* arg) {
    (void) unit_id;
    (void) arg;
    if ((uint32_t) address + quantity > MODEL_REGS)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    memcpy(&model.holding[address], registers, quantity * sizeof(uint16_t));
    return NMBS_ERROR_NONE;
}

static nmbs_error model_read_file_record(uint16_t file_number, uint16_t record_number, uint16_t* registers,
                                         uint16_t count, uint8_t unit_id, void* arg) {
    (void) unit_id;
    (void) arg;
    if (file_number < 1 || file_number > MODEL_FILES || (uint32_t) record_number + count > MODEL_FILE_REGS)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    memcpy(registers, &model.files[file_number - 1][record_number], count * sizeof(uint16_t));
    return NMBS_ERROR_NONE;
}

static nmbs_error model_write_file_record(uint16_t file_number, uint16_t record_number, const uint16_t* registers,
                                          uint16_t count, uint8_t unit_id, void* arg) {
    (void) unit_id;
    (void) arg;
    if (file_number < 1 || file_number > MODEL_FILES || (uint32_t) record_number + count > MODEL_FILE_REGS)
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    memcpy(&model.files[file_number - 1][record_number], registers, count * sizeof(uint16_t));
    return NMBS_ERROR_NONE;
}

static nmbs_error model_device_identification_map(nmbs_bitfield_256 map) {
    for (unsigned id = 0; id <= 6; id++)
        nmbs_bitfield_set(map, id);
    nmbs_bitfield_set(map, 0x80);
    nmbs_bitfield_set(map, 0x91);
    nmbs_bitfield_set(map, 0xA2);
    nmbs_bitfield_set(map, 0xB3);
    return NMBS_ERROR_NONE;
}

static nmbs_error model_device_identification(uint8_t object_id, char buffer[NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH]) {
    static const char long_object[] =
            "90byteslongextendedobjectthatcombinedwithotheronesisdefinitelygonnaexceedthepdusize0123456";
    switch (object_id) {
        case 0x00:
            strcpy(buffer, "VendorName");
            return NMBS_ERROR_NONE;
        case 0x01:
            strcpy(buffer, "ProductCode");
            return NMBS_ERROR_NONE;
        case 0x02:
            strcpy(buffer, "MajorMinorRevision");
            return NMBS_ERROR_NONE;
        case 0x06:
            strcpy(buffer, "UserApplicationName");
            return NMBS_ERROR_NONE;
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x80:
        case 0x91:
        case 0xA2:
        case 0xB3:
            strcpy(buffer, long_object);
            return NMBS_ERROR_NONE;
        default:
            return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }
}

static void model_callbacks(nmbs_callbacks* cb) {
    nmbs_callbacks_create(cb);
    cb->read_coils = model_read_coils;
    cb->read_discrete_inputs = model_read_discrete;
    cb->read_holding_registers = model_read_holding;
    cb->read_input_registers = model_read_input;
    cb->write_single_coil = model_write_single_coil;
    cb->write_single_register = model_write_single_register;
    cb->write_multiple_coils = model_write_multiple_coils;
    cb->write_multiple_registers = model_write_multiple_registers;
    cb->read_file_record = model_read_file_record;
    cb->write_file_record = model_write_file_record;
    cb->read_device_identification = model_device_identification;
    cb->read_device_identification_map = model_device_identification_map;
}

#endif
