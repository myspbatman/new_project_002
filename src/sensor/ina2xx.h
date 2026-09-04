#pragma once

#include <cstdint>
#include "hardware/i2c.h"

enum class InaType { Unknown, INA228, INA238 };

struct InaRawReading {
    int32_t bus_voltage_raw = 0;
    int32_t shunt_voltage_raw = 0;
    int32_t current_raw = 0;
    uint32_t power_raw = 0;
    uint16_t diag_alrt = 0;
};

struct InaReading {
    double voltage_v = 0;
    double shunt_voltage_v = 0;
    double current_a = 0;
    double power_w = 0;
    InaRawReading raw;
};

class Ina2xx {
public:
    explicit Ina2xx(i2c_inst_t* bus) : bus_(bus) {}
    bool probeAndConfigure(const uint8_t* addresses, size_t count,
                           double shunt_ohm, double max_current_a);
    bool read(InaReading& out);
    InaType type() const { return type_; }
    uint8_t address() const { return address_; }
    const char* typeName() const;

private:
    bool readRegister(uint8_t reg, uint8_t* data, size_t length);
    bool write16(uint8_t reg, uint16_t value);
    bool detectAt(uint8_t address);
    static int32_t signExtend(uint32_t value, unsigned bits);

    i2c_inst_t* bus_;
    uint8_t address_ = 0;
    InaType type_ = InaType::Unknown;
    double current_lsb_ = 0;
};
