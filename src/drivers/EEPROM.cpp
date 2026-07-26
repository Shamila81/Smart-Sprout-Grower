#include "EEPROM.h"
#include "memory"
#include <cstring>

EEPROM::EEPROM (std::shared_ptr<PicoI2C> &bus, uint8_t i2c_addr)
        : bus (bus), address(i2c_addr){}

void EEPROM::eeprom_write_numeric(uint16_t addr, uint16_t value) {

    uint8_t buffer[6];
    buffer[0] = (uint8_t)((addr >> 8) & 0xFF);
    buffer[1] = (uint8_t)(addr & 0xFF);

    buffer[2] = (uint8_t)(value & 0xFF);
    buffer[3] = (uint8_t)((value >> 8) & 0xFF);

    buffer[4] = (uint8_t)~buffer[2];
    buffer[5] = (uint8_t)~buffer[3];

    bus ->write(address, buffer, 6);

    sleep_ms(30);
}

uint16_t EEPROM::eeprom_read_numeric(uint16_t addr, uint16_t default_value) {
    uint8_t addr_buf[2] = {(uint8_t)((addr >> 8) & 0xFF), (uint8_t)(addr & 0xFF)};
    uint8_t data[4] = {0};

    bus -> transaction (address, addr_buf, 2, data, 4);

    if(value_is_valid(data)){
        uint16_t value = (data[1] << 8) | data[0];
        return value;
    }
    else {
        return default_value;
    }
}

bool EEPROM::value_is_valid(uint8_t *data) {

    if(data[0] ==(uint8_t )~data[2] && data[1] ==(uint8_t )~data[3] ){
        return true;
    }
    else{
        return false;
    }
}

void EEPROM::eeprom_write_block(uint16_t addr, const uint8_t *data, size_t len) {
    uint8_t buf[len + 2];
    buf[0] = (addr >> 8) & 0xFF;   // high address byte
    buf[1] = addr & 0xFF;          // low address byte
    memcpy(&buf[2], data, len);    // copy data after address bytes

    bus->write(address, buf, len + 2);

    sleep_ms(5);
}

void EEPROM::eeprom_read_block(uint16_t addr, uint8_t *data, size_t len) {
    uint8_t addr_bytes[2] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF) };

    bus->write(address, addr_bytes, 2);
    bus->read(address, data, len);
}

void EEPROM::eeprom_write_string(uint16_t addr, const std::string &str, size_t max_len) {
    uint8_t buf[max_len];
    memset(buf, 0, max_len);
    strncpy((char *)buf, str.c_str(), max_len - 1);
    eeprom_write_block(addr, buf, max_len);
}

void EEPROM::eeprom_read_string(uint16_t addr, std::string &out, size_t max_len) {
    uint8_t buf[max_len];
    eeprom_read_block(addr, buf, max_len);
    buf[max_len - 1] = '\0';
    out = std::string((char *)buf);
}