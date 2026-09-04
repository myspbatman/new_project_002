# RP2040-ETH Battery Monitor

Production-oriented Raspberry Pi Pico SDK firmware for:

`MATEK I2C-INA-BM -> I2C -> Waveshare RP2040-ETH -> CH9120 -> HTTP`

The CH9120 is configured as a DHCP-enabled TCP server on port 80. It is a transparent UART/Ethernet bridge, so the firmware implements the HTTP/1.0/1.1 parser on the RP2040. No static IP is compiled into the firmware.

## On-board RGB status LED

The WS2812 RGB LED is driven by PIO on GP25:

- purple: firmware boot;
- blinking blue: waiting for DHCP/network;
- solid green: network ready and INA sensor healthy;
- blinking amber: network ready but INA sensor missing or invalid.

Brightness and pin are configurable with `STATUS_LED_BRIGHTNESS` and
`STATUS_LED_PIN` in `src/config/config.h`.

## Hardware and wiring

### Required components

- Waveshare RP2040-ETH board (RP2040 + CH9120 Ethernet controller);
- MATEK I2C-INA-BM module with INA228 or INA238 and 200 µΩ shunt;
- regulated 5 V supply for the MATEK module (its `5V` input accepts 4–9 V);
- USB-C cable or another suitable supply for RP2040-ETH;
- Ethernet cable and a router with an enabled DHCP server;
- four-wire signal cable for `5V`, `GND`, `SDA`, and `SCL`;
- battery/load cables sized for the actual current, correctly rated terminals,
  and an appropriately rated fuse close to the battery source;
- optional 2.2–4.7 kΩ I2C pull-ups to 3.3 V if the installed module/cable does
  not already provide suitable pull-ups.

### Signal connection

```text
MATEK I2C-INA-BM                  Waveshare RP2040-ETH
----------------                  --------------------
SDA  -------------------------->  GP4
SCL  -------------------------->  GP5
GND  -------------------------->  GND
5V   -------------------------->  regulated 4–9 V supply
                                   (5 V recommended)

Ethernet router  -------------->  RJ45
USB-C / supply   -------------->  RP2040-ETH power
```

Always join the MATEK and RP2040 grounds. Do not connect the MATEK `5V` input
to RP2040 `3V3`.

### High-current path

Route the battery/load positive conductor through the two large current-path
pads of the MATEK module, following the direction and polarity printed on the
module. Do not carry load current through the JST/I2C connector. Keep the
high-current connections short and mechanically secure. The module is rated
for up to 150 A continuous and 204.8 A burst, but the safe installation limit
also depends on cable gauge, terminals, cooling, PCB mounting, and fuse rating.
Disconnect battery power while assembling or changing the wiring.

The Waveshare board internally reserves these pins:

| CH9120 | RP2040 | Purpose |
|---|---:|---|
| RXD | GP21 | UART data into CH9120 |
| TXD | GP20 | UART data from CH9120 |
| TCPCS | GP17 | TCP-client status (not meaningful in server mode) |
| CFG0 | GP18 | active-low UART configuration mode |
| RSTI | GP19 | active-low reset |

Connect the MATEK module as follows:

| MATEK I2C-INA-BM | RP2040-ETH |
|---|---|
| SDA | GP4 |
| SCL | GP5 |
| GND | GND |
| 5V | an appropriate 5 V supply |

The MATEK board specifies **4–9 V at its 5V pin**. Do not power that pin from RP2040 3V3. Both boards must share ground. Connect battery/load current paths according to the MATEK markings, observing polarity and high-current wiring practice. External I2C pull-ups may be required if the module/cable does not provide them.

Sensor addresses are scanned in this order: `0x45`, `0x44`, `0x41`. The TI manufacturer/device-ID registers distinguish INA228 from INA238. Configuration defaults are in `src/config/config.h`.

## Measurement implementation

The driver uses TI's device-specific register widths and equations. With a 200 µΩ shunt and 204.8 A maximum:

- INA228: `Current_LSB = 204.8 / 2^19 = 390.625 µA`, `SHUNT_CAL = 1024`.
- INA238: `Current_LSB = 204.8 / 2^15 = 6.25 mA`, `SHUNT_CAL = 1024`.
- `ADCRANGE=0`; the 40.96 mV maximum expected shunt drop is inside the ±163.84 mV range.

Samples are cached at 20 Hz. EMA filtering defaults to alpha 0.2. Ah and Wh use actual elapsed microseconds and are not integrated across a long sensor outage.

## Build and flash

Install the Arm embedded toolchain, CMake, and Raspberry Pi Pico SDK, then:

```sh
cd /home/mysp/rp2040-eth-battery-monitor
cmake -S . -B build -DPICO_SDK_PATH=/absolute/path/to/pico-sdk
cmake --build build -j
```

The result is `build/rp2040_eth_battery_monitor.uf2`. Hold BOOT while resetting/powering the board, then copy the UF2 to the mounted `RPI-RP2` volume.

USB serial debug output is enabled. The baud value shown by terminal applications can be set to 115200 (USB CDC itself is not baud-rate dependent).

## DHCP and CH9120 behavior

At boot, firmware uses the documented hardware CFG0 method and 9600-baud configuration protocol to select TCP Server, local port 80, 921600-baud data UART, and DHCP. Configuration is saved to CH9120 EEPROM. It then polls the documented current IP/mask/gateway registers for up to 15 seconds and retries without falling back to a static address.

Before requesting DHCP, stale address fields are cleared. On the tested Waveshare module CH9120 reports `10.10.10.10` as a no-lease fallback when Ethernet is unplugged; firmware explicitly treats that value as unassigned rather than announcing a false DHCP success.

CH9120 exposes one transparent TCP stream, not multiple hardware sockets. Requests are processed sequentially. Responses include `Connection: close` and `Content-Length`; the HTTP client is expected to close the TCP connection after consuming the response. TLS, chunked encoding, keep-alive pipelining, and concurrent clients are intentionally unsupported.

## REST API

All endpoints return UTF-8 JSON with `Content-Type: application/json`.
Measurements are JSON numbers, flags are JSON booleans, and timestamps are
unsigned milliseconds since boot. Invalid measurements are returned as zero
with `sensor_ok: false`; the API never emits `NaN` or `Infinity`.

```sh
curl http://DEVICE_IP/api/v1/battery
curl http://DEVICE_IP/api/v1/battery/raw
curl http://DEVICE_IP/api/v1/status
curl http://DEVICE_IP/health
curl http://DEVICE_IP/
curl -X POST http://DEVICE_IP/api/v1/battery/reset
```

### `GET /`

```json
{
  "device": "RP2040-ETH-BATTERY-MONITOR",
  "firmware": "1.0.0",
  "ip": "192.168.31.68",
  "sensor": {"type": "INA228", "ok": true},
  "battery": {"voltage": 48.72, "current": 12.43, "power": 605.59},
  "timestamp_ms": 12345678
}
```

### `GET /api/v1/battery`

```json
{
  "voltage": 48.72,
  "current": 12.43,
  "power": 605.59,
  "consumed_ah": 1.42,
  "consumed_mah": 1420.0,
  "consumed_wh": 68.4,
  "sensor_ok": true,
  "timestamp_ms": 12345678
}
```

If the sensor is unavailable, `sensor_ok` is `false` and the response also
contains an `error` string such as `sensor_not_found` or `i2c_timeout`.

### `GET /api/v1/battery/raw`

Returns signed raw bus, shunt and current register values; the unsigned raw
power register; converted voltage, current and power; and `sensor_ok`.

### `GET /api/v1/status`

Returns device identity and uptime plus nested `network` and `sensor` objects.

### `GET /health`

Healthy response:

```json
{"status":"ok"}
```

Sensor-offline response:

```json
{"status":"degraded","sensor":"offline"}
```

### `POST /api/v1/battery/reset`

Resets Ah, mAh and Wh counters and returns:

```json
{"success":true}
```

Every response has a correct `Content-Length`, and CORS is enabled with
`Access-Control-Allow-Origin: *`. Ethernet and HTTP continue operating when
the sensor is unavailable.

## Primary references

- [Waveshare RP2040-ETH wiki](https://www.waveshare.com/wiki/RP2040-ETH)
- [CH9120 datasheet](https://files.waveshare.com/wiki/common/CH9120DS1_EN.pdf)
- [CH9120 serial command set](https://files.waveshare.com/upload/e/e1/CH9120_Serial_Commands_Set.pdf)
- [MATEK I2C-INA-BM](https://www.mateksys.com/?portfolio=i2c-ina-bm)
- [TI INA228 datasheet](https://www.ti.com/lit/ds/symlink/ina228.pdf)
- [TI INA238 datasheet](https://www.ti.com/lit/ds/symlink/ina238.pdf)
