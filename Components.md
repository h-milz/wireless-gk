# Components

  * MCU: an [ESP32-C5](https://www.espressif.com/en/news/ESP32-C5) module with u.fl connector for an external antenna. The chip was announced in spring 2022 but is not available yet. According to the little information there is, this MCU sports two RISC-V cores one of which runs at 240 MHz, the other at 20 MHz (ultra-low-power), and it supposedly support 5 GHz WiFi6 (i.e. 802.11ax). This CPU is faster than the S3 series, and Espressif seems to be ditching Xtensa-L7 in favor of RISC-V anyway. Again, this chip is not available yet, and until it is, development will take place with its sibling, the C6 (also WiFi6, but only 2.4 GHz). The ESP32-C6 has a hardware limitation which supports 8 I2S/TDM slots only in 16 bit resolution, 5 slots at 24 bit and 4 slots at 32, and it has only one I2S controller compared to the two of the ESP32-S3 which has the same limitation otherwise. What is already visible in the ESP-IDF source code suggests that the ESP32-C5 will have no such limitation. 
     
  * ADC: AKM [AK5538VN](https://www.akm.com/eu/en/products/audio/audio-adc/ak5538vn/) 8-ch audio ADC which is [I2S / TDM](https://en.wikipedia.org/wiki/I%C2%B2S) capable. Sadly, it is not in stock at my favourite PCB maker. 

  * DAC: AKM [AK4458VN](https://www.akm.com/eu/en/products/audio/audio-dac/ak4458vn/) 8-ch audio DAC (same sourcing issue as with the 5538).  

  * a couple of low-noise op-amps like NE5532 or NJM2068.

  * some power management chips for LiPo charging and creating supply voltages.
 
Primarly for marketing reasons, everything will work with 24 bit resolution, 44.1 kHz sample rate. The VG-99 and the GR-55 work with these parameters, and it would be hard to advertise a device that does less, although from a technical standpoint 16/32 or 16/36 would probably be enough. The downside to this is, it will require a higher over-the-air bandwidth. 
