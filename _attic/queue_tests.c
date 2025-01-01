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
#include "driver/i2s_std.h"
// #include "driver/i2s_tdm.h"
#include "driver/pulse_cnt.h"
#include "rom/ets_sys.h"

// #define MAXLEN 256
#define NSAMPLES 60                 // the number of samples we want to send in a batch
#define SLOTSIZE 8                  // TDM256, 8 slots per sample
// #define NMAX 100000

#define SAMPLE_RATE 44100               // 44100
#define ISR_PIN GPIO_NUM_15         // take the one that is nearest to the WS pin as far as PCB layout
#define ID_PIN GPIO_NUM_23          // take the one that is nearest to GND
#define SETUP_PIN GPIO_NUM_17       // take the one that is nearest to the push button

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

static const char *TAG = "wirelessGK";

// static uint32_t end_time = 0, elapsed_time = 0;
static bool sender = false;
volatile bool i2s_data_ready = false;
// static uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

static uint8_t i2sbuf[4 * SLOTSIZE * NSAMPLES];
static uint8_t udpbuf[3 * SLOTSIZE * NSAMPLES];

static i2s_chan_handle_t rx_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL;

static QueueHandle_t pcnt_evt_queue; // Queue to signal processing task

// The channel config is the same for both. 
static i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

// I2S Rx config for the sender
static i2s_std_config_t rx_cfg = {
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

/*
 * this ISR gets called each time the counter reaches NSAMPLES
 * it sends a token to the tx task
 */
 
// The method using the queue works up to a sample rate of 16608 Hz, i.e. up to 266.8 events / second. 
// Above, the queue handler produces stack traces. 
// So, we're back to using busy polling and vTaskDelay, which can handle it easily. 
 
static bool IRAM_ATTR pcnt_intr_handler(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // QueueHandle_t queue = (QueueHandle_t)user_ctx;
    // send event data to queue, from this interrupt callback
    // seems not to accept the user_ctx value, so we give it the global variable. 
    // xQueueSendFromISR(pcnt_evt_queue, &(edata->watch_point_value), &xHigherPriorityTaskWoken); // so we send NSAMPLES. 
    // return (xHigherPriorityTaskWoken == pdTRUE);
    i2s_data_ready = true;
    return false; 
}


static void init_pcnt(void) {
    ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = NSAMPLES,
        .low_limit = -1,                // strange but needs to be < 0
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    ESP_LOGI(TAG, "install pcnt channels");
    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = ISR_PIN,
        .level_gpio_num = -1,    // unused
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan));

    ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));
    // seems there are no defaults, hence we set them to ignore. We want only the rising edge. 
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP));

    ESP_LOGI(TAG, "add watch points and register callbacks");
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, NSAMPLES));
    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_intr_handler,
    };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, pcnt_evt_queue));

    ESP_LOGI(TAG, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_LOGI(TAG, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_LOGI(TAG, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}


static void i2s_rx_task(void *args) {
    // uint32_t evt;
    int i;
    esp_err_t ret;
    size_t bytes_to_read = sizeof(i2sbuf);
    size_t bytes_read = 0;
    // int64_t start_time, stop_time;
    while (1) {
        // if (xQueueReceive(pcnt_evt_queue, &evt, portMAX_DELAY)) {
        if (i2s_data_ready) {
            // Read data from the queue
            // ESP_LOGI (TAG, "%s: received message %lu", __func__, evt);            
            
            // Read data from I2S buffer
            // start_time = esp_timer_get_time();
            ret = i2s_channel_read(rx_handle, i2sbuf, bytes_to_read, &bytes_read, 100);
            // stop_time = esp_timer_get_time();
            // ESP_LOGI(TAG, "%s:  %llu µs", __func__, stop_time-start_time);   
            
            switch (ret) {
                case ESP_OK:
                    // if (ret == ESP_OK && bytes_read == bytes_to_read) {
                    // Process collected samples
                    ESP_LOGI (TAG, "%s: read %u bytes", __func__, bytes_read);
                    // frame packing
                    for (i=0; i<NSAMPLES; ++i) {   
                        memcpy(udpbuf+3*i, i2sbuf+4*i, 3); 
                    }
                    // ESP_LOGI(TAG, "%s: frame packed", __func__);
                    // TODO udpbuf has 1440 bytes for 60 samples, 
                    // so we have 32 byte left for a checksum (within the max payload size of 1472 byte)
                    // it takes time, but does it help us in any way? 
                    // if a single bit flips, we discard the entire frame? or just a single sample and repeat the previous one? 
                    // so basically, we would need to checksum each individual slot within a sample. 
                    // and then repair invalid slots by linearly interpolating between the 2 neighbouring values. 
                    // one could omit packing and use the fourth byte in each slot as a checksum. 
                    // the we need to reduce NSAMPLES to 46. 
                    // I don't even want to think about what this means for latency. 

                    // UDP send            
                    // ESP_LOGI(TAG, "%s: triggering UDP send", __func__);
                    break;
                case ESP_ERR_INVALID_ARG:
                    ESP_LOGE (TAG, "NULL pointer or this handle is not RX handle");
                    break;
                case ESP_ERR_TIMEOUT:
                    ESP_LOGE (TAG, "Reading timeout, no reading event received from ISR within ticks_to_wait");
                    break;
                case ESP_ERR_INVALID_STATE:
                    ESP_LOGE (TAG, "I2S is not ready to read");
                    break;
            }
            i2s_data_ready = false; 
            vTaskDelay(0.9 * (NSAMPLES / SAMPLE_RATE) / portTICK_PERIOD_MS); 
        }
    }
}


static void i2s_tx_task(void *args) {

    // wait for UDP packet
    // unpack data
    // send to I2S 
    // that's all. 

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

    // TODO check if SETUP is pressed and then enter setup mode.  
    

    if (sender) {
        // initialize Wifi STA and UDP
        // enable_network_sender();
        
        // Initialize PCNT
        init_pcnt();
        
        // set up I2S receive channel on the Sender
        i2s_new_channel(&chan_cfg, NULL, &rx_handle);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_std_mode(rx_handle, &rx_cfg);
        // enable channel
        i2s_channel_enable(rx_handle);

        // Create queue for PCNT events
        pcnt_evt_queue = xQueueCreate(10, sizeof(uint32_t));
        assert(pcnt_evt_queue != NULL);

        xTaskCreate(i2s_rx_task, "i2s_rx_task", 4096, NULL, 5, NULL);
    } else {
        // initialize Wifi AP and UDP listener
        // enable_network_receiver();
        
        // set up I2S send channel on the Receiver
        i2s_new_channel(&chan_cfg, &tx_handle, NULL);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_std_mode(tx_handle, &tx_cfg);
        // enable channel
        i2s_channel_enable(tx_handle);

        xTaskCreate(i2s_tx_task, "i2s_tx_task", 4096, NULL, 5, NULL);
    }
    
    // we should never end up here. 
    vTaskDelete(NULL);

}


