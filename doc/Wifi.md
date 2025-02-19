# WiFi Concept

In order to keep latency low and resilience against network congestion high, the combo uses a private peer-to-peer WiFi6 802.11AX network with WPA3 authentication on the 5 GHz band, using UDP as the transport protocol. 

The theoretical net WiFi data rate is 44100 frames per second * 256 bits per frame = 11.2896 Mbps, which is well within the range for [WIFI_PHY_RATE_MCS7_SGI](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/network/esp_wifi.html#_CPPv415wifi_phy_rate_t) -- in fact, I measured a maximum UDP throughput between 2 ESP32-C5 boards of around 63 MBit/s using iperf.

Sending 44100 packets of 32 bytes per second would be very bandwidth inefficient because the header is also 28 bytes, and it would overwhelm the processor due to the huge interrupt rate, causing packet losses. This can be mitigated by sending multiple frames in batches so that [IP framentation](https://en.wikipedia.org/wiki/IP_fragmentation) is just avoided. Fragmentation occurs when datagrams are larger than the Maximum Transmission Unit (MTU) which is commonly 1500 bytes including the TCP or UDP and IP headers. For UDP, the maximum payload avoiding fragmentation is 1472 bytes. This would allow to send 46 frames per datagram, 958 datagrams per second. 

On the other hand, we can reduce the data rate by reducing the 32 bit samples to 24 bit samples, which is enough (see [Components](doc/Components.md)). This way, we can send 60 frames per datagram, and 735 datagrams per second. The A/D conversion of 60 frames takes 60 * 22.7 µs = 1.36 ms, which is then the basic A/D conversion per-sample latency not counting the network latency. The net data rate in this case is 8.5 MBit/s. 

The measured plain UDP packet latency for a 1440 byte payload between the two boards proved to be in the 540 µs range. This means the overall latency can be expected to be less than 10 ms.  

If we go for 16 bit AD resolution as discussed in the [Components](Components.md) section, the effective data rate would shrink to 5.6 MBit/s. This would also help reducing the MCU temperature and power requirement in the sender a bit. 

At the end of the day you need to choose between the devil and the deep blue sea I suppose. I'm all open for plausible proposals for a better (and technically feasible) technical design. 


# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 
