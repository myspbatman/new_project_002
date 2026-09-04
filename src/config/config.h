#pragma once

#include "hardware/i2c.h"
#include "hardware/uart.h"

namespace config {
inline constexpr char DEVICE_NAME[] = "RP2040-ETH-BATTERY-MONITOR";
inline constexpr char FIRMWARE_VERSION[] = "1.0.0";

inline constexpr uint HTTP_PORT = 80;

inline constexpr uint I2C_SDA_PIN = 4;
inline constexpr uint I2C_SCL_PIN = 5;
inline constexpr uint32_t I2C_FREQUENCY = 400000;
inline i2c_inst_t* const I2C_PORT = i2c0;
inline constexpr uint8_t INA_ADDRESSES[] = {0x45, 0x44, 0x41};

inline constexpr double SHUNT_RESISTANCE = 0.0002;
inline constexpr double MAX_CURRENT = 204.8;
inline constexpr uint32_t SENSOR_SAMPLE_RATE_HZ = 20;
inline constexpr float EMA_ALPHA = 0.2f;
inline constexpr uint32_t SENSOR_RESCAN_MS = 2000;

inline uart_inst_t* const CH9120_UART = uart1;
inline constexpr uint CH9120_TX_PIN = 20; // RP2040 TX -> CH9120 RXD
inline constexpr uint CH9120_RX_PIN = 21; // RP2040 RX <- CH9120 TXD
inline constexpr uint CH9120_TCPCS_PIN = 17;
inline constexpr uint CH9120_CFG0_PIN = 18;
inline constexpr uint CH9120_RSTI_PIN = 19;
inline constexpr uint32_t CH9120_CONFIG_BAUD = 9600;
inline constexpr uint32_t CH9120_DATA_BAUD = 921600;
inline constexpr uint32_t DHCP_TIMEOUT_MS = 15000;
inline constexpr uint32_t DHCP_RETRY_MS = 5000;

inline constexpr uint32_t WATCHDOG_TIMEOUT_MS = 5000;
inline constexpr uint STATUS_LED_PIN = 25;
inline constexpr uint8_t STATUS_LED_BRIGHTNESS = 24; // 0..255
inline constexpr size_t HTTP_MAX_REQUEST = 1024;
inline constexpr uint32_t HTTP_IDLE_TIMEOUT_MS = 2000;
} // namespace config
