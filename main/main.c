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
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
// #include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/pulse_cnt.h"
#include "rom/ets_sys.h"

#define I2S_NUM                 I2S_NUM_AUTO
#define NSAMPLES                60                      // the number of samples we want to send in a batch
#define NUM_SLOTS               4                       // TDM256, 8 slots per sample
#define SLOT_WIDTH              32                      // 
#define DMA_BUFFER_COUNT        8                       // Number of DMA buffers
#define DMA_BUFFER_SIZE         NSAMPLES * NUM_SLOTS * SLOT_WIDTH / 8  // Size of each DMA buffer

#define SAMPLE_RATE             44100                   // 44100
#define ISR_PIN                 GPIO_NUM_15             // take the one that is nearest to the WS pin as far as PCB layout
#define ID_PIN                  GPIO_NUM_23             // take the one that is nearest to GND
#define SETUP_PIN               GPIO_NUM_17             // take the one that is nearest to the push button
#define SIG_PIN                 GPIO_NUM_6
#define SIG2_PIN                GPIO_NUM_7

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif


static const char *TAG = "wirelessGK";

// static uint32_t end_time = 0, elapsed_time = 0;
static bool sender = false;
// static volatile bool i2s_data_ready = false;

static volatile uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

static uint8_t i2sbuf[2 * NUM_SLOTS * NSAMPLES];
static uint8_t udpbuf[3 * NUM_SLOTS * NSAMPLES];

static i2s_chan_handle_t rx_handle = NULL;
// static i2s_chan_handle_t tx_handle = NULL;

// static TaskHandle_t rx_task_handle; 

// The channel config is the same for both. 
// static i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
static i2s_chan_config_t chan_cfg = {
    .id = I2S_NUM_AUTO,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = DMA_BUFFER_COUNT,
    .dma_frame_num = DMA_BUFFER_SIZE / (SLOT_WIDTH * NUM_SLOTS),
    .auto_clear = true, // Automatically clear DMA buffer on underrun
};       

// I2S Rx config for the sender
static i2s_tdm_config_t rx_cfg = {
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,  // Set MCLK multiple to 256
    },
    .slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO,
                                                I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
/*    
              {
        .data_bit_width = SLOT_WIDTH,
        .slot_bit_width = SLOT_WIDTH,
        .slot_mode = I2S_SLOT_MODE_STEREO,
        .slot_mask = I2S_TDM_SLOT0 | I2S_TDM_SLOT1,
    },
*/    
    .gpio_cfg = {
        .mclk = GPIO_NUM_3,
        .bclk = GPIO_NUM_4,
        .ws = GPIO_NUM_2,
        .dout = I2S_GPIO_UNUSED,
        .din = GPIO_NUM_5,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = true,    // we use AK5538 mode 32/36 (start on rising WS edge)
        },
    },
};

#if 0
// I2S Tx Config for the Receiver
static i2s_std_config_t tx_cfg = {
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        .mclk_multiple = I2S_MCLK_MULTIPLE_256  // Set MCLK multiple to 256
    },
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = GPIO_NUM_3,
        .bclk = GPIO_NUM_4,
        .ws = GPIO_NUM_2,
        .dout = GPIO_NUM_5,
        .din = I2S_GPIO_UNUSED,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = true,    // we use AK4438 mode 14/18 (start on rising WS edge)
        },
    },
};
#endif

volatile int p = 0; 

#define NUM 200
typedef struct { 
    uint8_t loc;
    uint32_t time;
} log_t; 

volatile log_t _log[NUM];   

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

void app_main(void) {
    // int i, n;
    // int64_t start_time, stop_time;
    // float elapsed_time;
    
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // check if we're sender or receiver. The sender has ID_PIN = 0
    gpio_reset_pin(ID_PIN);
    gpio_set_direction(ID_PIN, GPIO_MODE_INPUT);
    // ESP_ERROR_CHECK(gpio_input_enable(ID_PIN)); 
    gpio_set_pull_mode(ID_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(ID_PIN);
    if (gpio_get_level(ID_PIN) == 0) {
        sender = true; 
    } 
    
    // signal pin for loop measurements
    gpio_reset_pin(SIG_PIN);
    gpio_set_direction(SIG_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(SIG_PIN);
    gpio_set_intr_type(SIG_PIN, GPIO_INTR_DISABLE);
    gpio_set_level(SIG_PIN, 0);
    
    // signal pin2 for loop measurements
    gpio_reset_pin(SIG2_PIN);
    gpio_set_direction(SIG2_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(SIG2_PIN);
    gpio_set_intr_type(SIG2_PIN, GPIO_INTR_DISABLE);
    gpio_set_level(SIG2_PIN, 0);
    

    // TODO check if SETUP is pressed and then enter setup mode.  

    if (sender) {
        int i;
        esp_err_t ret;
        size_t bytes_to_read = sizeof(i2sbuf);
        size_t bytes_read = 0;
        char t[][15] = {"", "ISR", "begin loop", "channel_read", "packing", "udp send", "end loop" }; 
        int p = 0;
        
        // initialize Wifi STA and UDP
        // enable_network_sender();
        
        // set up I2S receive channel on the Sender
        i2s_new_channel(&chan_cfg, NULL, &rx_handle);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_tdm_mode(rx_handle, &rx_cfg);
        // enable channel
        i2s_channel_enable(rx_handle);
    
        init_hardware_timer();

        while (1) {
            // gpio_set_level(SIG_PIN, 1);
            ret = i2s_channel_read(rx_handle, i2sbuf, bytes_to_read, &bytes_read, 1000);
            ESP_LOGI (TAG, "channel_read: %d bytes", bytes_read);

            _log[p].loc = 3;
            _log[p].time = get_time_us_in_isr();
            p++;

            // gpio_set_level(SIG_PIN, 0);
            if (ret == ESP_OK && bytes_read == bytes_to_read) {
                // packing
/*                
                for (i=0; i<NSAMPLES; ++i) {   
                    memcpy(udpbuf+3*i, i2sbuf+4*i, 3); 
                }

                _log[p].loc = 4;
                _log[p].time = get_time_us_in_isr();
                p++;    
*/
                // UDP send            
                // ESP_LOGI(TAG, "%s: triggering UDP send", __func__);
                

                _log[p].loc = 5;
                _log[p].time = get_time_us_in_isr();
                p++;

                if (p > NUM) {
                    int q = 0; 
                    printf ("%15s %ld \n", t[_log[q].loc], _log[0].time);
                    for (q=1; q<p; q++) {
                        printf ("%15s %ld diff %ld\n", t[_log[q].loc], _log[q].time, _log[q].time - _log[q-1].time);
                    }
                    vTaskDelete(NULL); 
                }


            } else {
                ESP_LOGE (TAG, "I2S read error: %d", ret);
            }        
        
        }
    } else {
        // initialize Wifi AP and UDP listener
        // enable_network_receiver();
/*        
        // set up I2S send channel on the Receiver
        i2s_new_channel(&chan_cfg, &tx_handle, NULL);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_std_mode(tx_handle, &tx_cfg);
        // enable channel
        i2s_channel_enable(tx_handle);
*/
        // xTaskCreate(i2s_tx_task, "i2s_tx_task", 4096, NULL, 5, NULL);
    }
    
    // we should never end up here. 
    vTaskDelete(NULL);

}


