#include "espidf_pppos.h"

void (*modem_err_callback)();
void (*pppos_ok_callback)();
void (*pppos_dis_callback)();

static const char *TAG = "espidf_pppos";
static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const int STOP_BIT = BIT1;
//static const int GOT_DATA_BIT = BIT2;

static void modem_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_MODEM_EVENT_PPP_START:
        ESP_LOGI(TAG, "Modem PPP Started");
        break;
    case ESP_MODEM_EVENT_PPP_STOP:
        ESP_LOGI(TAG, "Modem PPP Stopped");
        xEventGroupSetBits(event_group, STOP_BIT);
        break;
    case ESP_MODEM_EVENT_UNKNOWN:
        ESP_LOGW(TAG, "Unknown line received: %s", (char *)event_data);
        break;
    default:
        break;
    }
}
static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %d", event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = *(esp_netif_t**)event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}
static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %d", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);

        ESP_LOGI(TAG, "GOT ip event!!!");
        if(pppos_ok_callback)pppos_ok_callback();
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        if(pppos_dis_callback)pppos_dis_callback();
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
        if(pppos_ok_callback)pppos_ok_callback();
    }
}

void GSM_init(int tx_pin,int rx_pin,int rst_pin,int uart_num, void * _modem_err_callback, void * _pppos_ok_callback, void * _pppos_dis_callback)
{
modem_err_callback=_modem_err_callback;
pppos_ok_callback=_pppos_ok_callback;
pppos_dis_callback=_pppos_dis_callback;
ESP_LOGI(TAG, "Module: reset");
gpio_config_t conf = {
        .pin_bit_mask = (1ULL<<rst_pin),                 /*!< GPIO pin: set with bit mask, each bit maps to a GPIO */
        .mode = GPIO_MODE_OUTPUT,                   /*!< GPIO mode: set input/output mode                     */
        .pull_up_en = GPIO_PULLUP_ENABLE,           /*!< GPIO pull-up                                         */
        .pull_down_en = GPIO_PULLDOWN_ENABLE,       /*!< GPIO pull-down                                       */
        .intr_type = GPIO_INTR_DISABLE,  /*!< GPIO interrupt type - previously set                 */
    };
gpio_config(&conf);

gpio_set_level(rst_pin, 0);
vTaskDelay(pdMS_TO_TICKS(2000));
gpio_set_level(rst_pin, 1);
vTaskDelay(pdMS_TO_TICKS(5000));

    int RX_BUF_SIZE = 256;
     uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // We won't use a buffer for sending data.
    uart_driver_install(uart_num, RX_BUF_SIZE , 0, 0, NULL, 0);
    uart_param_config(uart_num, &uart_config);
    uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    int counter_retry = 0;
    while(1) //check module
    {
        uart_write_bytes(uart_num, "AT\r\n", 4);
        vTaskDelay(pdMS_TO_TICKS(100));
        int rxBytes = uart_read_bytes(uart_num, data, RX_BUF_SIZE, 500 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(TAG, "Read %d bytes: '%s'", rxBytes, data);
            if(strstr((const char *)data,(const char *)"OK\r\n") != 0)break;
        }
        counter_retry++;
        if(counter_retry > 20)
        {
            if(modem_err_callback)modem_err_callback();
            free(data);
            return;
        }
    }
    counter_retry = 0;
    while(1) //check network
    {
        uart_write_bytes(uart_num, "AT+NETOPEN\r\n", 12);
        vTaskDelay(pdMS_TO_TICKS(500));
        int rxBytes = uart_read_bytes(uart_num, data, RX_BUF_SIZE, 500 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(TAG, "Read %d bytes: '%s'", rxBytes, data);
            if(strstr((const char *)data,(const char *)"+NETOPEN: 0") != 0)break;
        }
        counter_retry++;
        if(counter_retry > 20)
        {
            if(modem_err_callback)modem_err_callback();
            free(data);
            return;
        }
    }
    free(data);
    uart_write_bytes(uart_num, "AT+NETCLOSE\r\n", 12);
    vTaskDelay(pdMS_TO_TICKS(500));
    uart_driver_delete(uart_num); //xóa uart để cho lib pppos xài

    //ok, run pppos library
    #if CONFIG_LWIP_PPP_PAP_SUPPORT
    esp_netif_auth_type_t auth_type = NETIF_PPP_AUTHTYPE_PAP;
    #elif CONFIG_LWIP_PPP_CHAP_SUPPORT
    esp_netif_auth_type_t auth_type = NETIF_PPP_AUTHTYPE_CHAP;
    #elif !defined(CONFIG_EXAMPLE_MODEM_PPP_AUTH_NONE)
    #error "Unsupported AUTH Negotiation"
    #endif

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    event_group = xEventGroupCreate();

    /* create dte object */
    esp_modem_dte_config_t config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    config.tx_io_num = tx_pin;
    config.rx_io_num = rx_pin;
    config.rts_io_num = -1;
    config.cts_io_num = -1;
    config.rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE;
    config.tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE;
    config.event_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE;
    config.event_task_stack_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_STACK_SIZE;
    config.event_task_priority = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_PRIORITY;
    config.dte_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE / 2;

    modem_dte_t *dte = esp_modem_dte_init(&config);
    /* Register event handler */
    ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, modem_event_handler, ESP_EVENT_ANY_ID, NULL));

    // Init netif object
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&cfg);
    assert(esp_netif);

    void *modem_netif_adapter = esp_modem_netif_setup(dte);
    esp_modem_netif_set_default_handlers(modem_netif_adapter, esp_netif);

    modem_dce_t *dce = NULL;
            /* create dce object */
    #if CONFIG_EXAMPLE_MODEM_DEVICE_SIM800
            dce = sim800_init(dte);
    #elif CONFIG_EXAMPLE_MODEM_DEVICE_BG96
            dce = bg96_init(dte);
    #elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7600
            dce = sim7600_init(dte);
    #else
            dce = sim7600_init(dte);  //sim7600 default
    #endif

    assert(dce != NULL);
    ESP_ERROR_CHECK(dce->set_flow_ctrl(dce, MODEM_FLOW_CONTROL_NONE));
    ESP_ERROR_CHECK(dce->store_profile(dce));
    /* Print Module ID, Operator, IMEI, IMSI */
    ESP_LOGI(TAG, "Module: %s", dce->name);
    ESP_LOGI(TAG, "Operator: %s", dce->oper);
    ESP_LOGI(TAG, "IMEI: %s", dce->imei);
    ESP_LOGI(TAG, "IMSI: %s", dce->imsi);
    /* Get signal quality */
    uint32_t rssi = 0, ber = 0;
    ESP_ERROR_CHECK(dce->get_signal_quality(dce, &rssi, &ber));
    ESP_LOGI(TAG, "rssi: %d, ber: %d", rssi, ber);
    /* Get battery voltage */
    uint32_t voltage = 0, bcs = 0, bcl = 0;
    ESP_ERROR_CHECK(dce->get_battery_status(dce, &bcs, &bcl, &voltage));
    ESP_LOGI(TAG, "Battery voltage: %d mV", voltage);

    ESP_LOGI(TAG, "Start pppos");
    /* setup PPPoS network parameters */
    #if !defined(CONFIG_EXAMPLE_MODEM_PPP_AUTH_NONE) && (defined(CONFIG_LWIP_PPP_PAP_SUPPORT) || defined(CONFIG_LWIP_PPP_CHAP_SUPPORT))
            esp_netif_ppp_set_auth(esp_netif, auth_type, CONFIG_EXAMPLE_MODEM_PPP_AUTH_USERNAME, CONFIG_EXAMPLE_MODEM_PPP_AUTH_PASSWORD);
    #endif
    /* attach the modem to the network interface */
    esp_netif_attach(esp_netif, modem_netif_adapter);
    /* Wait for IP address */
    xEventGroupWaitBits(event_group, CONNECT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
}

