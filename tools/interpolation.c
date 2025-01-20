    // testing interpolation
    uint32_t start;
    uint32_t stop; 
    int n, k;

    udpbuf[0] = (uint8_t *)heap_caps_calloc(1920, sizeof(uint8_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);     
    udpbuf[1] = (uint8_t *)heap_caps_calloc(1920, sizeof(uint8_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);     
    udpbuf[2] = (uint8_t *)heap_caps_calloc(1920, sizeof(uint8_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);     

    // Betrachten als uint32_t*
    uint32_t *buf0 = (uint32_t *)udpbuf[0];
    uint32_t *buf1 = (uint32_t *)udpbuf[1];
    uint32_t *buf2 = (uint32_t *)udpbuf[2];
    
    start = get_time_us_in_isr();
    for (n=0; n<100000; n++) {
        for (k=0; k < 1920/4; k++) {
            buf2[k] = (buf0[k] + buf1[k]) > 1;  // / 2
        }
    }
    stop = get_time_us_in_isr();
    ESP_LOGI(TAG, "time: %lu µs for %d iterations", stop - start, n+1);

    vTaskDelete(NULL);


