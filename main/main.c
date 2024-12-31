/* 
 * simulate the receiving side UDP packet unpacking from 
 * 24 byte / sample to 32 byte / sample for AK4438 mode 18. 
 */ 

#include <string.h>
#include <stdlib.h>
#include <stdio.h> 
#include <dirent.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "rom/ets_sys.h"


#define MAXLEN 256
#define NSAMPLES 60
#define SLOTSIZE 8
#define NMAX 100000

#define SAMPLE_RATE 4              // 44100
#define ISR_PIN GPIO_NUM_15
#define ID_PIN GPIO_NUM_23           // to identify the master = receiver

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

static const char *TAG = "wirelessGK";

uint32_t end_time = 0, elapsed_time = 0;
bool sender = false;
volatile bool i2s_data_ready = false;
static uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

static uint8_t i2sbuf[4 * SLOTSIZE * NSAMPLES];
static uint8_t udpbuf[3 * SLOTSIZE * NSAMPLES];

static i2s_chan_handle_t rx_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL;

// The channel config is the same for both. 
i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

// I2S Rx config for the sender
i2s_std_config_t rx_cfg = {
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,  // Set MCLK multiple to 256
    },
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
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

// I2S Tx Config for the Receiver
i2s_std_config_t tx_cfg = {
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


/*
static bool IRAM_ATTR i2s_rx_recv_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    // here we count the number of samples received and raise i2s_data_ready if a full frame is available. 
    if (++sample_count == NSAMPLES) {
        i2s_data_ready = true;
        sample_count = 0;
    }
    return false;
}    


// only needed for I2S receive. The Receiver (I2S send) is clocked by the UDP packets. 
i2s_event_callbacks_t cbs = {
    .on_recv = i2s_rx_recv_callback,
    .on_recv_q_ovf = NULL,
    .on_sent = NULL,
    .on_send_q_ovf = NULL,
};
*/

// OK it works fine with the external GPIO interrupt but not this way with the on_recv interrupt. 
static void IRAM_ATTR handle_interrupt(void *args) {
    // here we count the number of samples received and raise i2s_data_ready if a full frame is available. 
    if (++sample_count == NSAMPLES) {
        i2s_data_ready = true;
        sample_count = 0;
    }
    // ets_printf("ISR: %d\n", sample_count);
}

static void i2s_rx_task(void *args) {
    int i; 
    size_t bytes_read = 0;
    int64_t start_time, stop_time;
    
    // endless loop
    while (1) {
        if (i2s_data_ready) {
            // ESP_LOGI(TAG, "%s: data ready", __func__);
        
            start_time = esp_timer_get_time();
            i2s_channel_read(rx_handle, i2sbuf, sizeof(i2sbuf), &bytes_read, 1000);  // TODO see if 1000 is enough
            stop_time = esp_timer_get_time();
            ESP_LOGI (TAG, "%s: i2sbuf size = %u, bytes_read = %u", __func__, sizeof(i2sbuf), bytes_read);
            // TODO check if sizeof(is2_buf) =  bytes_read; if not: trouble!
            
            // ESP_LOGI(TAG, "%s: data read", __func__);
            // frame packing
            for (i=0; i<NSAMPLES; ++i) {   
                memcpy(udpbuf+3*i, i2sbuf+4*i, 3); 
            }
            // ESP_LOGI(TAG, "%s: frame packed", __func__);

            // UDP send            
            // ESP_LOGI(TAG, "%s: triggering UDP send", __func__);
            
    
            i2s_data_ready = false;  

            ESP_LOGI(TAG, "%s:  %.1f ms", __func__, (stop_time-start_time)/1000.0);

            // should we sleep here for a short while? 
            // we know nothing is going to happen for a while. 
            vTaskDelay(0.9 * (NSAMPLES / SAMPLE_RATE) / portTICK_PERIOD_MS); 
        } 
    }
    vTaskDelete(NULL);
}


static void i2s_tx_task(void *args) {

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

    // enable the WS interrupt on ISR_PIN
    gpio_reset_pin(ISR_PIN); 
    gpio_set_direction(ISR_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ISR_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(ISR_PIN);
    gpio_set_intr_type(ISR_PIN, GPIO_INTR_POSEDGE);
    // gpio_isr_register(handle_interrupt, NULL, ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, NULL); 
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ISR_PIN, handle_interrupt, NULL);
    gpio_intr_enable(ISR_PIN);

    // TODO 
    // enable_network();

    if (sender) {
        // set up I2S receive channel on the Sender
        i2s_new_channel(&chan_cfg, NULL, &rx_handle);
        i2s_channel_init_std_mode(rx_handle, &rx_cfg);

        // register recv callback for counting received samples. 
        // ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_handle, &cbs, NULL));

        // enable channel
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

        xTaskCreate(i2s_rx_task, "i2s_rx_task", 8192, NULL, 5, NULL);
        while (1) {
            vTaskDelay(200 / portTICK_PERIOD_MS);  
        }
    } else {
        // set up I2S send channel on the Receiver
        i2s_new_channel(&chan_cfg, &tx_handle, NULL);
        i2s_channel_init_std_mode(tx_handle, &tx_cfg);

        // enable channel
        i2s_channel_enable(tx_handle);

        xTaskCreate(i2s_tx_task, "i2s_tx_task", 4096, NULL, 5, NULL);
    }
    
    // we should never end up here. 
    vTaskDelete(NULL);

}


