
#include "string.h"

#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sys/socket.h"
#include "netdb.h"

#include "joylink.h"

#include "jd_innet.h"
// #include "joylink_innet_ctl.h"
#include "joylink_smnt.h"

// #include "auth/aes.h"

#include "joylink_log.h"
#include "nvs.h"
#include "nvs_flash.h"

// #define AES_KEY  "4QNEXWPFBM4VZJM8"
#define AES_KEY  _g_pdev->jlp.secret_key
const uint8_t AES_IV[16];
uint8_t jd_aes_out_buffer[128];
xTaskHandle jd_innet_timer_task_handle = NULL;
bool jd_innet_timer_task_flag = false;
void joylink_wifi_save_info(uint8_t*ssid,uint8_t*password);
void joylink_delay_3_min_timer_for_adv(void);


/*recv packet callback*/
static void jd_innet_pack_callback(void *buf, wifi_promiscuous_pkt_type_t type)
{
#if 0
    wifi_promiscuous_pkt_t *pack_all = NULL;
    int len=0;
    joylinkResult_t Ret;
    PHEADER_802_11 frame = NULL;
    uint8_t ssid_len = 0;

    if (type != WIFI_PKT_MISC) {
        pack_all = (wifi_promiscuous_pkt_t *)buf;
        frame = (PHEADER_802_11)pack_all->payload;
        len = pack_all->rx_ctrl.sig_len;
        joylink_cfg_DataAction(frame, len);

        if (joylink_cfg_Result(&Ret) == 0) {
            if (Ret.type != 0) {
                memset(AES_IV,0x0,sizeof(AES_IV));
                len = device_aes_decrypt((const uint8 *)AES_KEY, strlen(AES_KEY),AES_IV,
                    Ret.encData + 1,Ret.encData[0],
                    jd_aes_out_buffer,sizeof(jd_aes_out_buffer));
                if (len > 0) {
                    wifi_config_t config;
                    memset(&config,0x0,sizeof(config));

                    if (jd_aes_out_buffer[0] > 32) {
                        log_debug("sta password len error\r\n");
                        esp_restart();
                        return;
                    }
                    
                    memcpy(config.sta.password,jd_aes_out_buffer + 1,jd_aes_out_buffer[0]);

                    ssid_len = len - 1 - jd_aes_out_buffer[0] - 4 - 2;

                    if (ssid_len > sizeof(config.sta.ssid)) {
                        log_debug("sta ssid len error\r\n");
                        esp_restart();
                        return;

                    }
                    strncpy((char*)config.sta.ssid,(const char*)(jd_aes_out_buffer + 1 + jd_aes_out_buffer[0] + 4 + 2),ssid_len);
                    log_debug("ssid:%s\r\n",config.sta.ssid);
                    log_debug("password:%s\r\n",config.sta.password);
                    if (esp_wifi_set_config(ESP_IF_WIFI_STA,&config) != ESP_OK) {
                        log_debug("set sta fail\r\n");
                    } else {
                        if (esp_wifi_connect() != ESP_OK) {
                            log_debug("sta connect fail\r\n");
                        } else {
                            if (jd_innet_timer_task_handle != NULL) {
                                jd_innet_timer_task_flag = true;
                            }
                            esp_wifi_set_promiscuous(0);
                            // save flash
                            joylink_wifi_save_info(config.sta.ssid,config.sta.password);
                            //ble delay 3 min
                            joylink_delay_3_min_timer_for_adv();
                        }
                    }
                } else {
                    log_debug("aes fail\r\n");
                    esp_restart();
                }
            }
        }
    }
#endif
}

#define CHANNEL_ALL_NUM  (13*3)
#define CHANNEL_BIT_MASK ((uint64_t)0x5B6FFFF6DB)

uint8_t adp_changeCh(int i)
{
    static wifi_second_chan_t second_ch = WIFI_SECOND_CHAN_NONE; //0,1,2
    // log_debug("ch:%d, sec:%d \r\n", i, second_ch);
    esp_wifi_set_channel(i, second_ch);
    second_ch++;
    if (second_ch > WIFI_SECOND_CHAN_BELOW){
        second_ch = WIFI_SECOND_CHAN_NONE;
        return 1; //must to change channel
    } else {
        return 0; //don't to change channel
    }
}

static void jd_innet_timer_task (void *pvParameters) // if using timer,it will stack overflow because of log in joylink_cfg_50msTimer;
{
    int32_t delay_tick = 50;
    for (;;) {
        if (delay_tick == 0) {
            delay_tick = 1;
        }
        vTaskDelay(delay_tick);
        if (jd_innet_timer_task_flag) {
            break;
        }
        // delay_tick = joylink_cfg_50msTimer()/portTICK_PERIOD_MS;
    }

    jd_innet_timer_task_handle = NULL;
    jd_innet_timer_task_flag = false;
    log_debug("jd_innet_timer_task delete\r\n");
    vTaskDelete(NULL);
}

static void jd_innet_start (void *pvParameters)
{
    unsigned char* pBuffer = NULL;
    
    esp_wifi_set_promiscuous(0);

    pBuffer = (unsigned char*)malloc(1024);
    if (pBuffer == NULL) {
        log_debug("%s,%d\r\n",__func__,__LINE__);
        vTaskDelete(NULL);
    }
    
    // joylink_cfg_init(pBuffer);
    if (ESP_OK != esp_wifi_set_promiscuous_rx_cb(jd_innet_pack_callback)){
        log_debug ("[%s] set_promiscuous fail\n\r",__func__);
    }

    if (ESP_OK != esp_wifi_set_promiscuous(1)){
        log_debug ("[%s] open promiscuous fail\n\r",__func__);
    }

    if (jd_innet_timer_task_handle != NULL) {
        jd_innet_timer_task_flag = true;
        while(jd_innet_timer_task_flag) {
            vTaskDelay(10);
        }
    }
    xTaskCreate(jd_innet_timer_task, "jd_innet_timer_task", 2048, NULL, tskIDLE_PRIORITY + 5, &jd_innet_timer_task_handle);

    vTaskDelete(NULL);
}

void jd_innet_start_task(void)
{
    xTaskCreate(jd_innet_start, "jd_innet_start", 1024*3, NULL, tskIDLE_PRIORITY + 2, NULL);
}

