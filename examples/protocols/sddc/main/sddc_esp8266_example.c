/* SDDC Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "protocol_examples_common.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "sddc.h"
#include "cJSON.h"

#include "driver/gpio.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "smartconfig_ack.h"

static EventGroupHandle_t wifi_event_group;
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "sc";
static int smartconfig_flag = 0;

/*
 * handle MESSAGE
 */
static sddc_bool_t esp8266_on_message(sddc_t *sddc, const uint8_t *uid, const char *message, size_t len)
{
    cJSON *root = cJSON_Parse(message);

    /*
     * Parse here
     */

    char *str = cJSON_Print(root);
    sddc_printf("esp8266_on_message: %s\n", str);
    cJSON_free(str);
    cJSON_Delete(root);

    return SDDC_TRUE;
}

/*
 * handle MESSAGE ACK
 */
static void esp8266_on_message_ack(sddc_t *sddc, const uint8_t *uid, uint16_t seqno)
{
}

/*
 * handle MESSAGE lost
 */
static void esp8266_on_message_lost(sddc_t *sddc, const uint8_t *uid, uint16_t seqno)
{
}

/*
 * handle EdgerOS lost
 */
static void esp8266_on_edgeros_lost(sddc_t *sddc, const uint8_t *uid)
{
}

/*
 * handle UPDATE
 */
static sddc_bool_t esp8266_on_update(sddc_t *sddc, const uint8_t *uid, const char *udpate_data, size_t len)
{
    cJSON *root = cJSON_Parse(udpate_data);

    if (root) {
        /*
         * Parse here
         */

        char *str = cJSON_Print(root);

        sddc_printf("esp8266_on_update: %s\n", str);

        cJSON_free(str);

        cJSON_Delete(root);

        return SDDC_TRUE;
    } else {
        return SDDC_FALSE;
    }
}

/*
 * handle INVITE
 */
static sddc_bool_t esp8266_on_invite(sddc_t *sddc, const uint8_t *uid, const char *invite_data, size_t len)
{
    cJSON *root = cJSON_Parse(invite_data);

    if (root) {
        /*
         * Parse here
         */

        char *str = cJSON_Print(root);

        sddc_printf("esp8266_on_invite: %s\n", str);

        cJSON_free(str);

        cJSON_Delete(root);

        return SDDC_TRUE;
    } else {
        return SDDC_FALSE;
    }
}

/*
 * handle the end of INVITE
 */
static sddc_bool_t esp8266_on_invite_end(sddc_t *sddc, const uint8_t *uid)
{
    return SDDC_TRUE;
}

/*
 * Create REPORT data
 */
static char *esp8266_report_data_create(void)
{
    cJSON *root;
    cJSON *report;
    char *str;

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "report", report = cJSON_CreateObject());
        cJSON_AddStringToObject(report, "name",   "IoT Pi");
        cJSON_AddStringToObject(report, "type",   "device");
        cJSON_AddBoolToObject(report,   "excl",   SDDC_FALSE);
        cJSON_AddStringToObject(report, "desc",   "IoT Pi");
        cJSON_AddStringToObject(report, "model",  "1");
        cJSON_AddStringToObject(report, "vendor", "ACOINFO");

    /*
     * Add extension here
     */

    str = cJSON_Print(root);
    sddc_printf("REPORT DATA: %s\n", str);
    cJSON_Delete(root);

    return str;
}

/*
 * Create INVITE data
 */
static char *esp8266_invite_data_create(void)
{
    cJSON *root;
    cJSON *report;
    char *str;

    root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "report", report = cJSON_CreateObject());
        cJSON_AddStringToObject(report, "name",   "IoT Pi");
        cJSON_AddStringToObject(report, "type",   "device");
        cJSON_AddBoolToObject(report,   "excl",   SDDC_FALSE);
        cJSON_AddStringToObject(report, "desc",   "IoT Pi");
        cJSON_AddStringToObject(report, "model",  "1");
        cJSON_AddStringToObject(report, "vendor", "ACOINFO");

    /*
     * Add extension here
     */

    str = cJSON_Print(root);
    sddc_printf("INVITE DATA: %s\n", str);

    cJSON_Delete(root);

    return str;
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    system_event_sta_disconnected_t *event = (system_event_sta_disconnected_t *)event_data;

    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    if (event->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
        /*Switch to 802.11 bgn mode */
        esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
    }
}

static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_wifi_stop() );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status) {
    case SC_STATUS_WAIT:
        ESP_LOGI(TAG, "SC_STATUS_WAIT");
        break;
    case SC_STATUS_FIND_CHANNEL:
        ESP_LOGI(TAG, "SC_STATUS_FIND_CHANNEL");
        break;
    case SC_STATUS_GETTING_SSID_PSWD:
        ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
        break;
    case SC_STATUS_LINK:
        ESP_LOGI(TAG, "SC_STATUS_LINK");
        wifi_config_t *wifi_config = pdata;
        ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
        ESP_ERROR_CHECK( esp_wifi_connect() );
        break;
    case SC_STATUS_LINK_OVER:
        ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
        if (pdata != NULL) {
            sc_callback_data_t *sc_callback_data = (sc_callback_data_t *)pdata;
            switch (sc_callback_data->type) {
            case SC_ACK_TYPE_ESPTOUCH:
                ESP_LOGI(TAG, "Phone IP: %d.%d.%d.%d", sc_callback_data->ip[0], sc_callback_data->ip[1], sc_callback_data->ip[2], sc_callback_data->ip[3]);
                ESP_LOGI(TAG, "TYPE: ESPTOUCH");
                break;
            case SC_ACK_TYPE_AIRKISS:
                ESP_LOGI(TAG, "TYPE: AIRKISS");
                break;
            default:
                ESP_LOGE(TAG, "TYPE: ERROR");
                break;
            }
        }
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
        break;
    default:
        break;
    }
}

static void smartconfig_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    while (1) {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "Wi-Fi Connected to AP");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "SmartConfig over");
            esp_smartconfig_stop();
            smartconfig_flag = 0;
            vTaskDelete(NULL);
        }
    }
}

static void flash_key_task(void *arg)
{
    int i = 0;

    smartconfig_flag = 0;
    while (1) {
        vTaskDelay(1000 / portTICK_RATE_MS);
        if (!gpio_get_level(0)) {
            i++;
        } else {
            i = 0;
        }
        if (i > 3) {
            i = 0;
            sddc_printf("Start SmartConfig....\n");
            if (!smartconfig_flag) {
                smartconfig_flag = 1;
                initialise_wifi();
                xTaskCreate(smartconfig_task, "smartconfig_task", 2048, NULL, 10, NULL);
            }
        }
    }
}

static void sddc_example_task(void *arg)
{
    sddc_t *sddc;
    char *data;
    uint8_t mac[6];
    char ip[sizeof("255.255.255.255")];
    tcpip_adapter_ip_info_t ip_info = { 0 };

    /*
     * Create SDDC
     */
    sddc = sddc_create(SDDC_CFG_PORT);

    /*
     * Set call backs
     */
    sddc_set_on_message(sddc, esp8266_on_message);
    sddc_set_on_message_ack(sddc, esp8266_on_message_ack);
    sddc_set_on_message_lost(sddc, esp8266_on_message_lost);
    sddc_set_on_invite(sddc, esp8266_on_invite);
    sddc_set_on_invite_end(sddc, esp8266_on_invite_end);
    sddc_set_on_update(sddc, esp8266_on_update);
    sddc_set_on_edgeros_lost(sddc, esp8266_on_edgeros_lost);

    /*
     * Set token
     */
#if SDDC_CFG_SECURITY_EN > 0
    sddc_set_token(sddc, "1234567890");
#endif

    /*
     * Set report data
     */
    data = esp8266_report_data_create();
    sddc_set_report_data(sddc, data, strlen(data));

    /*
     * Set invite data
     */
    data = esp8266_invite_data_create();
    sddc_set_invite_data(sddc, data, strlen(data));

    /*
     * Get mac address
     */
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);

    /*
     * Set uid
     */
    sddc_set_uid(sddc, mac);

    /*
     * Get and print ip address
     */
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);

    inet_ntoa_r(ip_info.ip, ip, sizeof(ip));

    sddc_printf("IP addr: %s\n", ip);

    /*
     * SDDC run
     */
    while (1) {
        sddc_printf("SDDC running...\n");

        sddc_run(sddc);

        sddc_printf("SDDC quit!\n");
    }

    /*
     * Destroy SDDC
     */
    sddc_destroy(sddc);
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    wifi_event_group = xEventGroupCreate();
    xTaskCreate(flash_key_task,    "flash_key_task",    4096, NULL, 5, NULL);
    xTaskCreate(sddc_example_task, "sddc_example_task", 4096, NULL, 5, NULL);
}
