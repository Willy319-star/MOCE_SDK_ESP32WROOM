#include "service_wifi.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "service_wifi";

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif;
static bool s_netif_inited;
static bool s_wifi_inited;
static bool s_connected;
static uint8_t s_retry_count;
static uint8_t s_max_retry;

static esp_err_t errno_to_esp_err(void)
{
    return errno == EAGAIN || errno == EWOULDBLOCK ? ESP_ERR_TIMEOUT : ESP_FAIL;
}

static esp_err_t set_socket_timeout(int fd, int timeout_ms, bool recv_timeout)
{
    if (timeout_ms < 0) {
        return ESP_OK;
    }

    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };
    int opt = recv_timeout ? SO_RCVTIMEO : SO_SNDTIMEO;
    return setsockopt(fd, SOL_SOCKET, opt, &tv, sizeof(tv)) == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t resolve_ipv4(const char *host, uint16_t port, struct sockaddr_in *out_addr)
{
    if (host == NULL || out_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *result = NULL;
    char port_text[8];
    snprintf(port_text, sizeof(port_text), "%u", port);

    int err = getaddrinfo(host, port_text, &hints, &result);
    if (err != 0 || result == NULL) {
        ESP_LOGE(TAG, "resolve %s:%u failed: %d", host, port, err);
        return ESP_ERR_NOT_FOUND;
    }

    memcpy(out_addr, result->ai_addr, sizeof(*out_addr));
    freeaddrinfo(result);
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_retry_count < s_max_retry) {
            s_retry_count++;
            esp_wifi_connect();
            ESP_LOGW(TAG, "retry WiFi connection %u/%u", s_retry_count, s_max_retry);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_retry_count = 0;
        s_connected = true;
        ESP_LOGI(TAG, "got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase failed");
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t service_wifi_init_sta(const service_wifi_sta_config_t *config)
{
    if (config == NULL || config->ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_wifi_inited) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(init_nvs(), TAG, "nvs init failed");

    if (!s_netif_inited) {
        ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
        ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop init failed");
        s_netif_inited = true;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "wifi init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL),
                        TAG,
                        "wifi event register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL),
                        TAG,
                        "ip event register failed");

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = config->password[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    s_max_retry = config->max_retry > 0 ? config->max_retry : 5;
    s_retry_count = 0;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "wifi set config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");

    s_wifi_inited = true;
    ESP_LOGI(TAG, "WiFi STA started, ssid=%s", config->ssid);

    uint32_t timeout_ms = config->connect_timeout_ms > 0 ? config->connect_timeout_ms : 15000;
    return service_wifi_wait_connected(timeout_ms);
}

esp_err_t service_wifi_deinit(void)
{
    if (!s_wifi_inited) {
        return ESP_OK;
    }

    esp_wifi_stop();
    esp_wifi_deinit();

    if (s_sta_netif != NULL) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
    if (s_wifi_event_group != NULL) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    s_wifi_inited = false;
    s_connected = false;
    s_retry_count = 0;
    return ESP_OK;
}

esp_err_t service_wifi_disconnect(void)
{
    if (!s_wifi_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    s_connected = false;
    return esp_wifi_disconnect();
}

bool service_wifi_is_connected(void)
{
    return s_connected;
}

esp_err_t service_wifi_wait_connected(uint32_t timeout_ms)
{
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));
    if ((bits & WIFI_CONNECTED_BIT) != 0) {
        return ESP_OK;
    }
    return (bits & WIFI_FAIL_BIT) != 0 ? ESP_FAIL : ESP_ERR_TIMEOUT;
}

esp_err_t service_wifi_get_ip_info(service_wifi_ip_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_sta_netif == NULL || !s_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info;
    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(s_sta_netif, &ip_info), TAG, "get ip failed");

    snprintf(info->ip, sizeof(info->ip), IPSTR, IP2STR(&ip_info.ip));
    snprintf(info->netmask, sizeof(info->netmask), IPSTR, IP2STR(&ip_info.netmask));
    snprintf(info->gateway, sizeof(info->gateway), IPSTR, IP2STR(&ip_info.gw));
    return ESP_OK;
}

esp_err_t service_wifi_tcp_client_connect(const char *host,
                                          uint16_t port,
                                          uint32_t timeout_ms,
                                          service_wifi_tcp_client_t *client)
{
    if (client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(client, 0, sizeof(*client));
    client->fd = -1;

    struct sockaddr_in dest_addr;
    ESP_RETURN_ON_ERROR(resolve_ipv4(host, port, &dest_addr), TAG, "resolve failed");

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        return ESP_FAIL;
    }

    set_socket_timeout(fd, (int)timeout_ms, true);
    set_socket_timeout(fd, (int)timeout_ms, false);

    if (connect(fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        close(fd);
        return errno_to_esp_err();
    }

    client->fd = fd;
    return ESP_OK;
}

esp_err_t service_wifi_tcp_client_send(service_wifi_tcp_client_t *client, const void *data, size_t len)
{
    if (client == NULL || client->fd < 0 || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int sent = send(client->fd, data, len, 0);
    return sent == (int)len ? ESP_OK : errno_to_esp_err();
}

esp_err_t service_wifi_tcp_client_recv(service_wifi_tcp_client_t *client,
                                       void *buffer,
                                       size_t len,
                                       int timeout_ms,
                                       int *out_len)
{
    if (client == NULL || client->fd < 0 || buffer == NULL || len == 0 || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(set_socket_timeout(client->fd, timeout_ms, true), TAG, "set timeout failed");
    int received = recv(client->fd, buffer, len, 0);
    if (received < 0) {
        *out_len = 0;
        return errno_to_esp_err();
    }

    *out_len = received;
    return received == 0 ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
}

esp_err_t service_wifi_tcp_client_close(service_wifi_tcp_client_t *client)
{
    if (client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (client->fd >= 0) {
        shutdown(client->fd, SHUT_RDWR);
        close(client->fd);
        client->fd = -1;
    }
    return ESP_OK;
}

esp_err_t service_wifi_tcp_server_start(uint16_t port, service_wifi_tcp_server_t *server)
{
    if (server == NULL || port == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(server, 0, sizeof(*server));
    server->listen_fd = -1;

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        return ESP_FAIL;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(port),
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(fd, SERVICE_WIFI_TCP_BACKLOG) != 0) {
        close(fd);
        return errno_to_esp_err();
    }

    server->listen_fd = fd;
    server->port = port;
    return ESP_OK;
}

esp_err_t service_wifi_tcp_server_accept(service_wifi_tcp_server_t *server, int timeout_ms, int *out_client_fd)
{
    if (server == NULL || server->listen_fd < 0 || out_client_fd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(set_socket_timeout(server->listen_fd, timeout_ms, true), TAG, "set timeout failed");

    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int fd = accept(server->listen_fd, (struct sockaddr *)&source_addr, &addr_len);
    if (fd < 0) {
        *out_client_fd = -1;
        return errno_to_esp_err();
    }

    *out_client_fd = fd;
    return ESP_OK;
}

esp_err_t service_wifi_tcp_server_send(int client_fd, const void *data, size_t len)
{
    if (client_fd < 0 || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int sent = send(client_fd, data, len, 0);
    return sent == (int)len ? ESP_OK : errno_to_esp_err();
}

esp_err_t service_wifi_tcp_server_recv(int client_fd, void *buffer, size_t len, int timeout_ms, int *out_len)
{
    if (client_fd < 0 || buffer == NULL || len == 0 || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(set_socket_timeout(client_fd, timeout_ms, true), TAG, "set timeout failed");
    int received = recv(client_fd, buffer, len, 0);
    if (received < 0) {
        *out_len = 0;
        return errno_to_esp_err();
    }

    *out_len = received;
    return received == 0 ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
}

esp_err_t service_wifi_tcp_server_close_client(int client_fd)
{
    if (client_fd < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    return ESP_OK;
}

esp_err_t service_wifi_tcp_server_stop(service_wifi_tcp_server_t *server)
{
    if (server == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (server->listen_fd >= 0) {
        shutdown(server->listen_fd, SHUT_RDWR);
        close(server->listen_fd);
        server->listen_fd = -1;
    }
    return ESP_OK;
}

esp_err_t service_wifi_udp_open(uint16_t local_port, service_wifi_udp_t *udp)
{
    if (udp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(udp, 0, sizeof(*udp));
    udp->fd = -1;

    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) {
        return ESP_FAIL;
    }

    if (local_port > 0) {
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = htonl(INADDR_ANY),
            .sin_port = htons(local_port),
        };
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            close(fd);
            return errno_to_esp_err();
        }
    }

    udp->fd = fd;
    udp->local_port = local_port;
    return ESP_OK;
}

esp_err_t service_wifi_udp_send_to(service_wifi_udp_t *udp, const char *host, uint16_t port, const void *data, size_t len)
{
    if (udp == NULL || udp->fd < 0 || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sockaddr_in dest_addr;
    ESP_RETURN_ON_ERROR(resolve_ipv4(host, port, &dest_addr), TAG, "resolve failed");

    int sent = sendto(udp->fd, data, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    return sent == (int)len ? ESP_OK : errno_to_esp_err();
}

esp_err_t service_wifi_udp_recv_from(service_wifi_udp_t *udp,
                                     void *buffer,
                                     size_t len,
                                     int timeout_ms,
                                     char *remote_ip,
                                     size_t remote_ip_len,
                                     uint16_t *remote_port,
                                     int *out_len)
{
    if (udp == NULL || udp->fd < 0 || buffer == NULL || len == 0 || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(set_socket_timeout(udp->fd, timeout_ms, true), TAG, "set timeout failed");

    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int received = recvfrom(udp->fd, buffer, len, 0, (struct sockaddr *)&source_addr, &addr_len);
    if (received < 0) {
        *out_len = 0;
        return errno_to_esp_err();
    }

    if (remote_ip != NULL && remote_ip_len > 0) {
        inet_ntop(AF_INET, &source_addr.sin_addr, remote_ip, remote_ip_len);
    }
    if (remote_port != NULL) {
        *remote_port = ntohs(source_addr.sin_port);
    }
    *out_len = received;
    return ESP_OK;
}

esp_err_t service_wifi_udp_close(service_wifi_udp_t *udp)
{
    if (udp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (udp->fd >= 0) {
        close(udp->fd);
        udp->fd = -1;
    }
    return ESP_OK;
}
