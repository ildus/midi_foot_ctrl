#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "midi.h"

/* The examples use WiFi configuration that you can set via 'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define WIFI_SSID      "bigfoot"
#define WIFI_PASS      "12341234"

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

static char *button_names[] = {
	"top_left",
	"top_center",
	"top_right",
	"bottom_left",
	"bottom_center",
	"bottom_right"
};

static char *initial_format = "var initial_values = `";
static char *default_values[] = {
	"action=note_on&d1=60&d2=100",
	"action=cc&d1=109&d2=0",
	"action=cc&d1=108&d2=0",
	"action=cc&d1=68&d2=0",
	"action=cc&d1=86&d2=0",
	"action=cc&d1=85&d2=0"
};


static esp_err_t file_handler(httpd_req_t *req)
{
	file_cxt *ctx = req->user_ctx;
    httpd_resp_set_type(req, ctx->type);
    httpd_resp_send(req, ctx->start, ctx->end - ctx->start - 1);
    return ESP_OK;
}

static esp_err_t initial_handler(httpd_req_t *req)
{
	static size_t init_len = 0;
	esp_err_t err;
	size_t	len = 0;
	size_t	lengths[N_BUTTONS];
	size_t	key_lengths[N_BUTTONS];

	if (init_len == 0)
		init_len = strlen(initial_format);

	nvs_handle my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
	ESP_ERROR_CHECK(err);

	/* first calculate whole length */
	for (size_t i = 0; i < N_BUTTONS; i++)
	{
		size_t	vallen;
		char *key = button_names[i];
		key_lengths[i] = strlen(key);

		err = nvs_get_str(my_handle, key, NULL, &vallen);
		switch (err)
		{
			case ESP_OK:
				lengths[i] = vallen - 1; /* minus zero terminator */
				break;
			case ESP_ERR_NVS_NOT_FOUND:
				len += strlen(default_values[i]);
				lengths[i] = 0;		/* indicates that it's default */
				break;
			default:
				ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
				ESP_ERROR_CHECK(err);
		}
		len += key_lengths[i] + 1; /* plus ':' */
		len += lengths[i];
	}

	/* we allocate more, just for any case and \n delimiters */
	char *res = malloc(init_len + len + 100);
	if (res == 0)
	{
		ESP_LOGE(TAG, "could not allocate memory");
		return ESP_FAIL;
	}
	char *pos = res;

	/* fill the output */
	memcpy(pos, initial_format, init_len);
	pos += init_len;

	for (size_t i = 0; i < N_BUTTONS; i++)
	{
		char *key = button_names[i];
		memcpy(pos, key, key_lengths[i]);
		pos += key_lengths[i];
		*pos++ = ':';

		if (lengths[i] == 0)
		{
			size_t vallen = strlen(default_values[i]);
			memcpy(pos, default_values[i], vallen);
			pos += vallen;
		}
		else
		{
			ESP_ERROR_CHECK(nvs_get_str(my_handle, key, pos, &lengths[i]))
			pos += lengths[i];
		}

		*pos = '\n';
		pos++;
	}

	*pos++ = '`';
	*pos++ = ';';
	*pos = '\0';
	ESP_LOGI(TAG, "%s", res);

	nvs_close(my_handle);

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, res, pos - res);
	free(res);

	return ESP_OK;
}

static esp_err_t configure_handler(httpd_req_t *req)
{
	char *error = NULL;
	char *buf = NULL;
	httpd_resp_set_hdr(req, "Location", "/");

	if (req->content_len > 10000)
	{
		error = "too big packet";
		goto done;
	}

	buf = malloc(req->content_len + 1);
	int ret = httpd_req_recv(req, buf, req->content_len);

	if (ret <= 0 || ret != req->content_len)
	{
		error = "packet reading error";
		goto done;
	}
	buf[req->content_len] = '\0';

	char *pos;
	if ((pos = strstr(buf, "play")) != NULL)
	{
		size_t	len;
		pos += 5;
		midi_event_t *event = parse_action(pos, &len, NULL);
		if (event == NULL)
		{
			error = "action parsing error";
			goto done;
		}

		BaseType_t ret = xQueueSend(req->user_ctx, event, (TickType_t) 0 );

		free(event);
		if (ret == pdFALSE)
		{
			error = "queue of events is full";
			goto done;
		}
	}

done:
	if (error) {
		httpd_resp_set_status(req, HTTPD_500);
		httpd_resp_sendstr(req, error);
	}
	else
	{
		httpd_resp_set_status(req, HTTPD_200);
		httpd_resp_sendstr(req, "OK");
	}

	if (buf)
		free(buf);

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

void start_http_server(QueueHandle_t queue)
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

    httpd_uri_t initial = {
        .uri       = "/initial.js",
        .method    = HTTP_GET,
        .handler   = initial_handler,
        .user_ctx  = NULL,
    };
    httpd_register_uri_handler(server, &initial);

    httpd_uri_t configure = {
        .uri       = "/configure",
        .method    = HTTP_POST,
        .handler   = configure_handler,
        .user_ctx  = queue,
    };
    httpd_register_uri_handler(server, &configure);
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
