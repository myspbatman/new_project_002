#include "utils/watchdog.h"
#include "config/config.h"
#include "hardware/watchdog.h"
void app_watchdog::init() { watchdog_enable(config::WATCHDOG_TIMEOUT_MS, true); }
void app_watchdog::feed() { watchdog_update(); }
