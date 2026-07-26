#pragma once
#include "i2c/PicoI2C.h"
#include <memory>
#include <cstdint>


class EEPROM {
public:
    EEPROM(std::shared_ptr<PicoI2C> &bus, uint8_t i2c_addr = 0x50);

    void     eeprom_write_numeric(uint16_t addr, uint16_t value);
    uint16_t eeprom_read_numeric(uint16_t addr, uint16_t default_value);


    bool value_is_valid(uint8_t *data);
    void eeprom_read_block(uint16_t addr, uint8_t *data, size_t len);
    void eeprom_write_block(uint16_t addr, const uint8_t *data, size_t len);

    void eeprom_write_string(uint16_t addr, const std::string &str, size_t max_len);
    void eeprom_read_string(uint16_t addr, std::string &out, size_t max_len);

private:
    std::shared_ptr<PicoI2C> bus;
    uint8_t address;
};