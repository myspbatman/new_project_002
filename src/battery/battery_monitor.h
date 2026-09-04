#pragma once

#include <cstdint>
#include "sensor/ina2xx.h"
#include "pico/critical_section.h"

struct Measurements {
    double voltage = 0, current = 0, power = 0;
    double raw_voltage = 0, raw_current = 0, raw_power = 0;
    double consumed_ah = 0, consumed_wh = 0;
    uint64_t timestamp_ms = 0;
    bool sensor_ok = false;
    const char* error = "sensor_not_found";
    InaRawReading raw;
};

class BatteryMonitor {
public:
    BatteryMonitor();
    void init();
    void task(uint64_t now_ms);
    Measurements snapshot() const;
    void resetCounters();
    const Ina2xx& sensor() const { return sensor_; }

private:
    void setMeasurement(const InaReading& reading, uint64_t now_ms);
    Ina2xx sensor_;
    mutable critical_section_t lock_;
    Measurements measurements_;
    uint64_t next_sample_ms_ = 0, next_scan_ms_ = 0, last_valid_us_ = 0;
    bool filter_initialized_ = false;
};
