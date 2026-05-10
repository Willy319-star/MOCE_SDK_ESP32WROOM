# WiFi TCP UDP Demo

This example tests the `service_wifi` component in STA mode.

Configure WiFi directly in `main/main.c` before building/flashing:

```c
#define WIFI_DEMO_SSID "your_ssid"
#define WIFI_DEMO_PASSWORD "your_password"
```

The app starts TCP/UDP tests after WiFi connects. If WiFi connection fails, update the two macros above and rebuild.

Optional build-time configuration:

```sh
cd examples/wifi_tcp_udp_demo
idf.py menuconfig
```

Set:

- Demo mode: TCP client, TCP server, UDP client, or UDP server
- Remote host/port or local listen port

Build for ESP32-S3:

```sh
./tools/build.sh examples/wifi_tcp_udp_demo esp32s3 my_board_esp32s3
```

Flash:

```sh
./tools/flash.sh examples/wifi_tcp_udp_demo /dev/ttyACM0
```

Simple host-side tests:

```sh
# TCP server on PC for ESP TCP client mode
nc -lk 9000

# TCP client on PC for ESP TCP server mode
nc <esp_ip> 9000

# UDP server on PC for ESP UDP client mode
nc -luk 9000

# UDP client on PC for ESP UDP server mode
echo hello | nc -u <esp_ip> 9000
```
