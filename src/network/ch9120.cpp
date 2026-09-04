#include "network/ch9120.h"
#include "config/config.h"

#include <cstdio>
#include "hardware/gpio.h"
#include "pico/time.h"

namespace {
constexpr uint8_t PREFIX[] = {0x57, 0xAB};
constexpr uint8_t CMD_SET_MODE = 0x10, CMD_SET_IP = 0x11, CMD_SET_MASK = 0x12;
constexpr uint8_t CMD_SET_GATEWAY = 0x13, CMD_SET_PORT = 0x14, CMD_SET_BAUD = 0x21;
constexpr uint8_t CMD_SET_UART_FORMAT = 0x22, CMD_SET_TIMEOUT = 0x23;
constexpr uint8_t CMD_DISCONNECT_ON_LINK_LOSS = 0x24, CMD_RX_LENGTH = 0x25;
constexpr uint8_t CMD_FLUSH_ON_CONNECT = 0x26, CMD_DHCP = 0x33;
constexpr uint8_t CMD_GET_IP = 0x61, CMD_GET_MASK = 0x62, CMD_GET_GATEWAY = 0x63;
constexpr uint8_t CMD_SAVE = 0x0D, CMD_APPLY_AND_RESET = 0x0E;
}

void Ch9120::drain() { while (uart_is_readable(uart_)) (void)uart_getc(uart_); }

void Ch9120::enterConfig() {
    gpio_put(config::CH9120_CFG0_PIN, 0); sleep_ms(20);
    uart_set_baudrate(uart_, config::CH9120_CONFIG_BAUD); drain();
}
void Ch9120::leaveConfig() {
    gpio_put(config::CH9120_CFG0_PIN, 1); sleep_ms(20);
    uart_set_baudrate(uart_, config::CH9120_DATA_BAUD); drain();
}

bool Ch9120::command(uint8_t code, const uint8_t* params, size_t count,
                     uint8_t* response, size_t response_len) {
    drain(); uart_write_blocking(uart_, PREFIX, 2); uart_putc_raw(uart_, code);
    if (count) uart_write_blocking(uart_, params, count);
    const size_t expected = response ? response_len : 1;
    for (size_t i = 0; i < expected; ++i) {
        const uint64_t until = time_us_64() + 200000;
        while (!uart_is_readable(uart_) && time_us_64() < until) tight_loop_contents();
        if (!uart_is_readable(uart_)) return false;
        const uint8_t value = uart_getc(uart_);
        if (response) response[i] = value;
        else if (value != 0xAA) return false;
    }
    return true;
}

bool Ch9120::configure() {
    enterConfig();
    const uint8_t server = 0;
    const uint8_t zero_address[] = {0, 0, 0, 0};
    const uint8_t port[] = {uint8_t(config::HTTP_PORT & 0xff), uint8_t(config::HTTP_PORT >> 8)};
    const uint32_t baud = config::CH9120_DATA_BAUD;
    const uint8_t baud_le[] = {uint8_t(baud), uint8_t(baud >> 8), uint8_t(baud >> 16), uint8_t(baud >> 24)};
    const uint8_t uart_format[] = {1, 4, 8}; // 1 stop, no parity, 8 data bits
    const uint8_t timeout[] = {1, 0, 0, 0}; // approximately 5 ms UART packet timeout
    const uint8_t enabled = 1;
    const uint8_t rx_length[] = {0, 2, 0, 0}; // 512-byte maximum packet
    // Clear a persisted previous DHCP lease before requesting a fresh one.
    bool ok = command(CMD_SET_MODE, &server, 1) &&
        command(CMD_SET_IP, zero_address, 4) && command(CMD_SET_MASK, zero_address, 4) &&
        command(CMD_SET_GATEWAY, zero_address, 4) && command(CMD_SET_PORT, port, 2) &&
        command(CMD_SET_BAUD, baud_le, 4) && command(CMD_SET_UART_FORMAT, uart_format, 3) &&
        command(CMD_SET_TIMEOUT, timeout, 4) && command(CMD_DISCONNECT_ON_LINK_LOSS, &enabled, 1) &&
        command(CMD_RX_LENGTH, rx_length, 4) && command(CMD_FLUSH_ON_CONNECT, &enabled, 1) &&
        command(CMD_DHCP, &enabled, 1) && command(CMD_SAVE, nullptr, 0) &&
        command(CMD_APPLY_AND_RESET, nullptr, 0);
    leaveConfig();
    sleep_ms(500);
    return ok;
}

void Ch9120::init(uint64_t now_ms) {
    uart_ = config::CH9120_UART;
    uart_init(uart_, config::CH9120_CONFIG_BAUD);
    gpio_set_function(config::CH9120_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(config::CH9120_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(uart_, 8, 1, UART_PARITY_NONE); uart_set_fifo_enabled(uart_, true);
    gpio_init(config::CH9120_CFG0_PIN); gpio_set_dir(config::CH9120_CFG0_PIN, GPIO_OUT); gpio_put(config::CH9120_CFG0_PIN, 1);
    gpio_init(config::CH9120_RSTI_PIN); gpio_set_dir(config::CH9120_RSTI_PIN, GPIO_OUT); gpio_put(config::CH9120_RSTI_PIN, 1);
    gpio_init(config::CH9120_TCPCS_PIN); gpio_set_dir(config::CH9120_TCPCS_PIN, GPIO_IN); gpio_pull_up(config::CH9120_TCPCS_PIN);
    printf("[ETH] initializing CH9120\n");
    gpio_put(config::CH9120_RSTI_PIN, 0); sleep_ms(20); gpio_put(config::CH9120_RSTI_PIN, 1); sleep_ms(250);
    if (!configure()) printf("[ETH] configuration failed; retrying\n");
    printf("[DHCP] requesting address\n"); state_ = State::WaitingDhcp; deadline_ms_ = now_ms + config::DHCP_TIMEOUT_MS;
}

bool Ch9120::queryAddress(uint8_t code, uint8_t address[4]) { return command(code, nullptr, 0, address, 4); }
bool Ch9120::queryNetwork() {
    enterConfig();
    const bool ok = queryAddress(CMD_GET_IP, info_.ip) && queryAddress(CMD_GET_MASK, info_.subnet) && queryAddress(CMD_GET_GATEWAY, info_.gateway);
    leaveConfig();
    const bool nonzero = info_.ip[0] || info_.ip[1] || info_.ip[2] || info_.ip[3];
    // CH9120 reports 10.10.10.10 after a cleared address while DHCP has no
    // lease (observed on the Waveshare module with Ethernet unplugged).
    const bool no_lease_sentinel = info_.ip[0] == 10 && info_.ip[1] == 10 &&
                                   info_.ip[2] == 10 && info_.ip[3] == 10;
    info_.connected = ok && nonzero && !no_lease_sentinel;
    return info_.connected;
}

void Ch9120::formatIp(const uint8_t ip[4], char* out, size_t size) const { snprintf(out, size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]); }

void Ch9120::task(uint64_t now_ms) {
    if (state_ == State::Ready) return;
    if (state_ == State::RetryPending) {
        if (now_ms < deadline_ms_) return;
        gpio_put(config::CH9120_RSTI_PIN, 0); sleep_ms(20); gpio_put(config::CH9120_RSTI_PIN, 1); sleep_ms(250);
        // DHCP and TCP-server settings persist in CH9120 EEPROM; avoid rewriting
        // flash on every router/cable outage.
        printf("[DHCP] retrying\n"); state_ = State::WaitingDhcp; deadline_ms_ = now_ms + config::DHCP_TIMEOUT_MS; return;
    }
    // Query every second; querying briefly pauses transparent TCP transport.
    static uint64_t next_query = 0;
    if (now_ms >= next_query) {
        next_query = now_ms + 1000;
        if (queryNetwork()) {
            char ip[16], gw[16], mask[16]; formatIp(info_.ip, ip, sizeof(ip)); formatIp(info_.gateway, gw, sizeof(gw)); formatIp(info_.subnet, mask, sizeof(mask));
            printf("[DHCP] IP: %s\n[DHCP] Gateway: %s\n[DHCP] Subnet: %s\n[HTTP] server started :%u\n", ip, gw, mask, config::HTTP_PORT);
            state_ = State::Ready; return;
        }
    }
    if (now_ms >= deadline_ms_) { printf("[DHCP] timeout; will retry\n"); info_.connected = false; state_ = State::RetryPending; deadline_ms_ = now_ms + config::DHCP_RETRY_MS; }
}

int Ch9120::read(uint8_t* data, size_t capacity) {
    size_t n = 0; while (n < capacity && uart_is_readable(uart_)) data[n++] = uart_getc(uart_); return static_cast<int>(n);
}
size_t Ch9120::write(const uint8_t* data, size_t length) { uart_write_blocking(uart_, data, length); return length; }
