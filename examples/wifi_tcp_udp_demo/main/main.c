#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "service_wifi.h"

static const char *TAG = "wifi_tcp_udp_demo";

/* Configure WiFi credentials here before building/flashing. */
#define WIFI_DEMO_SSID "MOCE-2.4G"
#define WIFI_DEMO_PASSWORD "moceai88888"

static esp_err_t read_wifi_config(service_wifi_sta_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(*config));
    config->connect_timeout_ms = 15000;
    config->max_retry = 2;
    strlcpy(config->ssid, WIFI_DEMO_SSID, sizeof(config->ssid));
    strlcpy(config->password, WIFI_DEMO_PASSWORD, sizeof(config->password));

    if (config->ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void start_wifi(void)
{
    service_wifi_sta_config_t config;
    ESP_ERROR_CHECK(read_wifi_config(&config));

    ESP_LOGI(TAG, "connecting to SSID: %s", config.ssid);
    esp_err_t err = service_wifi_init_sta(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connection failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "please update WIFI_DEMO_SSID/WIFI_DEMO_PASSWORD in main.c");
        ESP_ERROR_CHECK(err);
    }

    service_wifi_ip_info_t ip_info;
    ESP_ERROR_CHECK(service_wifi_get_ip_info(&ip_info));
    ESP_LOGI(TAG, "connected: ip=%s netmask=%s gateway=%s", ip_info.ip, ip_info.netmask, ip_info.gateway);
}

#if CONFIG_WIFI_TCP_UDP_DEMO_TCP_CLIENT
static void run_tcp_client(void)
{
    service_wifi_tcp_client_t client;
    ESP_ERROR_CHECK(service_wifi_tcp_client_connect(CONFIG_WIFI_TCP_UDP_DEMO_REMOTE_HOST,
                                                    CONFIG_WIFI_TCP_UDP_DEMO_REMOTE_PORT,
                                                    5000,
                                                    &client));

    const char *message = CONFIG_WIFI_TCP_UDP_DEMO_MESSAGE;
    ESP_ERROR_CHECK(service_wifi_tcp_client_send(&client, message, strlen(message)));
    ESP_LOGI(TAG, "TCP client sent: %s", message);

    char buffer[128];
    int len = 0;
    esp_err_t err = service_wifi_tcp_client_recv(&client, buffer, sizeof(buffer) - 1, 3000, &len);
    if (err == ESP_OK) {
        buffer[len] = '\0';
        ESP_LOGI(TAG, "TCP client received: %s", buffer);
    } else {
        ESP_LOGW(TAG, "TCP client recv skipped: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(service_wifi_tcp_client_close(&client));
}
#endif

#if CONFIG_WIFI_TCP_UDP_DEMO_TCP_SERVER
static void run_tcp_server(void)
{
    service_wifi_tcp_server_t server;
    ESP_ERROR_CHECK(service_wifi_tcp_server_start(CONFIG_WIFI_TCP_UDP_DEMO_LOCAL_PORT, &server));
    ESP_LOGI(TAG, "TCP server listening on port %d", CONFIG_WIFI_TCP_UDP_DEMO_LOCAL_PORT);

    while (1) {
        int client_fd = -1;
        esp_err_t err = service_wifi_tcp_server_accept(&server, 10000, &client_fd);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "TCP accept timeout/error: %s", esp_err_to_name(err));
            continue;
        }

        char buffer[128];
        int len = 0;
        err = service_wifi_tcp_server_recv(client_fd, buffer, sizeof(buffer) - 1, 5000, &len);
        if (err == ESP_OK) {
            buffer[len] = '\0';
            ESP_LOGI(TAG, "TCP server received: %s", buffer);
            service_wifi_tcp_server_send(client_fd, buffer, len);
        }
        service_wifi_tcp_server_close_client(client_fd);
    }
}
#endif

#if CONFIG_WIFI_TCP_UDP_DEMO_UDP_CLIENT
static void run_udp_client(void)
{
    service_wifi_udp_t udp;
    ESP_ERROR_CHECK(service_wifi_udp_open(0, &udp));

    const char *message = CONFIG_WIFI_TCP_UDP_DEMO_MESSAGE;
    ESP_ERROR_CHECK(service_wifi_udp_send_to(&udp,
                                             CONFIG_WIFI_TCP_UDP_DEMO_REMOTE_HOST,
                                             CONFIG_WIFI_TCP_UDP_DEMO_REMOTE_PORT,
                                             message,
                                             strlen(message)));
    ESP_LOGI(TAG, "UDP client sent: %s", message);

    char buffer[128];
    char remote_ip[16];
    uint16_t remote_port = 0;
    int len = 0;
    esp_err_t err = service_wifi_udp_recv_from(&udp, buffer, sizeof(buffer) - 1, 3000, remote_ip, sizeof(remote_ip), &remote_port, &len);
    if (err == ESP_OK) {
        buffer[len] = '\0';
        ESP_LOGI(TAG, "UDP client received from %s:%u: %s", remote_ip, remote_port, buffer);
    } else {
        ESP_LOGW(TAG, "UDP client recv skipped: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(service_wifi_udp_close(&udp));
}
#endif

#if CONFIG_WIFI_TCP_UDP_DEMO_UDP_SERVER
static void run_udp_server(void)
{
    service_wifi_udp_t udp;
    ESP_ERROR_CHECK(service_wifi_udp_open(CONFIG_WIFI_TCP_UDP_DEMO_LOCAL_PORT, &udp));
    ESP_LOGI(TAG, "UDP server listening on port %d", CONFIG_WIFI_TCP_UDP_DEMO_LOCAL_PORT);

    while (1) {
        char buffer[128];
        char remote_ip[16];
        uint16_t remote_port = 0;
        int len = 0;
        esp_err_t err = service_wifi_udp_recv_from(&udp, buffer, sizeof(buffer) - 1, 10000, remote_ip, sizeof(remote_ip), &remote_port, &len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "UDP recv timeout/error: %s", esp_err_to_name(err));
            continue;
        }

        buffer[len] = '\0';
        ESP_LOGI(TAG, "UDP server received from %s:%u: %s", remote_ip, remote_port, buffer);
        service_wifi_udp_send_to(&udp, remote_ip, remote_port, buffer, len);
    }
}
#endif

void app_main(void)
{
    start_wifi();

#if CONFIG_WIFI_TCP_UDP_DEMO_TCP_CLIENT
    run_tcp_client();
#elif CONFIG_WIFI_TCP_UDP_DEMO_TCP_SERVER
    run_tcp_server();
#elif CONFIG_WIFI_TCP_UDP_DEMO_UDP_CLIENT
    run_udp_client();
#elif CONFIG_WIFI_TCP_UDP_DEMO_UDP_SERVER
    run_udp_server();
#endif

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
