#include "sensor/ina2xx.h"

#include <cmath>

namespace {
constexpr uint8_t REG_CONFIG = 0x00;
constexpr uint8_t REG_ADC_CONFIG = 0x01;
constexpr uint8_t REG_SHUNT_CAL = 0x02;
constexpr uint8_t REG_VSHUNT = 0x04;
constexpr uint8_t REG_VBUS = 0x05;
constexpr uint8_t REG_CURRENT = 0x07;
constexpr uint8_t REG_POWER = 0x08;
constexpr uint8_t REG_DIAG_ALRT = 0x0B;
constexpr uint8_t REG_MANUFACTURER_ID = 0x3E;
constexpr uint8_t REG_DEVICE_ID = 0x3F;
constexpr uint16_t TI_MANUFACTURER_ID = 0x5449;
constexpr uint16_t DEVICE_INA228 = 0x228;
constexpr uint16_t DEVICE_INA238 = 0x238;
}

bool Ina2xx::readRegister(uint8_t reg, uint8_t* data, size_t length) {
    if (i2c_write_timeout_us(bus_, address_, &reg, 1, true, 3000) != 1) return false;
    return i2c_read_timeout_us(bus_, address_, data, length, false, 3000) == static_cast<int>(length);
}

bool Ina2xx::write16(uint8_t reg, uint16_t value) {
    const uint8_t data[] = {reg, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    return i2c_write_timeout_us(bus_, address_, data, sizeof(data), false, 3000) == sizeof(data);
}

bool Ina2xx::detectAt(uint8_t address) {
    address_ = address;
    uint8_t bytes[2];
    if (!readRegister(REG_MANUFACTURER_ID, bytes, 2)) return false;
    const uint16_t manufacturer = (uint16_t(bytes[0]) << 8) | bytes[1];
    if (manufacturer != TI_MANUFACTURER_ID || !readRegister(REG_DEVICE_ID, bytes, 2)) return false;
    const uint16_t device = ((uint16_t(bytes[0]) << 8) | bytes[1]) >> 4;
    if (device == DEVICE_INA228) type_ = InaType::INA228;
    else if (device == DEVICE_INA238) type_ = InaType::INA238;
    else return false;
    return true;
}

bool Ina2xx::probeAndConfigure(const uint8_t* addresses, size_t count,
                               double shunt_ohm, double max_current_a) {
    type_ = InaType::Unknown;
    for (size_t i = 0; i < count && type_ == InaType::Unknown; ++i) detectAt(addresses[i]);
    if (type_ == InaType::Unknown) { address_ = 0; return false; }

    // ADCRANGE=0 gives +/-163.84 mV and covers 204.8 A * 200 uOhm = 40.96 mV.
    if (!write16(REG_CONFIG, 0x0000)) return false;
    // Continuous temperature+shunt+bus, 1052 us conversions, 16-sample average.
    if (!write16(REG_ADC_CONFIG, 0xFB6A)) return false;

    if (type_ == InaType::INA228) {
        current_lsb_ = max_current_a / 524288.0; // 2^19 signed current register
        const auto cal = static_cast<uint16_t>(std::lround(13107.2e6 * current_lsb_ * shunt_ohm));
        return write16(REG_SHUNT_CAL, cal);
    }
    current_lsb_ = max_current_a / 32768.0; // 2^15 signed current register
    const auto cal = static_cast<uint16_t>(std::lround(819.2e6 * current_lsb_ * shunt_ohm));
    return write16(REG_SHUNT_CAL, cal);
}

int32_t Ina2xx::signExtend(uint32_t value, unsigned bits) {
    const uint32_t sign = 1u << (bits - 1);
    return static_cast<int32_t>((value ^ sign) - sign);
}

bool Ina2xx::read(InaReading& out) {
    uint8_t b[3];
    if (type_ == InaType::INA228) {
        if (!readRegister(REG_VBUS, b, 3)) return false;
        out.raw.bus_voltage_raw = static_cast<int32_t>(((uint32_t(b[0]) << 16) | (uint32_t(b[1]) << 8) | b[2]) >> 4);
        if (!readRegister(REG_VSHUNT, b, 3)) return false;
        out.raw.shunt_voltage_raw = signExtend(((uint32_t(b[0]) << 16) | (uint32_t(b[1]) << 8) | b[2]) >> 4, 20);
        if (!readRegister(REG_CURRENT, b, 3)) return false;
        out.raw.current_raw = signExtend(((uint32_t(b[0]) << 16) | (uint32_t(b[1]) << 8) | b[2]) >> 4, 20);
        if (!readRegister(REG_POWER, b, 3)) return false;
        out.raw.power_raw = (uint32_t(b[0]) << 16) | (uint32_t(b[1]) << 8) | b[2];
        out.voltage_v = out.raw.bus_voltage_raw * 195.3125e-6;
        out.shunt_voltage_v = out.raw.shunt_voltage_raw * 312.5e-9;
        out.current_a = out.raw.current_raw * current_lsb_;
        out.power_w = out.raw.power_raw * 3.2 * current_lsb_;
    } else if (type_ == InaType::INA238) {
        if (!readRegister(REG_VBUS, b, 2)) return false;
        out.raw.bus_voltage_raw = int32_t((uint16_t(b[0]) << 8) | b[1]);
        if (!readRegister(REG_VSHUNT, b, 2)) return false;
        out.raw.shunt_voltage_raw = int16_t((uint16_t(b[0]) << 8) | b[1]);
        if (!readRegister(REG_CURRENT, b, 2)) return false;
        out.raw.current_raw = int16_t((uint16_t(b[0]) << 8) | b[1]);
        if (!readRegister(REG_POWER, b, 3)) return false;
        out.raw.power_raw = (uint32_t(b[0]) << 16) | (uint32_t(b[1]) << 8) | b[2];
        out.voltage_v = out.raw.bus_voltage_raw * 3.125e-3;
        out.shunt_voltage_v = out.raw.shunt_voltage_raw * 5.0e-6;
        out.current_a = out.raw.current_raw * current_lsb_;
        out.power_w = out.raw.power_raw * 0.2 * current_lsb_;
    } else return false;
    if (!readRegister(REG_DIAG_ALRT, b, 2)) return false;
    out.raw.diag_alrt = (uint16_t(b[0]) << 8) | b[1];
    return true;
}

const char* Ina2xx::typeName() const {
    return type_ == InaType::INA228 ? "INA228" : type_ == InaType::INA238 ? "INA238" : "unknown";
}
