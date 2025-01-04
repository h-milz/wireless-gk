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
#include "driver/i2s_tdm.h"
#include "driver/pulse_cnt.h"
#include "rom/ets_sys.h"

#define I2S_NUM                 I2S_NUM_AUTO
#define NSAMPLES                60                      // the number of samples we want to send in a batch
#define NUM_SLOTS               4                       // TDM256, 8 slots per sample
#define SLOT_WIDTH              32                      // 
#define DMA_BUFFER_COUNT        4                       // Number of DMA buffers. 
                                                        // 4 is enough because we pick up each individual one
#define DMA_BUFFER_SIZE         NSAMPLES * NUM_SLOTS * SLOT_WIDTH / 8  // Size of each DMA buffer

#define SAMPLE_RATE             44100                   // 44100
#define LED_PIN                 GPIO_NUM_15             // 
#define ID_PIN                  GPIO_NUM_23             // take the one that is nearest to GND
#define SETUP_PIN               GPIO_NUM_17             // take the one that is nearest to the push button
#define SIG_PIN                 GPIO_NUM_6
#define SIG2_PIN                GPIO_NUM_7

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

// #define RX_DEBUG 
// #define TX_DEBUG

static const char *TAG = "wirelessGK";

// static volatile uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

static uint8_t i2sbuf[4 * NUM_SLOTS * NSAMPLES];
static uint8_t udpbuf[3 * NUM_SLOTS * NSAMPLES];

static i2s_chan_handle_t rx_handle = NULL;
static i2s_chan_handle_t tx_handle = NULL;

static TaskHandle_t i2s_rx_task_handle; 

// The channel config is the same for both. 
static i2s_chan_config_t rx_chan_cfg = {
    .id = I2S_NUM_AUTO,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = DMA_BUFFER_COUNT,
    .dma_frame_num = NSAMPLES, 
    .auto_clear_after_cb = false, 
    .auto_clear_before_cb = false, 
    .allow_pd = false, 
    .intr_priority = 5, 
};       

// TODO this will be custom too. 
static i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

// I2S Rx config for the sender
static i2s_tdm_config_t rx_cfg = {
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,  // Set MCLK multiple to 256
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        // .ext_clk_freq_hz = 11289600,         // wenn external. Muss sein sample_rate_hz * slot_bits * slot_num
    },
    .slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO,
                                                I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3 ), 
//                                                | I2S_TDM_SLOT4 | I2S_TDM_SLOT5 | I2S_TDM_SLOT6 | I2S_TDM_SLOT7 ),
    .gpio_cfg = {
        .mclk = GPIO_NUM_22,                    // pick the pins that are closest to the ADC without crossing PCB traces. 
        .bclk = GPIO_NUM_4,
        .ws = GPIO_NUM_2,
        .dout = I2S_GPIO_UNUSED,
        .din = GPIO_NUM_5,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = true,                     // we use AK5538 mode 32/36 (start on rising WS edge)
        },
    },
};

#if 1
// I2S Tx Config for the Receiver
static i2s_std_config_t tx_cfg = {
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        .mclk_multiple = I2S_MCLK_MULTIPLE_256  // Set MCLK multiple to 256
    },
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = GPIO_NUM_22,                    // pick the pins that are closest to the DAC without crossing PCB traces. 
        .bclk = GPIO_NUM_4,
        .ws = GPIO_NUM_2,
        .dout = GPIO_NUM_5,
        .din = I2S_GPIO_UNUSED,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = true,                     // we use AK4438 mode 14/18 (start on rising WS edge)
        },
    },
};
#endif

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


static IRAM_ATTR bool i2s_rx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // we pass the *dma_buf and size in a struct, by reference. 
    static dma_params_t dma_params;
    dma_params.dma_buf = event->dma_buf;
    dma_params.size = event->size;
#ifdef RX_DEBUG
    _log[p].loc = 1;
    _log[p].time = get_time_us_in_isr();
    _log[p].ptr = event->dma_buf;
    _log[p].size = event->size; 
    p++;
#endif
    // TODO either pass the dma_buf as (uint32_t), or we do the packing here and send the index of the udp_buf. 
    xTaskNotifyFromISR(i2s_rx_task_handle, (uint32_t)(&dma_params), eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken == pdTRUE);
}    

static void i2s_rx_task(void *args) {
    int i;
    uint32_t size;
    uint8_t *dma_buf;
    int loops = 0, led = 0;
    uint32_t evt_p;
    dma_params_t *dma_params;
    uint32_t nsamples = NSAMPLES; // default
    
#ifdef RX_DEBUG
    char t[][15] = {"", "ISR", "begin loop", "after notify", "channel_read", "packing", "udp send", "end loop" }; 
#endif
    
    // we get passed the *dma_buf and size, then pack, optionally checksum, and send. 
    while(1) {     
#ifdef RX_DEBUG
        _log[p].loc = 2;
        _log[p].time = get_time_us_in_isr();
        p++;
#endif        
        xTaskNotifyWait(0, ULONG_MAX, &evt_p, portMAX_DELAY);
        // convert back to pointer
        dma_params = (dma_params_t *)evt_p;
        // and fetch pointer and size
        dma_buf = dma_params->dma_buf;
        size = dma_params->size;
#ifdef RX_DEBUG        
        _log[p].loc = 3;
        _log[p].time = get_time_us_in_isr();
        _log[p].ptr = dma_buf;
        _log[p].size = size;
        p++;
#endif
        // read DMA buffer and pack
        // instead of using i2s_channel_read() which uses the FreeRTOS queue infrastructure internally 
        // and thus is not low latency capable, we access the last written DMA buffer directly. 
        // we can assume the passed data block is always a complete frame because
        // otherwise the DMA code would not have called the on_recv callback to begin with
        // but just in case it is less ... otherwise take NSAMPLES. 
        nsamples = size / (NUM_SLOTS * SLOT_WIDTH / 8);
        // ESP_LOGI(TAG, "nsamples = %lu", nsamples);
        for (i=0; i<nsamples; ++i) { 
            memcpy(udpbuf+3*i, dma_buf+4*i, 3); 
        }
#ifdef RX_DEBUG        
        _log[p].loc = 5;
        _log[p].time = get_time_us_in_isr(); 
        p++;
#endif    
        // insert S1, S2
        
        // checksumming
        
        // send. 

        // ESP_LOGI(TAG, "p = %d", p);
#ifdef RX_DEBUG
        if (p >= NUM) {
            i2s_channel_disable(rx_handle); // also stop all interrupts
            int q; 
            uint32_t curr_time, prev_time=0;
            for (q=0; q<p; q++) {
                switch (_log[q].loc) {
                    case 1:         // ISR
                        curr_time = _log[q].time;
                        printf("%15s time %lu diff %lu dma_buf 0x%08lx size %lu \n", 
                                t[_log[q].loc], _log[q].time, 
                                curr_time - prev_time,
                                (uint32_t) _log[q].ptr, _log[q].size);
                        prev_time = curr_time;
                        break; 
                    default:
                        printf("%15s time %lu diff %lu dma_buf 0x%08lx size %lu \n", 
                                t[_log[q].loc], _log[q].time, _log[q].time-_log[q-1].time, (uint32_t)_log[q].ptr, _log[q].size);
                        break;
                }                        
            }
            vTaskDelete(NULL); 
        }
#endif
        // blink LED
        loops = (loops + 1) % (SAMPLE_RATE / NSAMPLES);
        if (loops == 0) {
            led = (led + 1) % 2;
            gpio_set_level(LED_PIN, led);
        }
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
        // initialize Wifi STA and UDP
        // enable_network_sender();
        
        // set up I2S receive channel on the Sender
        i2s_new_channel(&rx_chan_cfg, NULL, &rx_handle);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_tdm_mode(rx_handle, &rx_cfg);

        // create UDP sender task
        xTaskCreate(i2s_rx_task, "i2s_rx_task", 4096, NULL, 5, &i2s_rx_task_handle);

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
        i2s_new_channel(&tx_chan_cfg, &tx_handle, NULL);
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


