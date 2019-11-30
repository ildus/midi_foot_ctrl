#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "midi.h"

#include "lwip/err.h"
#include "lwip/sys.h"

/* The examples use WiFi configuration that you can set via 'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define WIFI_SSID      "bigfoot"
#define WIFI_PASS      "12341234"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static const char *TAG = "wifi softAP";

typedef struct {
	char *type;
	const char *start;
	const char *end;
} file_cxt;

extern const char index_start[]		asm("_binary_index_html_start");
extern const char index_end[]		asm("_binary_index_html_end");
extern const char comp_start[]		asm("_binary_components_js_start");
extern const char comp_end[]		asm("_binary_components_js_end");
extern const char pure_start[]		asm("_binary_pure_css_start");
extern const char pure_end[]		asm("_binary_pure_css_end");

static esp_err_t file_handler(httpd_req_t *req)
{
	file_cxt *ctx = req->user_ctx;
    httpd_resp_set_type(req, ctx->type);
    httpd_resp_send(req, ctx->start, ctx->end - ctx->start - 1);
    return ESP_OK;
}

file_cxt index_cxt = {"text/html", index_start, index_end};
file_cxt comp_cxt = {"text/js", comp_start, comp_end};
file_cxt pure_cxt = {"text/css", pure_start, pure_end};

void register_file_url(httpd_handle_t server, char *uri, file_cxt *cxt)
{
    httpd_uri_t data = {
        .uri       = uri,
        .method    = HTTP_GET,
        .handler   = file_handler,
        .user_ctx  = cxt,
    };
    httpd_register_uri_handler(server, &data);
}

void start_http_server()
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server");
    ESP_ERROR_CHECK(httpd_start(&server, &config));

	register_file_url(server, "/", &index_cxt);
	register_file_url(server, "/components.js", &comp_cxt);
	register_file_url(server, "/pure.css", &pure_cxt);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void initialise_wifi()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}
