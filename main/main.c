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
#define DMA_BUFFER_COUNT        12                       // Number of DMA buffers
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

// static volatile uint32_t sample_count = 0; // , txcount = 0, rxcount = 0, losses = 0, loopcount = 0, overall_losses = 0, overall_packets = 0;

static uint8_t i2sbuf[4 * NUM_SLOTS * NSAMPLES];
static uint8_t udpbuf[3 * NUM_SLOTS * NSAMPLES];

static i2s_chan_handle_t rx_handle = NULL;
// static i2s_chan_handle_t tx_handle = NULL;

static TaskHandle_t udp_tx_task_handle; 

// The channel config is the same for both. 
static i2s_chan_config_t chan_cfg = {        // I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    .id = I2S_NUM_AUTO,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = DMA_BUFFER_COUNT,
    .dma_frame_num = NSAMPLES, 
    .auto_clear_after_cb = false, 
    .auto_clear_before_cb = false, 
    .allow_pd = false, 
    .intr_priority = 5, 
};       

// I2S Rx config for the sender
static i2s_tdm_config_t rx_cfg = {
    .clk_cfg = {                                // I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .sample_rate_hz = SAMPLE_RATE,
        .clk_src = I2S_CLK_SRC_DEFAULT,         // we will use I2S_CLK_SRC_EXTERNAL !
        .mclk_multiple = I2S_MCLK_MULTIPLE_512,  // Set MCLK multiple to 256
    },
    .slot_cfg = I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO,
                                                I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
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

DRAM_ATTR volatile int p = 0; 

#define NUM 20
typedef struct { 
    uint8_t loc;        // location
    uint32_t time;      // timestamp in µs
    uint8_t *ptr;       // pointer to buffer
    size_t size;        // data size
    uint32_t uint32ptr;       // converted pointer to buffer
} log_t; 

DRAM_ATTR volatile log_t _log[NUM];   

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


static IRAM_ATTR bool i2s_rx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // gpio_set_level(SIG2_PIN, 1);
    // gpio_set_level(SIG2_PIN, 0);
    _log[p].loc = 1;
    _log[p].time = get_time_us_in_isr();
    _log[p].ptr = event->dma_buf;
    _log[p].size = event->size; 
    _log[p].uint32ptr = (uint32_t) _log[p].ptr;
    p++;
    // TODO either pass the dma_buf as (uint32_t), or we do the packing here and send the index of the udp_buf. 
    xTaskNotifyFromISR(udp_tx_task_handle, _log[p].uint32ptr, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken == pdTRUE);
}    


static void udp_tx_task(void *args) {
    size_t size;
    uint8_t *dma_buf;
    uint8_t *ptr;
    uint32_t uint32ptr;
    char t[][15] = {"", "ISR", "begin loop", "channel_read", "packing", "udp send", "end loop" }; 
    
    // we get passed the *dma_buf and size, then pack, optionally checksum, and send. 
    while(1) {
        xTaskNotifyWait( 0, 
                         ULONG_MAX,
                         &uint32ptr,
                         portMAX_DELAY);
        // pass the dma ptr as uint32_t
        ptr = (uint8_t *)uint32ptr;
        // fetch the values from 1 tick earlier
        dma_buf = _log[p-1].ptr;
        size = (size_t)_log[p-1].size; 
        // we're there. 
        _log[p].loc = 5;
        _log[p].time = get_time_us_in_isr();
        _log[p].ptr = dma_buf;
        _log[p].size = size;
        _log[p].uint32ptr = uint32ptr;
        p++;
        // packing
        
        // checksumming
        
        // send. 

        // ESP_LOGI(TAG, "%s p = %d", __func__, p);
        // printf ("%s ptr size %d\n", __func__, sizeof(ptr)); 
        if (p >= NUM) {
            i2s_channel_disable(rx_handle); // also stop all interrupts
            int q; 
            for (q=0; q<p; q++) {
                switch (_log[q].loc) {
                    case 1:         // ISR
                        printf("%15s time %lu uptr 0x%08lx dma_buf 0x%08lx size %u \n", 
                                t[_log[q].loc], _log[q].time, _log[q].uint32ptr, (uint32_t) _log[q].ptr, _log[q].size);
                        break; 
                    case 5:         // UDP task
                        printf("%15s time %lu diff %lu uptr 0x%08lx dma_buf 0x%08lx size %u \n", 
                                t[_log[q].loc], _log[q].time, _log[q].time-_log[q-1].time, 
                                _log[q].uint32ptr, (uint32_t) _log[q].ptr, _log[q].size);
                        break;
                    default:
                        break;                        
                
                }                        
            }
            vTaskDelete(NULL); 
        }

    
        
    
    }    

}

#include "i2s_private.h" 

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

    init_hardware_timer();

    if (sender) {
        // initialize Wifi STA and UDP
        // enable_network_sender();
        
        // set up I2S receive channel on the Sender
        i2s_new_channel(&chan_cfg, NULL, &rx_handle);
        // this will later be i2s_channel_init_tdm_mode(). 
        i2s_channel_init_tdm_mode(rx_handle, &rx_cfg);

        // create UDP sender task
        xTaskCreate(udp_tx_task, "udp_tx_task", 4096, NULL, 5, &udp_tx_task_handle);
        
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
        
        // determine some settings. 
        uint32_t bufsize = i2s_get_buf_size(rx_handle, SLOT_WIDTH, NSAMPLES);
        printf ("rx_chan bufsize = %lu\n", bufsize);
/*        
lib/esp-idf/components/esp_driver_i2s/i2s_common.c:484:esp_err_t i2s_alloc_dma_desc(i2s_chan_handle_t handle, uint32_t num, uint32_t bufsize)
lib/esp-idf/components/esp_driver_i2s/i2s_common.c:1383:uint32_t i2s_sync_get_fifo_count(i2s_chan_handle_t tx_handle)
lib/esp-idf/components/esp_driver_i2s/i2s_private.h:151:struct i2s_channel_obj_t {
SOC_I2S_SUPPORTS_APLL
lib/esp-idf/components/esp_driver_i2s/i2s_private.h:127:} i2s_dma_t;
*/        
        printf ("desc_num: %lu, frame_num: %lu, buf_size: %lu\n", 
                rx_handle->dma.desc_num, rx_handle->dma.frame_num, rx_handle->dma.buf_size);

        
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
    // vTaskDelete(NULL);
    while (1) {
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}


