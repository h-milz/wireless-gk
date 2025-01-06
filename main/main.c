/* 
 * simulate the receiving side UDP packet unpacking from 
 * 24 byte / sample to 32 byte / sample for AK4438 mode 18. 
 */ 

#include <string.h>
#include <stdlib.h>
#include <stdio.h> 
#include <dirent.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/pulse_cnt.h"
#include "rom/ets_sys.h"
#include "lwip/err.h"
#include <lwip/netdb.h>
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "wireless_gk.h"

static const char *TAG = "wgk_main";

// static volatile uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

// static uint8_t i2sbuf[4 * NUM_SLOTS * NSAMPLES];   // no longer needed, we work directly with the DMA bufs. 
static uint8_t udpbuf[3 * NUM_SLOTS * NSAMPLES];


#if (defined RX_DEBUG || defined TX_DEBUG)
#define NUM 20
typedef struct { 
    uint8_t loc;        // location
    uint32_t time;      // timestamp in µs
    uint8_t *ptr;       // pointer to buffer
    uint32_t size;        // data size
    uint32_t uint32ptr;       // converted pointer to buffer
} log_t; 
DRAM_ATTR volatile int p = 0; 
DRAM_ATTR volatile log_t _log[NUM];   
#endif 

// Timer configuration
#include "driver/gptimer.h"

// Handle for the general-purpose timer
static gptimer_handle_t gptimer = NULL;

// Initialize the hardware timer
static void init_hardware_timer(void) {
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Use default clock source
        .direction = GPTIMER_COUNT_UP,     // Count upwards
        .resolution_hz = 1000000,          // 1 MHz resolution (1 tick = 1 µs)
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
}

// ISR-safe function to read the timer value
static uint32_t get_time_us_in_isr(void) {
    uint64_t timer_val = 0;
    ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &timer_val));
    return (uint32_t) timer_val & 0xffffffff;
}


typedef struct { 
    uint8_t *dma_buf;       // pointer to buffer
    uint32_t size;        // data size
} dma_params_t; 



/*
 * WiFi stuff
 */

// mostly copied from examples/wifi/getting_started/station/main/station_example_main.c
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


static void monitor_task(void *pvParameters) {
    UBaseType_t hwm; 
    char *task_name; 
    TaskHandle_t myself = xTaskGetCurrentTaskHandle();
    TaskHandle_t tasks[] = { i2s_rx_task_handle, udp_tx_task_handle, myself };
    while (1) {
        for (int i=0; i<sizeof(tasks)/sizeof(TaskHandle_t); i++) {
            hwm = uxTaskGetStackHighWaterMark(tasks[i]);
            task_name = pcTaskGetName(tasks[i]);
            ESP_LOGI (MON_TAG, "task %s, HWM %d", task_name, hwm);
        }
        ESP_LOGI (MON_TAG, "largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

        vTaskDelay (1000/portTICK_PERIOD_MS);
    }
}


void app_main(void) {
    // int i, n;
    // int64_t start_time, stop_time;
    // float elapsed_time;
    bool setup = false, sender = false; 
    
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // check if we're sender or receiver. The sender has ID_PIN = 0
    gpio_reset_pin(ID_PIN);
    gpio_set_direction(ID_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ID_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(ID_PIN);
    if (gpio_get_level(ID_PIN) == 0) {
        sender = true; 
    } 
    gpio_reset_pin(ID_PIN);

    // check if SETUP is pressed and then enter setup mode.  
    gpio_reset_pin(SETUP_PIN);
    gpio_set_direction(SETUP_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SETUP_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(SETUP_PIN);
    if (gpio_get_level(SETUP_PIN) == 0) {
        setup = true; 
    } 
    gpio_reset_pin(SETUP_PIN);
    // TODO start actual setup mode

    // TODO Pins for S1 and S2
    // can be different for Rx and Tx depending on the PCB layout. 
    // can we read these using the ULP in the background every, say, 10 ms? 
            
    // LED to display function. 
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(LED_PIN);
    gpio_set_intr_type(LED_PIN, GPIO_INTR_DISABLE);
    gpio_set_level(LED_PIN, 0);

/*    
    // signal pin for loop measurements with oscilloscope
    gpio_reset_pin(SIG_PIN);
    gpio_set_direction(SIG_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(SIG_PIN);
    gpio_set_intr_type(SIG_PIN, GPIO_INTR_DISABLE);
    gpio_set_level(SIG_PIN, 0);    
*/

    init_hardware_timer();

    if (sender) {
        // initialize Wifi STA
        init_wifi_sender();
        
        // set up I2S receive channel on the Sender
        i2s_new_channel(&i2s_chan_cfg, NULL, &rx_handle);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_tdm_mode(rx_handle, &rx_cfg);

        // create I2S Rx task
        xTaskCreate(i2s_rx_task, "i2s_rx_task", 1024, NULL, 4, &i2s_rx_task_handle);
    
        // create UDP Tx task
        xTaskCreate(udp_tx_task, "udp_tx_task", 2048, NULL, 5, &udp_tx_task_handle);

        xTaskCreate(monitor_task, "monitor_task", 2048, NULL, 3, NULL);

        // create I2S rx on_recv callback
        i2s_event_callbacks_t cbs = {
            .on_recv = i2s_rx_callback,
            .on_recv_q_ovf = NULL,
            .on_sent = NULL,
            .on_send_q_ovf = NULL,
        };
        i2s_channel_register_event_callback(rx_handle, &cbs, NULL);

        // enable channel
        i2s_channel_enable(rx_handle);
        
    } else {
        // initialize Wifi AP and UDP listener
        // enable_network_receiver();
        
        // set up I2S send channel on the Receiver
        i2s_new_channel(&i2s_chan_cfg, &tx_handle, NULL);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_std_mode(tx_handle, &tx_cfg);
        // enable channel
        i2s_channel_enable(tx_handle);

        // hier könnte man die Steuerung über den on_sent callback machen. Wenn ein Buf geschickt ist, hole neues UDP-Paket. 
        // Dann braucht man da auch nicht zu pollen. 
        
        // xTaskCreate(udp_rx_task, "udp_rx_task", 4096, NULL, 5, NULL);
    }
    
    // we should never end up here. 
    // vTaskDelete(NULL);
    while (1) {
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}


