# Wireless-GK

(last edit: May 3rd, 2024)

Wireless-GK is a project which aims at developing a wireless alternative for Roland's famed (huh ...) GK cables. These cables are used to connect electric guitars which are equipped with a Roland GK-2A or GK-3 hexaphonic pickup to a Roland guitar synthesizer or modeller (e.g. GR-33, GR-55, VG-99 or any other of these models).  If you came here you probably know what you are looking for but anyway, if not, here's where you can get basic [information](https://en.wikipedia.org/wiki/Guitar_synthesizer#Roland_GK_interface). The cable consists of two 13-pin DIN connectors and a 13-wire cable. See the GK-3 schematics (sorry, this is the exact same blurred Roland sourced schematics that has been circulating forever...) to see the signals and wire assignments. 

# Previous Attempts and Developments

My first thoughts about a Wireless GK interface date back to the early 2000's. We did not have any compact microcontrollers and no WiFi back then, so what I was thinking about was a sender consisting of four 2-channel low-power audio ADCs, a 8-input controller packing the audio data into a data  stream, and a normal video transmitter.  On the receiving side, another controller would unpack the data stream to 4 stereo DACs. In principle.  Here's my old web page with the [ramble](https://www.muc.de/~hm/music/Wireless-GK/).

I did not pursue the idea for a number of years but in April 2024, I stumbled across a [Youtube video](https://www.youtube.com/watch?v=Ek9ydo4c_C4) by a Czech guy named "rockykoplik" demoing a device he had prototyped. Sadly, this project seems to have been abandoned in the meanwhile. I could not find any specification or even the general idea how he did it.  He attempted to collect 10,000 USD for the FCC/EC certification and needed 350 participants but there did not seem to be enough interest although there has been enough talk about wireless GK in the [VGuitarForums](https://www.vguitarforums.com). 

This triggered my wish to restart my former idea with more recent components, like 

  * Unexpected Maker's [Feather S3](https://esp32s3.com/feathers3.html) boards for sender and receiver. This board has a couple of advantages over generic ESP32-S3 boards: A JST-PH connector and LiPo battery charger; a second 3.3V LDO which keeps supply voltages separate and clean; a u.fl WiFi antenna connector which allows you to attach an external antenna for better signal quality without an [external antenna mod](https://www.youtube.com/watch?v=o4mtrueU6eM). 

  * sender: an AKM [AK5538VN](https://www.akm.com/eu/en/products/audio/audio-adc/ak5538vn/) 8-channel audio ADC which is [I2S / TDM](https://en.wikipedia.org/wiki/I%C2%B2S) capable. Sadly, it is not in stock at my favourite PCB maker (see **Building Prototypes** below). An 8-ch. alternative would be Cirrus Logic's CS5368 but this part uses 2 power supply voltages and much more power. Their better (low power, single supply) successor part CS5308P was announced in summer 2023 but it wasn't even available as samples in April 2024. Another alternative would be two TI PCM1840's but they are not natively cascadable and we would have to cascade their data in software.

  * receiver: an AKM [AK4438VN](https://www.akm.com/eu/en/products/audio/audio-dac/ak4438vn/) 8-ch audio DAC (same sourcing problem as with the 5538).  Alternatively, a [Texas Instruments / Burr-Brown PCM1691](https://www.ti.com/product/de-de/PCM1681-Q1). 

  * a couple of low-noise op-amps like TL074 or OPA4134 or any other low-noise type with the same standard footprint. Or maybe the same NJM2068D's that Roland uses. 

  * some power management chips creating supply voltages.
 
  * ultra low-power LCD or e-ink displays (SPI or I2C) for the simple user interface 

Primarly for marketing reasons, everything will work with 24 bit resolution, 44.1 kHz sample rate. The VG-99 and the GR-55 work with these parameters, and it would be hard to advertise a device that does less, although from a technical standpoint 16/32 or 16/36 would probably be enough. The downside to this is, it will require a higher over-the-air bandwidth (see **Alternative Approaches** below). 


##About Using WiFi

The combo will be using a private WiFi (with an arbitrary settable SSID) with the receiver as the AP and websocket server and the sender as the client, without a router in between. The WiFi password will probably be the same hardcoded random string. 

The net WiFi data rate is 8 channels x 44.1 kHz x 32 byte (I2S TDM data format) = 11289600 bps, which is within the documented 20 Mbps TCP throughput provided by the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32s3/api-guides/WiFi.html) but not leaving very much redundancy to play with. Processing time permitting, I may compress the buffers from 32 bits per slot (of which the last 8 bit are unused in the TDM format) to 24.   

Using websockets over TCP will reduce the typical HTTP latency, but provide lost / out-of-sequence packet handling and checksumming on the TCP layer. Granted, one could use UDP which is said to provide 30 Mbps but then we would have to roll our own error handling, creating complexity and new latency. 

Speaking if which, with a private P2P WiFi and  under reasonably good environmental conditions we should end up with a latency of 5-15 ms. This leaves to be measured which is the first step I'll do as soon as the FeatherS3 boards arrive (i.e. raise a GPIO in the sender, then send 32 byte packets one way, then on the receiver pick up the packet and raise a GPIO. Measure the time between the GPIOs with my oscilloscope).  



At the end of the day you need to choose between the devil and the deep blue sea I suppose. I'm all open for plausible proposals for a better (and technically feasible) technical design. 



At first-order approximation, the basic functionality will look like this: 



##Sender

  * the sender will be the WiFi client in station mode connecting to the server via websocket.
  * for the pairing, the user must trigger a WiFi scan on the receiver and select the desired  SSID which will stored permanently but can be changed any time later.  
  * the signals E1 - E6 from the hex pickup and the normal guitar signal will be sampled by the ADC.
  * the two GK switches will be read via GPIO, the GK VOL voltage will be AD converted by an internal ADC of the ESP32 every, say, 100ms in the background (on the second MCU core), and together they will be inserted in slot #7 of the TDM data stream
  * one ESP32-S3 I2S channel (I2S0) will work as I2S master, generating all the required clocks. Each data packet in the TDM frame will be 32 bytes long, but we may pack them into 24 to reduce the over-the-air data rate. 
  * The WS / FSYNC / LRCK pin of I2S0 will be used as an interrupt source for an edge-triggered GPIO. On the appropriate edge (TDM frame start), it will trigger an interrupt service routine which will set a "data ready" flag, and then the main routine can immediately pick up the I2S DMA buffer from the previous conversion cycle without any further latency (well, maybe a microsecond).  
  * as soon as the TDM packets are read, they will merged to one, plus the GK switch bits and the GKVOL value sitting around, and the resulting TDM8 frame will be WiFi'd to the receiver. 
  * Power: a single LiPo battery of about 2500 mAh or larger if I can get one. One could use 18650 cells for example, as long as they have a JST-PH connector for the FeatherS3. Charging and battery voltage measurement will be handled by the FeatherS3 board.  A charge pump and an inverter with LDOs behind them will generate +/- 7V directly from the LiPo battery for the op-amps and the GK-2A or GK-3. The ADCs will be powered by the secondary LDO on the FeatherS3 board. 

##Receiver

  * the receiver will work as a WiFi AP and create the private WiFi and a [websocket](https://en.wikipedia.org/wiki/WebSocket) server. Websockets are TCP based without the normal HTTP overhead, so you get checksumming, packet re-sending and packet sequencing for free. 
  * for the pairing, the user will have to set an unsuspicious WiFi AP SSID and store it. You can change the name any time later. This has no other function than allowing you to run several of these devices within WiFi range, e.g. on a stage. The WiFi password will probably be a hardcoded random string.
  * when receiving a websocket event, the first 7 TDM slots (GK1-6 and normal guitar) are written to the PCM1681 DAC via I2S. The switch bits will be written to two open drain GPIOs. The GK-VOL value will be PWM'd to a simple low pass filter, restoring the voltage. 
  * Power: via USB-C for 2x 3.3V (generated by the FeatherS3),  and by the guitar synth which provides the usual +/-7V for the op-amps via the GK cable. Care will be taken that all supply voltages will be as quiet as possible by using additional LDOs where needed.


##Potential Extensions

I can imagine some extensions for this solution: 

  * [Roland US-20](https://www.roland.com/global/products/us-20/) functionality in the receiver. It would be trivial to add a second or more 13-pin outputs and switch between them, even using the GK-2A or switches, or extra switches in the sender.
  * [RMC fanout box](https://www.rmcpickup.com/fanoutbox.html) functionality in the receiver. Trivial as well. 
  * [RMC subsonic filtering](https://www.joness.com/gr300/Filter-Buffer.htm) in the sender, removing the need for a VG-99 mod when using a piezo pickup equipped guitar. 

These extensions could be optional and pluggable via flat ribbon cables or something. More ideas welcome. In any case I envision a completely open architecture where anybody can add their stuff somehow. 



##Alternative Approaches

As written on my [old web page](https://www.muc.de/~hm/music/Wireless-GK/), it would generally be possible to avoid a microcontroller and a WiFi in the data path, hence reducing latency, and use a generic **ISM band video transmitter** instead. The data format could be AES3 / S/PDIF based which is ideally encoded (with differential Manchester or Biphase Mark (BP-M) encoding) for use on a DC free data link. The required bandwidth for BP-M is [twice the data rate](https://www.researchgate.net/figure/PSD-for-Manchester-Coding_fig15_45914350). Said video transmitters in the 2.4 or 5.8 GHz band provide a single channel for NTSC or PAL, and provide about 6.5 MHz of video bandwidth. This limits the data rate to about 3.2 Mbps. This is just sufficient for 3 PCM streams in 44.1 kHz / 24 bit format = 3175200 bps. Which means we would need two digital interface transmitters (DITs) like TI's PCM9211 and two video transmitters on the sender, and vice versa on the receiver. But with 3+3 channels, we would have to either omit the normal guitar signal or temporarily deselect one of the GK signals as outlined on old Wireless-GK page. This makes things more expensive and requires much more power particularly on the sender. The downside is, such a construction does not provide any error correction except CRC checksumming in the S/PDIF transmitter. And everybody would have to roll their own video transmitters/receivers because FCC regulations are different everyware. Beware the cheap Chinese stuff from AliExpress for example. These things may work nicely but have no FCC certification anywhere. At least no vendor I've checked advertises anything like this. 
   
To keep things less complicated one could try to find a UHF transmitter with a higher bandwidth, say, 15 MHz, but I cannot find any at the moment, and they would not be free to use without an FCC license. Which may actually be what killed rockykoplik's project. (which is also why it does work using WiFi 802.11n in HT40 mode with a channel bandwidth of 40 MHz). 

So no, I do not think this is a viable alternative.  But feel free to propose ways to get this done. The part up to the input / output of the video transceivers should be trivial to design with the help of a low-power MCU like an Arm Cortex-M0 board. That might even work in CircuitPython because we would just shuffle a couple of bits and misuse the I2S port for ADC / DIT / DIR clock generation. 
  
An alternative to the ESP32-S3 would be a **Teensy 4.0** board which is MUCH faster but has no WiFi. One would have to attach a WiFi coprocessor (e.g. ESP8266 or ESP32 based) over UART or over SPI which does not necessarily make things faster or lower latency. 

Using a **5 GHz WiFi** controller would be a real option, a) because the 5 GHz band is usually much less congested and b) because it has many more channels at its disposal which helps to further reduce environmental influences. The RTL8720DN module is dirt cheap and provides 802.11n MCS0-7 HT40 with up to 135 or 150 (depending who you ask) Mbps. But it does not support I2S TDM in & out, and the main MCU in the module is an ARM Cortex-M33 200 MHz type, which should be about as fast as a single Xtensa X7 core at the same frequency and hence comparable to a ESP32-S2. Bummer. 

So yes, something like a slightly faster ESP32-something with 5 GHz WiFi would be something. Eventually. Or a successor to the RTL8720DN with a faster CPU core (e.g. Cortex-M7) and proper I2S/TDM support.   
  
Too bad that the Raspberry Pi Zero 2W does not support 5 GHz WiFi. Is has ample CPU power at a decent power requirement. Not sure about I2S/TDM support. Also a no-go. (Yes it runs a fully fledged Linux but booting it can be made reasonably fast using busybox init, and we could ship SD Card images for DIYers.) 


## Building Prototypes

In the past. I had various PCBs made and (pre-)assembled by [JLCPCB.com](https://jlcpcb.com/). They provide good quality at a decent price, and together with their partner [LCSC.com](https://www.lcsc.com/) they have a couple of 100,000 common parts on stock. (No I am not affiliated, just a satisfied customer.)  I will try to make this thing as rebuild-proof as possible. 

The sender will have to be connected to the guitar's GK pickup by a short GK cable, and the receiver to the synth. The shortest pre-manufactured GK cables I've seen so far (on eBay) were 2 m but maybe I'll find even shorter ones. On the other hand, soldering 1 m stubs should not be a major hassle albeit somewhat fiddly. But I did that 20 years ago by cutting 5m cables and soldering a second 13-pin male connector at each half and I'm still alive. 




# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 



