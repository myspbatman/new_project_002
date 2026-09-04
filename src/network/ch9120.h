#pragma once

#include <cstddef>
#include <cstdint>
#include "hardware/uart.h"

struct NetworkInfo {
    bool connected = false;
    bool dhcp = true;
    uint8_t ip[4] = {}, gateway[4] = {}, subnet[4] = {};
};

class Ch9120 {
public:
    void init(uint64_t now_ms);
    void task(uint64_t now_ms);
    int read(uint8_t* data, size_t capacity);
    size_t write(const uint8_t* data, size_t length);
    const NetworkInfo& info() const { return info_; }
    bool ready() const { return state_ == State::Ready; }

private:
    enum class State { WaitingDhcp, Ready, RetryPending };
    bool configure();
    bool command(uint8_t code, const uint8_t* params, size_t count,
                 uint8_t* response = nullptr, size_t response_len = 0);
    bool queryAddress(uint8_t command_code, uint8_t address[4]);
    bool queryNetwork();
    void enterConfig();
    void leaveConfig();
    void drain();
    void formatIp(const uint8_t ip[4], char* out, size_t size) const;

    uart_inst_t* uart_ = nullptr;
    NetworkInfo info_;
    State state_ = State::RetryPending;
    uint64_t deadline_ms_ = 0;
};
