#ifndef 

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
