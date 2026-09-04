#pragma once
#include <cstddef>
#include "battery/battery_monitor.h"
#include "network/ch9120.h"

class RestApi {
public:
    RestApi(BatteryMonitor& battery, Ch9120& network) : battery_(battery), network_(network) {}
    size_t handle(const char* method, const char* path, char* body, size_t capacity,
                  const char*& content_type, int& status);
private:
    BatteryMonitor& battery_; Ch9120& network_;
};
