#pragma once
#include <cstdint>
class StatusLed {
public:
    void init();
    void task(uint64_t now_ms, bool network_ready, bool sensor_ok);
private:
    void set(uint8_t red, uint8_t green, uint8_t blue);
    uint64_t next_update_ms_ = 0;
    bool phase_ = false;
    unsigned int sm_ = 0;
    unsigned int offset_ = 0;
};
