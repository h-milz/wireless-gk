
#if 0
void i2s_rx_task(void *args) {
    int i, j;
    uint32_t size;
    uint8_t *dmabuf;
    uint32_t loops = 0, led = 0x00;
    uint32_t evt_p;
    dma_params_t *dma_params;
    uint32_t count = 0; 
    uint32_t checksum; 
    uint32_t num_bytes = UDP_BUF_SIZE - 2 * NUM_SLOTS_UDP * SLOT_SIZE_UDP;

    memset(udp_tx_buf, 0, UDP_PAYLOAD_SIZE);    
    // we get passed the *dma_buf and size, then pack, optionally checksum, and send. 
    while(1) {     
#ifdef TX_DEBUG
        _log[p].loc = 1;
        _log[p].time = get_time_us_in_isr();
        p++;
#endif        
        xTaskNotifyWait(0, ULONG_MAX, &evt_p, portMAX_DELAY);
        // convert back to pointer
        dma_params = (dma_params_t *)evt_p;    
        // and fetch pointer and size
        dmabuf = dma_params->dma_buf;
        size = dma_params->size;                    // size is not used, so we could just transfer the *dma_buf. 
#ifdef TX_DEBUG        
        _log[p].loc = 2;
        _log[p].time = get_time_us_in_isr();
        _log[p].ptr = dmabuf;
        _log[p].size = size;
        p++;
#endif
        // read DMA buffer and pack
#ifndef TX_TEST
        memcpy (udp_tx_buf, dmabuf, size);         
#else        
        // memset (udp_tx_buf, 0, UDP_BUF_SIZE);         // clean up first
        for (i=0; i<NFRAMES; i++) {
            for (j=0; j<NUM_SLOTS_I2S; j++) {                
                // the offset of a sample in the DMA buffer is (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1 
                // the offset of a sample in the UDP buffer is (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP
                memcpy (udp_tx_buf + (i * NUM_SLOTS_UDP + j) * SLOT_SIZE_UDP, 
                        dmabuf + (i * NUM_SLOTS_I2S + j) * SLOT_SIZE_I2S + 1,
                        SLOT_SIZE_UDP);
            }                    
        }
#endif
        // insert current packet count into the last slot. only 24 least significant bits
/*
        count = (count + 1) & 0x00FFFFFF; 
        memcpy ((uint32_t *)(udp_tx_buf + UDP_BUF_SIZE - 3 ),      
                &count,
                3); 
*/                 
        // insert XOR checksum after the sample data
        checksum = calculate_checksum((uint32_t *)udp_tx_buf, UDP_BUF_SIZE/4); 
        memcpy (udp_tx_buf + UDP_BUF_SIZE, &checksum, sizeof(checksum)); 
        // TODO insert S1, S2 in the last byte

#ifdef TX_DEBUG        
        _log[p].loc = 3;
        _log[p].time = get_time_us_in_isr(); 
        p++;
#endif    
        // insert S1, S2
        
        // checksumming
        
        // send. 
        xTaskNotifyGive(udp_tx_task_handle);
        // xTaskNotifyIndexed(udp_tx_task_handle, 0, size, eSetValueWithOverwrite);

        // ESP_LOGI(RX_TAG, "p = %d", p);
        // blink LED        
        loops = (loops + 1) % (SAMPLE_RATE / NFRAMES);
        if (loops == 0) {
            // led ^= 0x01; // toggle
            // gpio_set_level(LED_PIN, led);
            for (i=0; i<NUM_SLOTS_I2S; i++) {
                for (j=0; j<SLOT_SIZE_I2S; j++) {
                    printf ("%02x", dmabuf[i * SLOT_SIZE_I2S +j]);   
                }                    
                printf (" ");             
            }
            printf ("\n");
        }

    }    
}
#endif

