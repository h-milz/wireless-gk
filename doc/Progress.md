# Progress 

2025-01-04

 * Wifi6 with HT20 / MCS7_SGI works. 
 * WPA2/PSK with my FritzBox works. WPA3/PSK preferred but that seems not to work. No prio #1 task for now.
 * sending 2 or 4 channels over UDP to Linux works in principle but I fight with ENOMEM errors from the LWIP stack, losing packets due to socket restart. 

2025-01-03

 * working sender prototype for 4 or 8 channels, still without ADC or UDP, but that's next. 

2024-12-29

 * time measurement; buffer compression 32 -> 24 bit, to compare 2 potential algorithms

2ß24-12-28

 * rearranged git repo and added ESP-IDF master branch for ESP32-C5 preview

2024-12-26

 * ordered ESP32-C5 DevKit early samples from Espressif :-) They are supposed to arrived at the end of Jan 2025. 

2024-05-24

 * C6 boards arrived, first benchmarks measuring simple math performance in comparison to S2 and S3. 

2024-05-21

 * code stub for I2S setup and LRCK interrupt handling; measured interrupt latency

2024-05-20

 * code stubs for MD5 / SHA1 / SHA256 HMAC generation incl. time measurement; buffer compression 32 -> 24 bit

2024-05-19

 * code stubs for Wifi AP & web configuration UI 

2024-05-16

 * ADC and DAC breakouts arrived. 

2024-05-13

 * ADC input filter design done and documented. 

2024-05-05

 * ordered two ESP32-C6 DevKits, a TI PCM1808 ADC and a TI PCM5102 DAC breakout board each.

