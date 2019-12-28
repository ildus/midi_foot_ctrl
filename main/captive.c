#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <lwip/netdb.h>

static bool dns_enabled = true;
static const char * TAG = "dns";

static int create_udp_socket(int port)
{
    struct sockaddr_in saddr = {0};
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket. Error %d", errno);
        return -1;
    }

    // Bind the socket to any address
    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
        goto err;
    }

    // All set, socket is configured for sending and receiving
    return sock;

err:
    close(sock);
    return -1;
}

static void dns_main(void * pvParameters)
{
    uint8_t data[128];
    int len = 0;
    struct sockaddr_in client = {0};
    socklen_t client_len = sizeof(struct sockaddr_in);
    uint32_t i = 0;

    ESP_LOGI(TAG, "DNS server has started");

    int sock = create_udp_socket(53);

    if (sock < 0)
        ESP_LOGE(TAG, "Failed to create IPv4 multicast socket");

    while (dns_enabled)
    {
        len = recvfrom(sock, data, 100, 0, (struct sockaddr *)&client, &client_len);

        if ((len < 0) || (len > 100))
        {
            ESP_LOGE(TAG, "recvfrom error\n");
            continue;
        }

        printf("DNS request:");
        for (i = 0x4; i < len; i++)
        {
            if ((data[i] >= 'a' && data[i] <= 'z') || (data[i] >= 'A' && data[i] <= 'Z') || (data[i] >= '0' && data[i] <= '9'))
                printf("%c", data[i]);
            else
                printf("_");
        }
        printf("\r\n");

        data[2] |= 0x80;
        data[3] |= 0x80;
        data[7] = 1;

        data[len++] = 0xc0;
        data[len++] = 0x0c;

        data[len++] = 0x00;
        data[len++] = 0x01;
        data[len++] = 0x00;
        data[len++] = 0x01;

        data[len++] = 0x00;
        data[len++] = 0x00;
        data[len++] = 0x00;
        data[len++] = 0x0A;

        data[len++] = 0x00;
        data[len++] = 0x04;

        data[len++] = 192;
        data[len++] = 168;
        data[len++] = 4;
        data[len++] = 1;

        sendto(sock, data, len, 0, (struct sockaddr *)&client, client_len);
        vTaskDelay(10);
    }

    ESP_LOGI(TAG, "DNS server stopped");
    shutdown(sock, 0);
    close(sock);
    vTaskDelete(NULL);
}

void shutdown_dns()
{
    dns_enabled = false;
}

void start_dns_server()
{
    xTaskCreate(&dns_main, "dns_task", 2048, NULL, 5, NULL);
}
