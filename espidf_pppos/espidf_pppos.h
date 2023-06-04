#ifndef _ESPIDF_PPPOS_
#define _ESPIDF_PPPOS_

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "mqtt_client.h"
#include "esp_modem.h"
#include "esp_modem_netif.h"
#include "esp_log.h"
#include "sim800.h"
#include "bg96.h"
#include "sim7600.h"
#include "driver/gpio.h"
#include "module_config.h"

void GSM_init(int tx_pin,int rx_pin,int rst_pin,int uart_num, void * _modem_err_callback, void * _pppos_ok_callback, void * _pppos_dis_callback);

#endif 