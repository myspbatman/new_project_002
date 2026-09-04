#pragma once

#include <cstddef>
#include <cstdint>
#include "api/rest_api.h"
#include "network/ch9120.h"

class HttpServer {
public:
    HttpServer(Ch9120& transport, RestApi& api) : transport_(transport), api_(api) {}
    void task(uint64_t now_ms);
    void reset();
private:
    void process();
    Ch9120& transport_; RestApi& api_;
    char request_[1025] = {};
    size_t used_ = 0;
    uint64_t last_byte_ms_ = 0;
};
