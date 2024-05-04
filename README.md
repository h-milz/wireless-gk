# Wireless-GK

(last edit: May 4th, 2024)

Wireless-GK is a project which aims at developing a wireless alternative for Roland's famed (huh ...) GK cables. These cables are used to connect electric guitars which are equipped with a Roland GK-2A or GK-3 hexaphonic pickup to a Roland guitar synthesizer or modeller (e.g. GR-33, GR-55, VG-99 or any other of these models).  If you came here you probably know what you are looking for but anyway, if not, here's where you can get basic [information](https://en.wikipedia.org/wiki/Guitar_synthesizer#Roland_GK_interface). The cable consists of two 13-pin DIN connectors and a 13-wire cable. See the GK-3 schematics (sorry, this is the exact same blurred Roland sourced schematics that has been circulating forever...) to see the signals and wire assignments. 

# Previous Attempts and Developments

My first thoughts about a Wireless GK interface date back to the early 2000's. We did not have any compact microcontrollers and no WiFi back then, so what I was thinking about was a sender consisting of four 2-channel low-power audio ADCs, a 8-input controller packing the audio data into a data  stream, and a normal video transmitter.  On the receiving side, another controller would unpack the data stream to 4 stereo DACs. In principle.  Here's my old web page with the [ramble](https://www.muc.de/~hm/music/Wireless-GK/).

I did not pursue the idea for a number of years but in April 2024, I stumbled across a [Youtube video](https://www.youtube.com/watch?v=Ek9ydo4c_C4) by a Czech guy named "rockykoplik" demoing a device he had prototyped. Sadly, this project seems to have been abandoned in the meanwhile. I could not find any specification or even the general idea how he did it.  He attempted to collect 10,000 USD for the FCC/EC certification and needed 350 participants but there did not seem to be enough interest although there has been enough talk about wireless GK in the [VGuitarForums](https://www.vguitarforums.com). 

This triggered my wish to restart my former idea with more recent components, like 

  * MCU: [ESP32-C6-DevKitC-1](https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/index.html) or better -1U (with u.fl connector for an external antenna to avoid hacking an [external antenna mod](https://www.youtube.com/watch?v=o4mtrueU6eM)). This thing sports a single RISC-V core running at 160 MHz. This sounds feeble but in reality is is [slightly faster](https://github.com/nopnop2002/esp-idf-benchmark) (at least in Dhrystones) than the ESP32-S3, and it supports 2.4 GHz [WiFi6](https://en.wikipedia.org/wiki/Wi-Fi_6) 802.11ax (up to HT20/MCS9) which was specifically engineered for IoT solutions in noisy environments. As soon as the [ESP32-C5](https://www.espressif.com/en/news/ESP32-C5) comes out and is supported by the ESP-IDF, it should be straightforward to simply do a pin-compatible upgrade to 240 MHz and 5 GHz WiFi6. 

  * ADC: AKM [AK5538VN](https://www.akm.com/eu/en/products/audio/audio-adc/ak5538vn/) 8-channel audio ADC which is [I2S / TDM](https://en.wikipedia.org/wiki/I%C2%B2S) capable. Sadly, it is not in stock at my favourite PCB maker (see **Building Prototypes** below). An 8-ch. alternative would be Cirrus Logic's CS5368 but this part uses 2 power supply voltages and much more power. Their better (low power, single supply) successor part CS5308P was announced in summer 2023 but it wasn't even available as samples in April 2024. 

  * DAC: AKM [AK4438VN](https://www.akm.com/eu/en/products/audio/audio-dac/ak4438vn/) 8-ch audio DAC (same sourcing problem as with the 5538).  Alternatively, a [Texas Instruments / Burr-Brown PCM1691](https://www.ti.com/product/de-de/PCM1681-Q1). 

  * a couple of low-noise op-amps like TL074 or OPA4134 or any other low-noise type with the same standard footprint. Or maybe the same NJM2068D's that Roland uses. 

  * some power management chips for LiPo charging and creating supply voltages.
 
  * ultra low-power LCD or e-ink displays (SPI or I2C) for the simple user interface 

Primarly for marketing reasons, everything will work with 24 bit resolution, 44.1 kHz sample rate. The VG-99 and the GR-55 work with these parameters, and it would be hard to advertise a device that does less, although from a technical standpoint 16/32 or 16/36 would probably be enough. The downside to this is, it will require a higher over-the-air bandwidth (see **Alternative Approaches** below). 


## A Word About Using WiFi

The combo will be using a private WiFi (with an arbitrary settable SSID) with the receiver as the AP and websocket server and the sender as the client, without a router in between and thus avoiding router latency. The WiFi password will probably be the same hardcoded random string. 

The net WiFi data rate is 8 channels x 44.1 kHz x 32 byte (I2S TDM data format) = 11289600 bps, which is well within the theoretical net data rate for WiFi6 / MCS9. (Although the [ESP-IDF says](https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32c6/api-guides/wifi.html) 20 Mbps for TCP and 30 for UDP - guess that's a copy&paste error from earlier architectures.) 

Using websockets over TCP will reduce the typical HTTP latency, but provide lost / out-of-sequence packet handling and checksumming on the TCP layer. Granted, one could use UDP which is said to provide 30 Mbps but then we would have to roll our own error handling, creating complexity and new latency. 

Speaking of which, with a private P2P WiFi and  under reasonably good environmental conditions we should end up with a latency of 5-15 ms. This remains to be measured which is the first step I'll do as soon as the development boards arrive (i.e. raise a GPIO in the sender, then send a couple hundred 32 byte packets one way, then on the receiver pick up the packets and raise a GPIO when finished. Measure the time between the GPIOs with my oscilloscope).  



At the end of the day you need to choose between the devil and the deep blue sea I suppose. I'm all open for plausible proposals for a better (and technically feasible) technical design. 



At first-order approximation, the basic functionality will look like this: 



##Sender

  * the sender will be the WiFi client in station mode connecting to the server via websocket.
  * for the pairing, the user must trigger a WiFi scan on the receiver and select the desired  SSID which will stored permanently but can be changed any time later.  There will be a small display and a couple of buttons with a simple UI for this.
  * the signals E1 - E6 from the hex pickup and the normal guitar signal will be sampled by the ADC.
  * the two GK switches will be read via GPIO, the GK VOL voltage will be AD converted by an internal ADC of the ESP32 every, say, 100ms in the background, and the data will be inserted in slot #7 of the TDM data stream, which is not used by audio data. 
  * the ESP32 I2S controller (I2S0) will work as I2S master, generating all the required clocks. Each data packet in the TDM frame will be 32 bytes long, but we may pack them into 24 to reduce the over-the-air data rate if we have spare CPU cycles. 
  * The WS / FSYNC / LRCK pin of I2S0 will be used as an interrupt source for an edge-triggered GPIO. The appropriate edge will trigger an interrupt service routine which will set a "data ready" flag, and then the main routine can immediately pick up the I2S DMA buffer from the previous conversion cycle without any further latency (well, maybe a microsecond which corresponds to ~12 BCLK cycles).  
  * as soon as the TDM DMA buffer is read, the GK switch bits and the GKVOL value will be copied to bytes 28-31, and the resulting 32-byte buffer will be WiFi'd to the receiver. 
  * Power: a single LiPo battery of about 2500 mAh or larger if I can get one. One could use 18650 cells for example. Charging and battery voltage measurement will be handled by an equivalent of Adafruit's [PowerBoost 1000C](https://learn.adafruit.com/adafruit-powerboost-1000c-load-share-usb-charge-boost/overview) board. A charge pump and an inverter with LDOs behind them will generate +/- 7V directly from the LiPo battery for the op-amps and the GK-2A or GK-3. The ADC analog supply will be generated from the LiPo via a separate LDO. 

##Receiver

  * the receiver will work as a WiFi AP and create the private WiFi and a [websocket](https://en.wikipedia.org/wiki/WebSocket) server. Websockets are TCP based without the normal HTTP overhead, so you get checksumming, packet re-sending and packet sequencing for free. 
  * for the pairing, the user will have to set an unsuspicious WiFi AP SSID and store it (again, There will be a small display and a couple of buttons with a simple UI for this). You can change the name any time later. This has no other function than allowing you to run several of these devices within WiFi range, e.g. on a stage. The WiFi password will probably be a hardcoded random string. Maybe I can do something with [Wi-Fi Easy Connect](https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32c6/api-guides/wifi.html#wi-fi-easy-connect-dpp) or WPS using a BlueTooth link. 
  * again, the ESP32 I2S controller (I2S0) will work as I2S master, generating all the required clocks.
  * when receiving a websocket event, the first 28 bytes of the received buffer (7 TDM slots for GK1-6 and normal guitar) are written to the DAC via I2S. Bytes 28-31 are be dissected and handled accordingly: The switch bits are written to two open drain GPIOs. The GK-VOL value is written to an PWM out to a simple low pass filter, restoring the voltage. 
  * Power: via USB-C for the ESP32 and via a separate LDO for the DAC, and by the guitar synth which provides the usual +/-7V for the op-amps via the GK cable. Care will be taken that all supply voltages will be as quiet as possible by using additional LDOs where needed.


##Potential Extensions

I can imagine some extensions for this solution: 

  * [Roland US-20](https://www.roland.com/global/products/us-20/) functionality in the receiver. It would be trivial to add a second or more 13-pin outputs and switch between them, even using the GK-2A or switches, or extra switches in the sender.
  * [RMC fanout box](https://www.rmcpickup.com/fanoutbox.html) functionality in the receiver. Trivial as well. 
  * [RMC subsonic filtering](https://www.joness.com/gr300/Filter-Buffer.htm) in the sender, removing the need for a VG-99 mod when using a piezo pickup equipped guitar. 

These extensions could be optional and pluggable via flat ribbon cables or something. More ideas welcome. In any case I envision a completely open architecture where anybody can add their stuff somehow. 



##Alternative Approaches

As written on my [old web page](https://www.muc.de/~hm/music/Wireless-GK/), it would generally be possible to avoid a microcontroller and a WiFi in the data path, hence reducing latency, and use a generic **ISM band video transmitter** instead. The data format could be AES3 / S/PDIF based which is ideally encoded (with differential Manchester or Biphase Mark (BP-M) encoding) for use on a DC free data link. The required bandwidth for BP-M is [twice the data rate](https://www.researchgate.net/figure/PSD-for-Manchester-Coding_fig15_45914350). Said video transmitters in the 2.4 or 5.8 GHz band provide a single channel for NTSC or PAL, and provide about 6.5 MHz of video bandwidth. This limits the data rate to about 3.2 Mbps. This is just sufficient for 3 PCM streams in 44.1 kHz / 24 bit format = 3175200 bps. Which means we would need two digital interface transmitters (DITs) like TI's PCM9211 and two video transmitters on the sender, and vice versa on the receiver. But with 3+3 channels, we would have to either omit the normal guitar signal or temporarily deselect one of the GK signals as outlined on old Wireless-GK page. This makes things more expensive and requires much more power particularly on the sender. The downside is, such a construction does not provide any error correction except CRC checksumming in the S/PDIF transmitter. And everybody would have to roll their own video transmitters/receivers because FCC regulations are different everyware. Beware the cheap Chinese stuff from AliExpress for example. These things may work nicely but have no FCC certification anywhere. At least no vendor I've checked advertises anything like this. 
   
To keep things less complicated one could try to find a UHF transmitter with a higher bandwidth, say, 15 MHz, but I cannot find any at the moment, and they would not be free to use without an FCC license. Which may actually be what killed rockykoplik's project. (which is also why it does work using WiFi 802.11n in HT40 mode with a channel bandwidth of 40 MHz). 

So no, I do not think this is a viable alternative.  But feel free to propose ways to get this done. The part up to the input / output of the video transceivers should be trivial to design with the help of a low-power MCU like an Arm Cortex-M0 board. That might even work in CircuitPython because we would just shuffle a couple of ADC / DAC / DIT / DIR configuration bits and misuse the I2S port for TDM clock generation. 
  

## Building Prototypes

In the past. I had various PCBs made and (pre-)assembled by [JLCPCB.com](https://jlcpcb.com/). They provide good quality at a decent price, and together with their partner [LCSC.com](https://www.lcsc.com/) they have a couple of 100,000 common parts on stock. (No I am not affiliated, just a satisfied customer.)  I will try to make this thing as rebuild-proof as possible. 

The sender will have to be connected to the guitar's GK pickup by a short GK cable, and the receiver to the synth. The shortest pre-manufactured GK cables I've seen so far (on eBay) were 2 m but maybe I'll find even shorter ones. On the other hand, soldering 1 m stubs should not be a major hassle albeit somewhat fiddly. But I did that 20 years ago by cutting 5m cables and soldering a second 13-pin male connector at each half and I'm still alive. 




# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 



