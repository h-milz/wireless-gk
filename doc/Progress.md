# Progress 

2025-02-12

 * much testing and optimizing. It turns out that at seemingly random intervals, outgoing datagrams in the sender pile up in the Wifi buffer before getting sent, and in these cases receptions stalls. This led to the pops and glitches I've been encountering. Also, the rate and length of stalls appears to depend on the CPU core temperature which goes up to about 39°C within half an hour (mind you, this is a pre-production sample where Espressif may work on not for the first, but maybe for the second production release). With a much larger Rx buffer and a different strategy of handling the stalls, I'm now down to 10 ms latency while stalls up to 10-20 ms are barely audible because I return silence when this happens. I can now listen music for hours without problems. I expect the synth to be able to handle the remaining gaps. I mean, on stage it's very noisy anyway ... 


2025-02-03

 * long measurement session to find sweet spots between sample rate, number of frames per packet, and the offset of the pointers in the ring buffer. It appears that at 44.1 kHz, 60 frames and an offset of 4 ist best, as far as packet errors. The remaining errors are mitigated by caching each valid sample so that in case of a packet error, the cached sample is used and the packets gaps are linearly interpolated to avoid discontinuities. Also, receiving a larger number of packets before starting replay helps. Defensively, the end-to-end latency is 11.4 ms. 
 * It turns out that the sample rates 46875 and 37500 Hz also work fine, which can be generated using an integer divider from any of the supported I2S clocks, avoiding clock jitter from the fractional divider. 44643 and 41667 Hz can also be used because their dividers have very small fractional parts when derived from the 160 MHz or 240 MHz PLLs, reducing clock jitter. (see [Components](doc/Components.md) for a discussion). 

2025-02-02
 
 * ok, after a lot of experimenting it appears I found a way to deal with the UDP jitter so that after listening to music using a Sennheiser studio headphone for several hours, I could not hear any glitches or pops except caused by excessive logging, although in the stats I can see occasional lost or misordered packets. 

2025-01-27

 * the new buffering strategy works basically. Optimization is next. 

2025-01-26

 * comprehensive simulation of the influence of interpolation. Please see the tools/interp.c and the tools/fft*.py files for clues. In a nutshell: even with a short interpolation over just 3 samples the effect of discontinuities on the spectrum can efficiently be mitigated. So that's the way to go. Implementation and live testing is next. Finding the sweet spot that also satisfies latency expectations. 

2025-01-25

 * changing the ring buffer architecture to insert incoming UDP packets by sequence number (i.e., send time) instead of arrival time. This should help a continuous packet flow. 
 * interpolation tests - if a packet goes lost, we need to duplicate the previous ring buffer element. Interpolation should help to prevent discontinuities and hence glitches. 

2025-01-24

 * comprehensive UDP latency testing on the current architecture. Plain UDP packet latency is about 540 +/- 200 µs. Occasionally, packets still arrive late (> 1400 µs) and cause glitches. In these cases, the simple ring buffer runs dry. 

2025-01-23

 * added UDP Rx ring buffer, lots of experimenting and optimization. Rx-Tx work now with very few glitches probably still caused by UDP jitter. 

2025-01-20

 * sample interpolation and circular buffer experiments

2025-01-16

 * live streaming between sender and receiver now. Listening to music, bridged between my smartphone and my hi-fi amp. Basically, this combo is now a generic wireless 8-channel 24/44 audio bridge. 48 kHz should work as well. Still, there are glitches. 

2025-01-14

 * live streaming audio to my Linux machine works using netcat and sox as a receiver. The data in the DMA buffer are signed integer 32 bit. This works with packing 32->24 bit and 2 stereo channels embedded in 8-slot frames.  The sound is pristine, no audible distortions, gaps or noise. I think this is the proof that the concept works. 

2025-01-13

 * First light! Audio transferred from the sender to my Linux box (sine test signals and music). As tested with my oscilloscope, data is sent by the ADC MSB first, and the last byte of each 32 bit sample is null. So that's ok. But the data in the DMA buffer is little endian, that is, LSB first. Sounds strange in the loudspeakers if you miss this one. I assume this makes no difference when pumping the data into the DAC in the same order. My Linux machine loses a lot of packets, though, which may be due to the udpserver being in Python. Got to rewrite it in C or use `netcat`. [This video](https://youtu.be/FOIhJvwpMKE) shows my experimental setup with the scope on the left displaying the word clock on top and the data bits below, and the ESP32-C5 DevKit and the ADC breakout board on the right.

2025-01-12

 * WPS works fine with WPA3 (esp-idf examples)
 * WPS pairing and normal operation work. 
 * automatic free channel search works. 

2025-01-10

The two boards now talk exclusively 11AX with WPA3-SAE in 5 GHz. According to iperf, the UDP data rate with a payload of 1440 bytes is around 63 MBit/s. YAY! 

    I (4195) wifi:security: WPA3-SAE, phy:11ax, rssi:-29, cipher(pairwise:0x3, group:0x3), pmf:1, 

OK, I think it's time for another latency measurement because that's now on a different platform. I send 1440 byte packets every second and pull an interrupt line on the sender, and I mesure the time from the interrupt until the packet actually arrives. This is the pure UDP latency. 

    I (29424) wgk_rx: latency: 823 µs
    I (30424) wgk_rx: latency: 805 µs
    I (31424) wgk_rx: latency: 800 µs
    I (32424) wgk_rx: latency: 813 µs
    I (33424) wgk_rx: latency: 801 µs
    I (34424) wgk_rx: latency: 797 µs
    I (35424) wgk_rx: latency: 793 µs
    I (36424) wgk_rx: latency: 791 µs
    I (37424) wgk_rx: latency: 739 µs
    I (38424) wgk_rx: latency: 800 µs

Add this to the 1.36 ms for 60 frame ADC plus 22.7 µs until the first sample arrives at the DAC, plus a couple of microseconds for the data processing, and there you go: 2.16 ms, say, 2,2 ms. Beat this! (OK, that's over a distance of about 20 cm, but light speed is light speed, even on a stage.) 
 
2025-01-09

 * Rx setup done, AP works
 * strangely, Rx and Tx negotiate only 11AN at HT20. Needs further invesigation. 
 * testing Rx strategies - should UDP packet reception time the thing, or the DMA on_sent event? 

2025-01-07

![ESP32-C5 Devkits](devkits.jpg)

 * Hooray! The C5 DevKits arrived! 
 * First tests are very promising. The device running at 160 or 240 MHz connects to my FritzBox using 5 GHz Wifi6 11AX just fine (well, most of the time - sometimes the AP offers only 11AC), and if it does, I see no ENOMEM induced packet losses with 8 slots at all. So I've got to figure out how to force the pair to always negotiate 11AX. I'll have it run overnight to see if it's stable over hours, and next is getting the receiver to work as Wifi6 11AX AP with WPA3. 
 * If it connects with 11AX, it's WIFI_PHY_RATE_MCS7_SGI in HT20 mode (the FritzBox says 81 MBit/s), and it authorizes using WPA3-SAE. I don't think it can get any better than that :-) . (Too bad the chip does not support HT40 with 11AX or 11AC but that's not relevant for this application.) 

2025-01-06

 * code split
 * rewrite of STA setup, new AP setup, new UDP_RX task

2025-01-05

 * overnight, the sender protoype has sent ~ 17 GB of (artificial) data to my Linux machine without any major problem. I still see occasional packet losses due to ENOMEM errors caused by UDP sendto(). The root cause is not a general lack of RAM by the way - the largest free block on the heap is constantly ~ 286k of 512k. With the C5 that has PSRAM, I need to make the LWIP buffers static anyway - we will see. 
 * when sending a continuous stream, the C6 pulls about 110-120 mA. The C5 will likely need a bit more due to the higher clock frequency. 

2025-01-04

 * Wifi6 with HT20 / MCS7_SGI works. The FritzBox reports the device as connected with 81 MBit/s. 
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


# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 

