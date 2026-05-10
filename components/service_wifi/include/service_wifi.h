#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_netif_ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SERVICE_WIFI_MAX_SSID_LEN      32
#define SERVICE_WIFI_MAX_PASSWORD_LEN  64
#define SERVICE_WIFI_TCP_BACKLOG       4

typedef struct {
    char ssid[SERVICE_WIFI_MAX_SSID_LEN + 1];
    char password[SERVICE_WIFI_MAX_PASSWORD_LEN + 1];
    uint32_t connect_timeout_ms;
    uint8_t max_retry;
} service_wifi_sta_config_t;

typedef struct {
    char ip[16];
    char netmask[16];
    char gateway[16];
} service_wifi_ip_info_t;

typedef struct {
    int fd;
} service_wifi_tcp_client_t;

typedef struct {
    int listen_fd;
    uint16_t port;
} service_wifi_tcp_server_t;

typedef struct {
    int fd;
    uint16_t local_port;
} service_wifi_udp_t;

esp_err_t service_wifi_init_sta(const service_wifi_sta_config_t *config);
esp_err_t service_wifi_deinit(void);
esp_err_t service_wifi_disconnect(void);
bool service_wifi_is_connected(void);
esp_err_t service_wifi_wait_connected(uint32_t timeout_ms);
esp_err_t service_wifi_get_ip_info(service_wifi_ip_info_t *info);

esp_err_t service_wifi_tcp_client_connect(const char *host,
                                          uint16_t port,
                                          uint32_t timeout_ms,
                                          service_wifi_tcp_client_t *client);
esp_err_t service_wifi_tcp_client_send(service_wifi_tcp_client_t *client, const void *data, size_t len);
esp_err_t service_wifi_tcp_client_recv(service_wifi_tcp_client_t *client,
                                       void *buffer,
                                       size_t len,
                                       int timeout_ms,
                                       int *out_len);
esp_err_t service_wifi_tcp_client_close(service_wifi_tcp_client_t *client);

esp_err_t service_wifi_tcp_server_start(uint16_t port, service_wifi_tcp_server_t *server);
esp_err_t service_wifi_tcp_server_accept(service_wifi_tcp_server_t *server, int timeout_ms, int *out_client_fd);
esp_err_t service_wifi_tcp_server_send(int client_fd, const void *data, size_t len);
esp_err_t service_wifi_tcp_server_recv(int client_fd, void *buffer, size_t len, int timeout_ms, int *out_len);
esp_err_t service_wifi_tcp_server_close_client(int client_fd);
esp_err_t service_wifi_tcp_server_stop(service_wifi_tcp_server_t *server);

esp_err_t service_wifi_udp_open(uint16_t local_port, service_wifi_udp_t *udp);
esp_err_t service_wifi_udp_send_to(service_wifi_udp_t *udp, const char *host, uint16_t port, const void *data, size_t len);
esp_err_t service_wifi_udp_recv_from(service_wifi_udp_t *udp,
                                     void *buffer,
                                     size_t len,
                                     int timeout_ms,
                                     char *remote_ip,
                                     size_t remote_ip_len,
                                     uint16_t *remote_port,
                                     int *out_len);
esp_err_t service_wifi_udp_close(service_wifi_udp_t *udp);

#ifdef __cplusplus
}
#endif
