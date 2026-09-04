#include "led/status_led.h"
#include "config/config.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "pico/time.h"

namespace {
PIO led_pio = pio0;
uint8_t scale(uint8_t value) {
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * config::STATUS_LED_BRIGHTNESS) / 255u);
}
}

void StatusLed::init() {
    offset_ = pio_add_program(led_pio, &ws2812_program);
    sm_ = pio_claim_unused_sm(led_pio, true);
    ws2812_program_init(led_pio, sm_, offset_, config::STATUS_LED_PIN, 800000.0f);
    set(80, 0, 160);
    sleep_us(80);
}

void StatusLed::set(uint8_t red, uint8_t green, uint8_t blue) {
    const uint32_t grb = (uint32_t(scale(green)) << 16) |
                         (uint32_t(scale(red)) << 8) | scale(blue);
    pio_sm_put_blocking(led_pio, sm_, grb << 8);
}

void StatusLed::task(uint64_t now_ms, bool network_ready, bool sensor_ok) {
    if (now_ms < next_update_ms_) return;
    phase_ = !phase_;
    if (!network_ready) {
        next_update_ms_ = now_ms + 500;
        phase_ ? set(0, 40, 255) : set(0, 0, 0);
    } else if (!sensor_ok) {
        next_update_ms_ = now_ms + 500;
        phase_ ? set(255, 80, 0) : set(20, 5, 0);
    } else {
        next_update_ms_ = now_ms + 1000;
        set(0, 255, 20);
    }
}
