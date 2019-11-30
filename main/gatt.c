
#include "esp_bt.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <string.h>

#include "midi.h"

#define TAG "midi-ble"

#define PROFILE_NUM 1
#define PROFILE_MIDI_APP_IDX 0
#define MIDI_APP_ID 0x42
#define DEVICE_NAME "Bigfoot"
#define SVC_INST_ID 0

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)
#define BT_DISABLED_FLAG            (1 << 2)

#define CHAR_VAL_LEN_MAX 20

#define MIN_OCTAVE 0
#define MAX_OCTAVE 8

static int current_octave = 4;

static EventGroupHandle_t bt_event_group;
const static int CONNECTED_BIT = BIT0;

// Callback declaration
static void gatts_profile_midi_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

static uint8_t adv_config = 0;

uint16_t midi_handle_table[MIDI_IDX_NB];

static uint8_t service_uuid[16] = {
    // Midi service UUID: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700
    // LSB <--------------------------------------------------------------------------------> MSB
    0x00, 0xc7, 0xc4, 0x4e, 0xe3, 0x6c, 0x51, 0xa7, 0x33, 0x4b, 0xe8, 0xed, 0x5a, 0x0e, 0xb8, 0x03,
};

static uint8_t midi_char_uuid[16] = {
    // Midi characteristi UUID: 7772E5DB-3868-4112-A1A9-F2669D106BF3
    // LSB <--------------------------------------------------------------------------------> MSB
    0xf3, 0x6b, 0x10, 0x9d, 0x66, 0xf2, 0xa9, 0xa1, 0x12, 0x41, 0x68, 0x38, 0xdb, 0xe5, 0x72, 0x77,
};


// Advertising data

// The length of adv data must be less than 31 bytes
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = false,
    .include_txpower     = true,
    .min_interval        = 0x02,
    .max_interval        = 0x0C,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// Scan response data
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x02,
    .max_interval        = 0x0C,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 16,
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Application profile struct
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_MIDI_APP_IDX] = {
        .gatts_cb = gatts_profile_midi_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

// Service
static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read_write_notify   = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t midi_ccc[2]      = {0x80, 0x00};

static const uint8_t empty_midi_packet[5] = {
    0x80, // Header + timestamp high
    0x80, // Timestamp low
    0x00, // Note status
    0x00, // Note
    0x00, // Velocity
};

static uint16_t get_13bit_timestamp()
{
	return (xTaskGetTickCount() * portTICK_PERIOD_MS) % ((2 << 13) - 1);
}

void send_notes(void * arg)
{
    ESP_LOGI(TAG, "created task for sending notes");

	size_t count = 0;
	task_context	*task = arg;

	uint16_t	conn_id = task->conn_id;
	free(task);

    while (++count < 10000)
    {
		midi_event_t	event;

		_Static_assert(sizeof(event) == 5, "struct should be packed");
		midi_generate_note(&event, C, 4, 60);
		midi_setup_note(&event, 0, count % 2 == 0, 0);

		esp_ble_gatts_send_indicate(
			gl_profile_tab[PROFILE_MIDI_APP_IDX].gatts_if,
			conn_id,
			midi_handle_table[IDX_MIDI_CHAR_VAL],
			sizeof(event), (uint8_t *) &event, false);

		vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
	vTaskDelete(NULL);
}

/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t gatt_db[MIDI_IDX_NB] =
{
    // Service Declaration
    [IDX_SVC]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(service_uuid), sizeof(service_uuid), service_uuid}},

    // MIDI Characteristic Declaration
    [IDX_MIDI_CHAR]     =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      sizeof(char_prop_read_write_notify), sizeof(char_prop_read_write_notify), (uint8_t *)&char_prop_read_write_notify}},

    // MIDI Characteristic Value
    [IDX_MIDI_CHAR_VAL] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, midi_char_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      CHAR_VAL_LEN_MAX, sizeof(empty_midi_packet), (uint8_t *)empty_midi_packet}},

    // MIDI Client Characteristic Configuration Descriptor
    [IDX_MIDI_CHAR_CFG]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *) &character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), sizeof(midi_ccc), (uint8_t *) midi_ccc}},
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config &= (~ADV_CONFIG_FLAG);
            if (adv_config == 0) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config == 0) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "advertising start failed");
            } else {
                ESP_LOGI(TAG, "advertising start successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Advertising stop failed");
            } else {
                ESP_LOGI(TAG, "Stop adv successfully\n");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}

void config_advertising()
{
    esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(DEVICE_NAME);
    if (set_dev_name_ret) {
        ESP_LOGE(TAG, "set device name failed, error = %s", esp_err_to_name(set_dev_name_ret));
    }
    //config adv data
    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret) {
        ESP_LOGE(TAG, "config adv data failed, error = %s", esp_err_to_name(ret));
    }
    adv_config |= ADV_CONFIG_FLAG;
    //config scan response data
    ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
    if (ret) {
        ESP_LOGE(TAG, "config scan response data failed, error = %s", esp_err_to_name(ret));
    }
    adv_config |= SCAN_RSP_CONFIG_FLAG;
}

static void gatts_profile_midi_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            config_advertising();
            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, MIDI_IDX_NB, SVC_INST_ID);
            if (create_attr_ret) {
                ESP_LOGE(TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
            break;
        }
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_READ_EVT");
            break;
        case ESP_GATTS_WRITE_EVT:
            if (!param->write.is_prep) {
                // the data length of gattc write  must be less than CHAR_VAL_LEN_MAX.
                ESP_LOGI(TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle, param->write.len);
                esp_log_buffer_hex(TAG, param->write.value, param->write.len);
                if (midi_handle_table[IDX_MIDI_CHAR_CFG] == param->write.handle && param->write.len == 2){
                    uint16_t descr_value = param->write.value[1] << 8 | param->write.value[0];
                    if (descr_value == 0x0001) {
                        ESP_LOGI(TAG, "notify enabled");
                        xEventGroupSetBits(bt_event_group, CONNECTED_BIT);
                    } else if (descr_value == 0x0002) {
                        ESP_LOGI(TAG, "indicate enabled");
                    } else if (descr_value == 0x0000) {
                        ESP_LOGI(TAG, "notify/indicate disable ");
                    } else {
                        ESP_LOGE(TAG, "unknown descr value");
                        esp_log_buffer_hex(TAG, param->write.value, param->write.len);
                    }
                }
                /* send response when param->write.need_rsp is true*/
                if (param->write.need_rsp){
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            }
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT");
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            break;
        case ESP_GATTS_CONF_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status = %d", param->conf.status);
            break;
        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
            esp_log_buffer_hex(TAG, param->connect.remote_bda, 6);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.min_int = 0x02;
            conn_params.max_int = 0x0c;
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);

            gl_profile_tab[PROFILE_MIDI_APP_IDX].conn_id = param->connect.conn_id;

			/* create task */
			task_context *task = (task_context *) malloc(sizeof(task_context));
			task->conn_id = param->connect.conn_id;

			xTaskCreate(send_notes, "notes sender", 2048, (void *) task,
				tskIDLE_PRIORITY, NULL);

            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = %d", param->disconnect.reason);
            xEventGroupClearBits(bt_event_group, CONNECTED_BIT);
            if (adv_config == 0) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
			ESP_LOGI(TAG, "ESP_GATTS_CREAT_ATTR_TAB_EVT");

            if (param->add_attr_tab.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            } else if (param->add_attr_tab.num_handle != MIDI_IDX_NB) {
                ESP_LOGE(TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to MIDI_IDX_NB(%d)", param->add_attr_tab.num_handle, MIDI_IDX_NB);
            } else {
                ESP_LOGI(TAG, "create attribute table successfully, the number handle = %d\n",param->add_attr_tab.num_handle);
                memcpy(midi_handle_table, param->add_attr_tab.handles, sizeof(midi_handle_table));
                esp_ble_gatts_start_service(midi_handle_table[IDX_SVC]);
            }
            break;
        }
        case ESP_GATTS_STOP_EVT:
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    // If event is register event, store the gatts_if for each profile
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK && param->reg.app_id == MIDI_APP_ID) {
            gl_profile_tab[PROFILE_MIDI_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    // Call each profile's callback, which is only one for now
	for (int idx = 0; idx < PROFILE_NUM; idx++) {
		if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
				gatts_if == gl_profile_tab[idx].gatts_if) {
			if (gl_profile_tab[idx].gatts_cb) {
				gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
			}
		}
	}
}


void app_main()
{
    esp_err_t ret;
	bt_event_group = xEventGroupCreate();

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK( ret );
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gatts_app_register(MIDI_APP_ID);
    if (ret){
        ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    ESP_ERROR_CHECK(nvs_flash_init());

    initialise_wifi();
    start_http_server();
}
