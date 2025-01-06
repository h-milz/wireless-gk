/* 
 * simulate the receiving side UDP packet unpacking from 
 * 24 byte / sample to 32 byte / sample for AK4438 mode 18. 
 */ 

#include "wireless_gk.h"

#define ID_PIN                  GPIO_NUM_23             // take the one that is nearest to GND

static const char *TAG = "wgk_main";

EventGroupHandle_t s_wifi_event_group;

// static volatile uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

// static uint8_t i2sbuf[4 * NUM_SLOTS * NSAMPLES];   // no longer needed, we work directly with the DMA bufs. 
uint8_t udpbuf[3 * NUM_SLOTS * NSAMPLES];

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
uint32_t get_time_us_in_isr(void) {
    uint64_t timer_val = 0;
    ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &timer_val));
    return (uint32_t) timer_val & 0xffffffff;
}


/*
 * WiFi stuff
 */

// mostly copied from examples/wifi/getting_started/station/main/station_example_main.c
/* FreeRTOS event group to signal when we are connected*/
static int s_retry_num = 0;



void monitor_task(void *pvParameters) {
    UBaseType_t hwm; 
    char *task_name; 
    TaskHandle_t myself = xTaskGetCurrentTaskHandle();
    TaskHandle_t tasks[] = { i2s_rx_task_handle, udp_tx_task_handle, i2s_tx_task_handle, udp_rx_task_handle, myself };
    while (1) {
        for (int i=0; i<sizeof(tasks)/sizeof(TaskHandle_t); i++) {
            hwm = uxTaskGetStackHighWaterMark(tasks[i]);
            task_name = pcTaskGetName(tasks[i]);
            ESP_LOGI (TAG, "task %s, HWM %d", task_name, hwm);
        }
        ESP_LOGI (TAG, "largest free block: %u", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

        vTaskDelay (1000/portTICK_PERIOD_MS);
    }
}


void app_main(void) {

    bool sender = false; 
    bool creds_available = false;
    
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    s_wifi_event_group = xEventGroupCreate();
    
    // check if we're sender or receiver. The sender has ID_PIN = 0
    gpio_reset_pin(ID_PIN);
    gpio_set_direction(ID_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ID_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(ID_PIN);
    if (gpio_get_level(ID_PIN) == 0) {
        sender = true; 
    } 
    gpio_reset_pin(ID_PIN);

    init_hardware_timer();

    if (sender) {
        // initialize GPIO pins
        // returns true if setup was pressed
        init_gpio_tx();
    
        // TODO check if we had WiFi credentials in NVS, if not, setup. 
    
        // initialize Wifi STA
        init_wifi_tx();
        
        // set up I2S receive channel on the Sender
        i2s_new_channel(&i2s_chan_cfg, NULL, &i2s_rx_handle);
        // this will later be i2s_channel_init_tdm_mode(). 
#ifdef I2S_STD
        i2s_channel_init_std_mode(i2s_rx_handle, &i2s_rx_cfg);
#elif I2S_TDM        
        i2s_channel_init_tdm_mode(i2s_rx_handle, &i2s_rx_cfg);
#endif        

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
        i2s_channel_register_event_callback(i2s_rx_handle, &cbs, NULL);

        // enable channel
        i2s_channel_enable(i2s_rx_handle);
        
    } else {   // receiver
        // initialize GPIO pins
        // returns true if setup was pressed
        init_gpio_rx();

        // TODO check if we had WiFi credentials in NVS, if not, setup. 
    
        // initialize Wifi AP
        init_wifi_rx();
        
        // set up I2S send channel on the Receiver
        i2s_new_channel(&i2s_chan_cfg, &i2s_tx_handle, NULL);
        // this will later be i2s_channel_init_tdm_mode(). 
#ifdef I2S_STD
        i2s_channel_init_std_mode(i2s_tx_handle, &i2s_tx_cfg);
#elif I2S_TDM        
        i2s_channel_init_tdm_mode(i2s_tx_handle, &i2s_tx_cfg);
#endif               

        // TODO strip down stack sizes
        // create I2S Tx task
        xTaskCreate(i2s_rx_task, "i2s_tx_task", 4096, NULL, 4, &i2s_rx_task_handle);
    
        // create UDP Rx task
        // hier könnte man die Steuerung über den on_sent callback machen. Wenn ein Buf geschickt ist, hole neues UDP-Paket. 
        // Dann braucht man da auch nicht zu pollen. 
        
        xTaskCreate(udp_tx_task, "udp_rx_task", 4096, NULL, 5, &udp_tx_task_handle);

        xTaskCreate(monitor_task, "monitor_task", 4096, NULL, 3, NULL);

        // create I2S tx on_sent callback
        i2s_event_callbacks_t cbs = {
            .on_recv = NULL,
            .on_recv_q_ovf = NULL,
            .on_sent = i2s_tx_callback,
            .on_send_q_ovf = NULL,
        };
        i2s_channel_register_event_callback(i2s_tx_handle, &cbs, NULL);

        i2s_channel_enable(i2s_tx_handle);
        
    }
    
    // we should never end up here. 
    // vTaskDelete(NULL);
    while (1) {
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}


