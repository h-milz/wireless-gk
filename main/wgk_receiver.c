/*
 * bla 
 */
 
 
#include "wireless_gk.h"

// #define RX_DEBUG 

#define LED_PIN                 GPIO_NUM_15             // 
#define SETUP_PIN               GPIO_NUM_17             // take the one that is nearest to the push button
#define SIG_PIN                 GPIO_NUM_6
#define SIG2_PIN                GPIO_NUM_7



static const char *RX_TAG = "wgk_rx";

i2s_chan_handle_t i2s_tx_handle = NULL;
TaskHandle_t i2s_tx_task_handle = NULL; 
TaskHandle_t udp_rx_task_handle = NULL; 

IRAM_ATTR bool i2s_tx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
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
    xTaskNotifyFromISR(i2s_tx_task_handle, (uint32_t)(&dma_params), eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken == pdTRUE);
}    


void i2s_tx_task(void *args) {
    int i;
    uint32_t size;
    uint8_t *dma_buf;
    uint32_t loops = 0, led = 0;
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
        dma_params = (dma_params_t *)evt_p;    // könnte man sparen, wenn man eine union nehmen würde. 
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
        // TODO read DMA buffer and unpack
        // nsamples = size / (NUM_SLOTS * SLOT_WIDTH / 8);
        // ESP_LOGI(RX_TAG, "nsamples = %lu", nsamples);
        for (i=0; i<nsamples; ++i) { 
            memcpy(udpbuf+3*i, dma_buf+4*i, 3); 
        }
#ifdef RX_DEBUG        
        _log[p].loc = 5;
        _log[p].time = get_time_us_in_isr(); 
        p++;
#endif    
        // extract S1, S2
        
        // checksumming
        
        // send. 
        xTaskNotifyFromISR(udp_rx_task_handle, size, eSetValueWithOverwrite, NULL);
            
        // ESP_LOGI(RX_TAG, "p = %d", p);
#ifdef RX_DEBUG
        if (p >= NUM) {
            i2s_channel_disable(i2s_rx_handle); // also stop all interrupts
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



void init_wifi_rx(void) {
    // we are WiFi AP 
    // TODO and provide WPS in setup mode
    // TODO check if credentials are available in nvs, if not, create new ones and store in nvs. 


    while (1) {
    
    }

}



void udp_rx_task(void *pvParameters) {


    while (1) {
    
    }

}


bool init_gpio_rx (void) {
    bool setup_needed = false; 
    
    // check if SETUP is pressed and then enter setup mode.  
    gpio_reset_pin(SETUP_PIN);
    gpio_set_direction(SETUP_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SETUP_PIN, GPIO_PULLUP_ONLY);
    gpio_pullup_en(SETUP_PIN);
    if (gpio_get_level(SETUP_PIN) == 0) {
        setup_needed = true; 
    } 
    gpio_reset_pin(SETUP_PIN);

    // TODO Pins for S1 and S2
    // can be different for Rx and Tx depending on the PCB layout. 
    // can we read/write these using the ULP in the background every, say, 10 ms? 
            
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
    // Returns true if setup was pressed
    return (setup_needed); 
}


void rx_setup (void) {
    // TODO start actual setup mode
    // on Rx, this means 
    // - generate new WiFi credentials
    // - store them in NVS
    // - start AP & WPS
}

