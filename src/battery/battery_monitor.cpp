#include "battery/battery_monitor.h"
#include "config/config.h"

#include <cmath>
#include <cstdio>
#include "hardware/gpio.h"
#include "pico/time.h"

BatteryMonitor::BatteryMonitor() : sensor_(config::I2C_PORT) { critical_section_init(&lock_); }

void BatteryMonitor::init() {
    i2c_init(config::I2C_PORT, config::I2C_FREQUENCY);
    gpio_set_function(config::I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(config::I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(config::I2C_SDA_PIN); gpio_pull_up(config::I2C_SCL_PIN);
    const bool ok = sensor_.probeAndConfigure(config::INA_ADDRESSES,
        sizeof(config::INA_ADDRESSES), config::SHUNT_RESISTANCE, config::MAX_CURRENT);
    if (ok) printf("[INA] found 0x%02x\n[INA] device: %s\n", sensor_.address(), sensor_.typeName());
    else printf("[INA] sensor not found\n");
    next_scan_ms_ = ok ? 0 : config::SENSOR_RESCAN_MS;
}

void BatteryMonitor::task(uint64_t now_ms) {
    if (sensor_.type() == InaType::Unknown) {
        if (now_ms < next_scan_ms_) return;
        if (sensor_.probeAndConfigure(config::INA_ADDRESSES, sizeof(config::INA_ADDRESSES),
                                     config::SHUNT_RESISTANCE, config::MAX_CURRENT)) {
            printf("[INA] reconnected 0x%02x (%s)\n", sensor_.address(), sensor_.typeName());
            next_sample_ms_ = now_ms; last_valid_us_ = 0; filter_initialized_ = false;
        } else next_scan_ms_ = now_ms + config::SENSOR_RESCAN_MS;
        return;
    }
    if (now_ms < next_sample_ms_) return;
    next_sample_ms_ = now_ms + 1000 / config::SENSOR_SAMPLE_RATE_HZ;
    InaReading reading;
    if (!sensor_.read(reading)) {
        critical_section_enter_blocking(&lock_);
        measurements_.sensor_ok = false; measurements_.error = "i2c_timeout";
        critical_section_exit(&lock_);
        // Force a complete identity check on the next rescan.
        sensor_ = Ina2xx(config::I2C_PORT); next_scan_ms_ = now_ms + config::SENSOR_RESCAN_MS;
        return;
    }
    const bool valid = std::isfinite(reading.voltage_v) && std::isfinite(reading.current_a) &&
        std::isfinite(reading.power_w) && reading.voltage_v >= 0 && reading.voltage_v <= 85.0 &&
        reading.current_a >= -config::MAX_CURRENT && reading.current_a <= config::MAX_CURRENT;
    if (!valid) {
        critical_section_enter_blocking(&lock_);
        measurements_.sensor_ok = false; measurements_.error = "reading_out_of_range";
        critical_section_exit(&lock_); return;
    }
    setMeasurement(reading, now_ms);
}

void BatteryMonitor::setMeasurement(const InaReading& r, uint64_t now_ms) {
    const uint64_t now_us = time_us_64();
    critical_section_enter_blocking(&lock_);
    const double a = config::EMA_ALPHA;
    if (!filter_initialized_) { measurements_.voltage = r.voltage_v; measurements_.current = r.current_a; measurements_.power = r.power_w; filter_initialized_ = true; }
    else {
        measurements_.voltage += a * (r.voltage_v - measurements_.voltage);
        measurements_.current += a * (r.current_a - measurements_.current);
        measurements_.power += a * (r.power_w - measurements_.power);
    }
    if (last_valid_us_ != 0) {
        const double dt_h = (now_us - last_valid_us_) / 3.6e9;
        if (dt_h <= 1.0 / 60.0) { // never integrate across a long sensor outage
            measurements_.consumed_ah += r.current_a * dt_h;
            measurements_.consumed_wh += r.power_w * dt_h;
        }
    }
    last_valid_us_ = now_us;
    measurements_.raw_voltage = r.voltage_v; measurements_.raw_current = r.current_a; measurements_.raw_power = r.power_w;
    measurements_.raw = r.raw; measurements_.timestamp_ms = now_ms;
    measurements_.sensor_ok = true; measurements_.error = nullptr;
    critical_section_exit(&lock_);
}

Measurements BatteryMonitor::snapshot() const {
    critical_section_enter_blocking(&lock_); Measurements copy = measurements_; critical_section_exit(&lock_); return copy;
}
void BatteryMonitor::resetCounters() {
    critical_section_enter_blocking(&lock_); measurements_.consumed_ah = 0; measurements_.consumed_wh = 0; critical_section_exit(&lock_);
}
