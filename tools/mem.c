    uint32_t start, stop, diff; 
    int i; 

    // ein paar einfache Tests
    
    udpbuf[0] = (uint8_t *)heap_caps_calloc(UDP_BUF_SIZE, sizeof(uint8_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL); 
    udpbuf[1] = (uint8_t *)heap_caps_calloc(UDP_BUF_SIZE, sizeof(uint8_t), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL); 
    udpbuf[2] = (uint8_t *)heap_caps_calloc(UDP_BUF_SIZE, sizeof(uint8_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM); 
    udpbuf[3] = (uint8_t *)heap_caps_calloc(UDP_BUF_SIZE, sizeof(uint8_t), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM); 
    
    // fill 2 buffers with "random" data
    int n = 100000; 
    for (i=0; i<UDP_BUF_SIZE; i++) {
         udpbuf[0][i] = i;    
         udpbuf[2][i] = i;    
    }
    start = get_time_us_in_isr(); 
    for (i=0; i<n; i++) {
        memcpy (udpbuf[1], udpbuf[0], UDP_BUF_SIZE);
    }
    stop = get_time_us_in_isr(); 
    ESP_LOGI(TAG, "time DRAM - DRAM= %lu µs", (stop - start)); 
    
    start = get_time_us_in_isr(); 
    for (i=0; i<n; i++) {
        memcpy (udpbuf[3], udpbuf[0], UDP_BUF_SIZE);
    }
    stop = get_time_us_in_isr(); 
    ESP_LOGI(TAG, "time DRAM - SPIRAM= %lu µs", (stop - start)); 

    start = get_time_us_in_isr(); 
    for (i=0; i<n; i++) {
        memcpy (udpbuf[1], udpbuf[2], UDP_BUF_SIZE);
    }
    stop = get_time_us_in_isr(); 
    ESP_LOGI(TAG, "time SPIRAM - DRAM= %lu µs", (stop - start)); 

    start = get_time_us_in_isr(); 
    for (i=0; i<n; i++) {
        memcpy (udpbuf[3], udpbuf[2], UDP_BUF_SIZE);
    }
    stop = get_time_us_in_isr(); 
    ESP_LOGI(TAG, "time SPIRAM - SPIRAM= %lu µs", (stop - start)); 
    
    vTaskDelete(NULL); 



