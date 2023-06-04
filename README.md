# espidf_pppos
Thư viện này switch toàn bộ lưu lượng mạng từ wifi sang gsm cho esp32
# Hỗ trợ
- SIM76xx
- SIM800,SIM900
- BG96
# Sử dụng
### Copy thư viện vào thư mục main của dự án của bạn
### Vào sdk config
 - Bật:  Enable PPP support (new/experimental)
 - Bật:  Enable Notify Phase Callback
 - Bật:  Enable PAP support
### Trong CmakeList.txt add các dòng sau :
"espidf_pppos/esp_modem.c"
"espidf_pppos/esp_modem_dce_service"
"espidf_pppos/esp_modem_netif.c"
"espidf_pppos/esp_modem_compat.c"
"espidf_pppos/sim800.c"
"espidf_pppos/sim7600.c"
"espidf_pppos/bg96.c"
"espidf_pppos/espidf_pppos.c"

### Include thư viện vào main
#include "espidf_pppos/espidf_pppos.h"

### Gọi hàm GSM_init để khởi động
void GSM_init(int tx_pin,int rx_pin,int rst_pin,int uart_num, void * _modem_err_callback, void * _pppos_ok_callback, void * _pppos_dis_callback);

# Ví dụ
void gsm_error()
{
  ESP_LOGI(TAG, "Module error, please check hardware");
}
void internet_ok()
{
  ESP_LOGI(TAG, "Internet ok");
}
void internet_err()
{
  ESP_LOGI(TAG, "Internet stop");
}
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    GSM_init(17,16,18,UART_NUM_1,gsm_error,internet_ok,internet_err);

    while(1)
    {
       vTaskDelay(1);
    }
}

