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

#define MAXLEN 256
#define NSAMPLES 60
#define SLOTSIZE 8
#define NMAX 100000
const char *message = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static const char *TAG = "wirelessGK";

static uint8_t i2sbuf[4 * SLOTSIZE * NSAMPLES];
static uint8_t xferbuf[3 * SLOTSIZE * NSAMPLES];


void app_main(void) {
    int i, n;
    int64_t start_time, stop_time;
    float elapsed_time;
    
    ESP_ERROR_CHECK(nvs_flash_init());

    strncpy ((char *)xferbuf, message, sizeof(xferbuf));   // this is our received USB data
    strncpy ((char *)i2sbuf, message, sizeof(i2sbuf));     // this will be sent to the DAC
    
    // first variant: we null the whole xferbuf and then copy the blocks. 
    // each trailing byte per slot will remain null.    
    // ESP_LOGI (TAG, "message: %s", message);
    ESP_LOGI (TAG, "testing first variant");
    start_time = esp_timer_get_time();
    for (n=0; n<NMAX; n++) {
        memset (i2sbuf, 0x20, sizeof(i2sbuf));
        for (i=NSAMPLES-1; i>=0; --i) {   
            memcpy(i2sbuf+4*i, xferbuf+3*i, 3); 
        }
    }
    stop_time = esp_timer_get_time();
    elapsed_time = (stop_time - start_time) / 1000; 
    ESP_LOGI (TAG, "time = %.1f ms", elapsed_time);
    ESP_LOGI (TAG, "result: %30s", (char *)i2sbuf); 
    
    // second variant: we copy the bytes to the xferbuf and null each fourth byte while going. 
    // for 100k loops, this is slightly faster, 1804 vs. 1873 ms on ESP32-S3 and 907 vs. 1252 ms on C6. 
    // which means 18µs / 9 µs minus the loop iteration per packet. 
    // a small price for a 25% UDP bandwidth reduction. 
    ESP_LOGI (TAG, "testing second variant");
    start_time = esp_timer_get_time();
    for (n=0; n<NMAX; n++) {
        for (i=NSAMPLES-1; i>=0; --i) {   
            memcpy(i2sbuf+4*i, xferbuf+3*i, 3); 
            i2sbuf[4*i+3] = 0x20;
        }
    }
    stop_time = esp_timer_get_time();
    elapsed_time = (stop_time - start_time) / 1000; 
    ESP_LOGI (TAG, "time = %.1f ms", elapsed_time);
    ESP_LOGI (TAG, "result: %30s", (char *)i2sbuf); 

    // we will fetch the S1 / S2 bits like this: 
    // i.e. the two trailing bits of the buffer. 
    // int s1 = xferbuf & 0x01;
    // int s2 = xferbuf & 0x02;
    // we _could_ keep them from being sent to the GKVOL slot like so: 
    // i2sbuf[4*(NSAMPLES-1)+2] = 0;  // masking out the entire 3rd byte of the value, leaving 16 bit resolution. (1) 
    // but maybe not. 
    
    
}


