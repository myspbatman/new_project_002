#include "api/rest_api.h"
#include "config/config.h"

#include <cstdio>
#include <cstring>
#include "pico/time.h"

static void ipText(const uint8_t ip[4], char out[16]) { snprintf(out, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]); }

size_t RestApi::handle(const char* method, const char* path, char* out, size_t cap,
                       const char*& type, int& status) {
    const Measurements m = battery_.snapshot(); type = "application/json"; status = 200; int n = 0;
    if (!strcmp(method, "GET") && !strcmp(path, "/api/v1/battery")) {
        if (m.sensor_ok) n = snprintf(out, cap, "{\"voltage\":%.3f,\"current\":%.3f,\"power\":%.3f,\"consumed_ah\":%.6f,\"consumed_mah\":%.3f,\"consumed_wh\":%.6f,\"sensor_ok\":true,\"timestamp_ms\":%llu}", m.voltage,m.current,m.power,m.consumed_ah,m.consumed_ah*1000,m.consumed_wh,(unsigned long long)m.timestamp_ms);
        else n = snprintf(out, cap, "{\"voltage\":0,\"current\":0,\"power\":0,\"consumed_ah\":%.6f,\"consumed_mah\":%.3f,\"consumed_wh\":%.6f,\"sensor_ok\":false,\"error\":\"%s\",\"timestamp_ms\":%llu}",m.consumed_ah,m.consumed_ah*1000,m.consumed_wh,m.error?m.error:"sensor_error",(unsigned long long)m.timestamp_ms);
    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/v1/battery/raw")) {
        n = snprintf(out, cap, "{\"bus_voltage_raw\":%ld,\"shunt_voltage_raw\":%ld,\"current_raw\":%ld,\"power_raw\":%lu,\"voltage\":%.6f,\"current\":%.6f,\"power\":%.6f,\"sensor_ok\":%s}",(long)m.raw.bus_voltage_raw,(long)m.raw.shunt_voltage_raw,(long)m.raw.current_raw,(unsigned long)m.raw.power_raw,m.raw_voltage,m.raw_current,m.raw_power,m.sensor_ok?"true":"false");
    } else if (!strcmp(method, "GET") && !strcmp(path, "/api/v1/status")) {
        char ip[16],gw[16],mask[16]; ipText(network_.info().ip,ip); ipText(network_.info().gateway,gw); ipText(network_.info().subnet,mask);
        n = snprintf(out, cap, "{\"device\":\"%s\",\"firmware\":\"%s\",\"uptime\":%llu,\"network\":{\"connected\":%s,\"dhcp\":true,\"ip\":\"%s\",\"gateway\":\"%s\",\"subnet\":\"%s\"},\"sensor\":{\"connected\":%s,\"type\":\"%s\",\"address\":\"0x%02X\",\"shunt_ohm\":%.7f}}",config::DEVICE_NAME,config::FIRMWARE_VERSION,(unsigned long long)(to_ms_since_boot(get_absolute_time())),network_.info().connected?"true":"false",ip,gw,mask,m.sensor_ok?"true":"false",battery_.sensor().typeName(),battery_.sensor().address(),config::SHUNT_RESISTANCE);
    } else if (!strcmp(method, "GET") && !strcmp(path, "/health")) {
        n = m.sensor_ok ? snprintf(out,cap,"{\"status\":\"ok\"}") : snprintf(out,cap,"{\"status\":\"degraded\",\"sensor\":\"offline\"}");
    } else if (!strcmp(method, "POST") && !strcmp(path, "/api/v1/battery/reset")) {
        battery_.resetCounters(); n = snprintf(out,cap,"{\"success\":true}");
    } else if (!strcmp(method, "GET") && !strcmp(path, "/")) {
        char ip[16]; ipText(network_.info().ip,ip);
        n = snprintf(out,cap,"{\"device\":\"%s\",\"firmware\":\"%s\",\"ip\":\"%s\",\"sensor\":{\"type\":\"%s\",\"ok\":%s},\"battery\":{\"voltage\":%.3f,\"current\":%.3f,\"power\":%.3f},\"timestamp_ms\":%llu}",config::DEVICE_NAME,config::FIRMWARE_VERSION,ip,battery_.sensor().typeName(),m.sensor_ok?"true":"false",m.voltage,m.current,m.power,(unsigned long long)m.timestamp_ms);
    } else { status = 404; n = snprintf(out,cap,"{\"error\":\"not_found\"}"); }
    if (n < 0) return 0; return static_cast<size_t>(n) < cap ? static_cast<size_t>(n) : cap - 1;
}
