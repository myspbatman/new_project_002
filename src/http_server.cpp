#include "http_server.h"
#include "config/config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

void HttpServer::reset() { used_ = 0; request_[0] = '\0'; }

void HttpServer::task(uint64_t now_ms) {
    if (!transport_.ready()) { reset(); return; }
    uint8_t chunk[128]; const int n = transport_.read(chunk, sizeof(chunk));
    if (n > 0) {
        last_byte_ms_ = now_ms;
        const size_t room = config::HTTP_MAX_REQUEST - used_;
        if (static_cast<size_t>(n) > room) {
            static constexpr char response[] = "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
            transport_.write(reinterpret_cast<const uint8_t*>(response), sizeof(response)-1); reset(); return;
        }
        memcpy(request_ + used_, chunk, n); used_ += n; request_[used_] = '\0'; process();
    } else if (used_ && now_ms - last_byte_ms_ > config::HTTP_IDLE_TIMEOUT_MS) reset();
}

void HttpServer::process() {
    char* headers_end = strstr(request_, "\r\n\r\n"); if (!headers_end) return;
    char method[8] = {}, path[128] = {}, version[16] = {};
    if (sscanf(request_, "%7s %127s %15s", method, path, version) != 3 ||
        (strcmp(method,"GET") && strcmp(method,"POST")) ||
        (strcmp(version,"HTTP/1.0") && strcmp(version,"HTTP/1.1"))) {
        static constexpr char bad[] = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        transport_.write(reinterpret_cast<const uint8_t*>(bad), sizeof(bad)-1); reset(); return;
    }
    size_t content_length = 0; const char* p = request_;
    while ((p = strstr(p, "\r\n")) && p < headers_end) {
        p += 2;
        if (!strncasecmp(p, "Content-Length:", 15)) content_length = strtoul(p + 15, nullptr, 10);
    }
    const size_t header_bytes = (headers_end + 4) - request_;
    if (used_ < header_bytes + content_length) return;

    char body[1400]; const char* content_type; int status;
    const size_t body_len = api_.handle(method, path, body, sizeof(body), content_type, status);
    const char* reason = status == 200 ? "OK" : status == 404 ? "Not Found" : "Error";
    char header[320]; const int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        status, reason, content_type, static_cast<unsigned>(body_len));
    transport_.write(reinterpret_cast<const uint8_t*>(header), header_len);
    transport_.write(reinterpret_cast<const uint8_t*>(body), body_len);
    reset();
}
