#include <cstdio>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "config/config.h"
#include "battery/battery_monitor.h"
#include "network/ch9120.h"
#include "api/rest_api.h"
#include "http_server.h"
#include "led/status_led.h"
#include "utils/watchdog.h"

int main() {
    stdio_init_all(); sleep_ms(1200);
    printf("[BOOT] RP2040 Battery Monitor\n[BOOT] Firmware %s\n", config::FIRMWARE_VERSION);
    StatusLed status_led; status_led.init();
    BatteryMonitor battery; battery.init();
    Ch9120 network; network.init(to_ms_since_boot(get_absolute_time()));
    RestApi api(battery, network); HttpServer http(network, api);
    app_watchdog::init();
    uint64_t next_log = 0;
    while (true) {
        const uint64_t now = to_ms_since_boot(get_absolute_time());
        battery.task(now); network.task(now); http.task(now);
        status_led.task(now, network.ready(), battery.snapshot().sensor_ok);
        if (now >= next_log) {
            next_log = now + 1000; const auto m = battery.snapshot();
            const auto& ni = network.info();
            printf("[STATUS] sensor=%s V=%.3f A=%.3f W=%.3f SOC=%.1f%% network=%s ip=%u.%u.%u.%u\n",
                   m.sensor_ok?"ok":"offline",m.voltage,m.current,m.power,m.soc_percent,network.ready()?"ready":"dhcp",
                   ni.ip[0],ni.ip[1],ni.ip[2],ni.ip[3]);
        }
        app_watchdog::feed(); tight_loop_contents();
    }
}
