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

#define SAMPLE_RATE 441               // 44100
#define ISR_PIN GPIO_NUM_15         // take the one that is nearest to the WS pin as far as PCB layout
#define ID_PIN GPIO_NUM_23          // take the one that is nearest to GND
#define SETUP_PIN GPIO_NUM_17       // take the one that is nearest to the push button

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif


static const char *TAG = "wirelessGK";

// static uint32_t end_time = 0, elapsed_time = 0;
static bool sender = false;
static volatile bool i2s_data_ready = false;

static volatile uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

static uint8_t i2sbuf[4 * SLOTSIZE * NSAMPLES];
static uint8_t udpbuf[3 * SLOTSIZE * NSAMPLES];

static i2s_chan_handle_t rx_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL;

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
 * this ISR gets called each time the PCNT reaches NSAMPLES
 */
static bool IRAM_ATTR pcnt_intr_handler(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
    i2s_data_ready = true;
    ++sample_count; 
    return false; 
}

static struct _log { 
    int loc;
    int64_t time;
} log_t; 

volatile log_t log[101];   

volatile p = 0; 

// OK it works fine with the external GPIO interrupt but not this way with the on_recv interrupt. 
static void IRAM_ATTR handle_interrupt(void *args) {
    // here we count the number of samples received and raise i2s_data_ready if a full frame is available. 
    ++sample_count; 
    if (sample_count % NSAMPLES == 0) {
        i2s_data_ready = true;
        log[p].loc = 1;
        log[p].time = esp_timer_get_time();
    }
    // ets_printf("ISR: %d, data_ready = %d \n", sample_count, i2s_data_ready);
}

static void init_pcnt(void) {
    pcnt_unit_config_t unit_config = {
        .high_limit = NSAMPLES,
        .low_limit = -1,                // strange but needs to be < 0
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    pcnt_new_unit(&unit_config, &pcnt_unit);

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = ISR_PIN,
        .level_gpio_num = -1,    // unused
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan);

    pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    // seems there are no defaults, hence we set them to ignore. We want only the rising edge. 
    pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP);

    pcnt_unit_add_watch_point(pcnt_unit, NSAMPLES);
    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_intr_handler,
    };
    pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, NULL);

    pcnt_unit_enable(pcnt_unit);
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit);
}

// static int64_t start_time = 0, stop_time = 0;
// static int64_t prev_time = 0; 

#define ENTRIES 1024
static int64_t timestamp[ENTRIES+1];
int j = 0;
static uint32_t tt = 0;

static void i2s_rx_task(void *args) {
    int i;
    esp_err_t ret;
    size_t bytes_to_read = sizeof(i2sbuf);
    size_t bytes_read = 0;

    while (1) {
        if (i2s_data_ready) {
            i2s_data_ready = false; 
            // if (start_time == 0) start_time = esp_timer_get_time();
            j++;
            tt++;
            
            // Read data from the queue
            printf("%s: int: %lu, here: %lu\n", __func__, t, tt);

            // we want to know the average time for a complete loop. 
            // measured this way, a loop takes 5439 µs = 5.439 ms
            // which appears a bit too much. 
            // the interrupt rate should be 1.36 ms

/*
            timestamp[j] = esp_timer_get_time();
            j++;
*/            
/*
            if (j > ENTRIES) {
                int k;
                int64_t sum = 0, average;
                int intrate = 1000000*NSAMPLES/SAMPLE_RATE;

                stop_time =  esp_timer_get_time();            
                average = (stop_time - start_time)/ENTRIES;
                ESP_LOGI (TAG, "average loop: %lld µs", average); 
                ESP_LOGI (TAG, "int rate should be %d µs", intrate); 
                ESP_LOGI (TAG, "factor = %f", (float)average / (float)intrate);
                vTaskDelete(NULL);
            }
*/

            // Read data from I2S buffer
            // the read itself needs 13-14 µs on a C6. 
            ret = i2s_channel_read(rx_handle, i2sbuf, bytes_to_read, &bytes_read, 1000);
            
            switch (ret) {
                case ESP_OK:
                    if (bytes_read != bytes_to_read) {
                        ESP_LOGE(TAG, "I2S read error: only %d bytes read", bytes_read);
                    }
                    // Process collected samples
                    // ESP_LOGI (TAG, "%s: read %u bytes", __func__, bytes_read);
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
            // vTaskDelay(10900 * (NSAMPLES / SAMPLE_RATE) / portTICK_PERIOD_MS); 
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
#if 0        
        init_pcnt();
#else
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
#endif
        
        // set up I2S receive channel on the Sender
        i2s_new_channel(&chan_cfg, NULL, &rx_handle);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_std_mode(rx_handle, &rx_cfg);
        // enable channel
        i2s_channel_enable(rx_handle);

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


