# Wireless-GK

(last edit: May 6, 2024)

Wireless-GK is a project which aims at developing a wireless alternative for Roland's famed (huh ...) GK cables. These cables are used to connect electric guitars which are equipped with a Roland GK-2A or GK-3 hexaphonic pickup to a Roland guitar synthesizer or modeller (e.g. GR-33, GR-55, VG-99 or any other of these models).  If you came here you probably know what you are looking for but anyway, if not, here's where you can get basic [information](https://en.wikipedia.org/wiki/Guitar_synthesizer#Roland_GK_interface). The cable consists of two 13-pin DIN connectors and a 13-wire cable. See the GK-3 schematics (sorry, this is the exact same blurred Roland sourced schematics that has been circulating forever...) to see the signals and wire assignments. 

# Previous Attempts and Developments

My first thoughts about a Wireless GK interface date back to the early 2000's. We did not have any compact microcontrollers and no WiFi back then, so what I was thinking about was a sender consisting of four 2-channel low-power audio ADCs, a 8-input controller packing the audio data into a data  stream, and a normal video transmitter.  On the receiving side, another controller would unpack the data stream to 4 stereo DACs. In principle.  Here's my old web page with the [ramble](https://www.muc.de/~hm/music/Wireless-GK/).

I did not pursue the idea for a number of years but in April 2024, I stumbled across a [Youtube video](https://www.youtube.com/watch?v=Ek9ydo4c_C4) by a Czech guy named "rockykoplik" demoing a device he had prototyped. Sadly, this project seems to have been abandoned in the meanwhile. I could not find any specification or even the general idea how he did it.  He attempted to collect 10,000 USD for the FCC/EC certification and needed 350 participants but there did not seem to be enough interest although there has been enough talk about wireless GK in the [VGuitarForums](https://www.vguitarforums.com). 

This triggered my wish to restart my former idea with more recent components, like 

  * MCU: an [ESP32-C6](https://www.espressif.com/en/products/socs/esp32-c6)  based MCU with u.fl connector for an external antenna to avoid hacking an [external antenna mod](https://www.youtube.com/watch?v=o4mtrueU6eM)). This MCU sports two RISC-V cores one of which runs at 160 MHz, the other at 20 MHz (ultra-low-power). This sounds feeble but in reality is is [slightly faster](https://github.com/nopnop2002/esp-idf-benchmark) (at least in Dhrystones) than the ESP32-S3, and it supports 2.4 GHz [WiFi6](https://en.wikipedia.org/wiki/Wi-Fi_6) 802.11ax (up to HT20/MCS9) which was specifically engineered for IoT solutions in noisy environments. As soon as the [ESP32-C5](https://www.espressif.com/en/news/ESP32-C5) comes out and is supported by the ESP-IDF, it should be straightforward to simply do a pin-compatible upgrade to 240 MHz and 5 GHz WiFi6. 

  * ADC: AKM [AK5538VN](https://www.akm.com/eu/en/products/audio/audio-adc/ak5538vn/) 8-channel audio ADC which is [I2S / TDM](https://en.wikipedia.org/wiki/I%C2%B2S) capable. Sadly, it is not in stock at my favourite PCB maker (see **Building Prototypes** below). An 8-ch. alternative would be Cirrus Logic's CS5368 but this part uses 2 power supply voltages and much more power. Their better (low power, single supply) successor part CS5308P was announced in summer 2023 but it wasn't even available as samples in April 2024. 

  * DAC: AKM [AK4438VN](https://www.akm.com/eu/en/products/audio/audio-dac/ak4438vn/) 8-ch audio DAC (same sourcing problem as with the 5538).  Alternatively, a [Texas Instruments / Burr-Brown PCM1691](https://www.ti.com/product/de-de/PCM1681-Q1). 

  * a couple of low-noise op-amps like TL074 or OPA4134 or any other low-noise type with the same standard footprint. Or maybe the same NJM2068D's that Roland uses. 

  * some power management chips for LiPo charging and creating supply voltages.
 
Primarly for marketing reasons, everything will work with 24 bit resolution, 44.1 kHz sample rate. The VG-99 and the GR-55 work with these parameters, and it would be hard to advertise a device that does less, although from a technical standpoint 16/32 or 16/36 would probably be enough. The downside to this is, it will require a higher over-the-air bandwidth (see **Alternative Approaches** below). 


## A Word About Using WiFi

In order to reduce latency, the combo will be using a private peer-to-peer WiFi with one of these configs: 

 * with the receiver as the AP running a websocket server and the sender as the client, 
 * same, but running UDP instead of websockets
 * running ESP-NOW which was designed for high throughput and low latency. 

The net WiFi data rate is 8 channels x 44.1 kHz x 32 byte (I2S TDM data format) = 11289600 bps, which is well within the theoretical net data rate for WiFi6 / MCS9. (Although the [ESP-IDF says](https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32c6/api-guides/wifi.html) 20 Mbps for TCP and 30 for UDP - guess that's a copy&paste error from earlier architectures.) 



Speaking of which, with a private P2P WiFi and  under reasonably good environmental conditions we should end up with a latency of 5-15 ms. With pure UDP or ESP-NOW we should  be able to get this down to small single digits but would have to add some basic error detection like XOR checksumming and packet numbering. This remains to be tested which is the first step I'll do as soon as the development boards arrive (i.e. raise a GPIO in the sender, then send a couple hundred 32 byte packets one way, then on the receiver pick up the packets and raise a GPIO when finished. Measure the time between the GPIOs with my oscilloscope).  



At the end of the day you need to choose between the devil and the deep blue sea I suppose. I'm all open for plausible proposals for a better (and technically feasible) technical design. 



At first-order approximation, the basic functionality will look like this: 



## Sender

  * the sender will be the WiFi client in station mode connecting to the server via websocket.
  * for the pairing, the user will have to configure some WiFi options depending on the protocol and encryption via a simple web UI.  
  * the signals E1 - E6 from the hex pickup and the normal guitar signal will be sampled by the ADC.
  * the two GK switches will be read via GPIO, the GK VOL voltage will be AD converted by an internal ADC of the ESP32 every, say, 100ms in the background, and the data will be inserted in slot #7 of the TDM data stream, which is not used by audio data. 
  * the ESP32 I2S controller (I2S0) will work as I2S master, generating all the required clocks. Each data packet in the TDM frame will be 32 bytes long, but we may pack them into 24 to reduce the over-the-air data rate if we have spare CPU cycles. 
  * The WS / FSYNC / LRCK pin of I2S0 will be used as an interrupt source for an edge-triggered GPIO. The appropriate edge will trigger an interrupt service routine which will set a "data ready" flag, and then the main routine can immediately pick up the I2S DMA buffer from the previous conversion cycle without any further latency (well, maybe a microsecond which corresponds to ~12 BCLK cycles).  
  * as soon as the TDM DMA buffer is read, the GK switch bits and the GKVOL value will be copied to bytes 28-31, and the resulting 32-byte buffer will be WiFi'd to the receiver. 
  * Power: a single LiPo battery of about 2500 mAh or larger if I can get one. One could use 18650 cells for example. Charging and battery voltage measurement will be handled by an equivalent of Adafruit's [PowerBoost 1000C](https://learn.adafruit.com/adafruit-powerboost-1000c-load-share-usb-charge-boost/overview) board. A charge pump and an inverter with LDOs behind them will generate +/- 7V directly from the LiPo battery for the op-amps and the GK-2A or GK-3. The ADC analog supply will be generated from the LiPo via a separate LDO. 

## Receiver

  * the receiver will work as a WiFi AP or ESP-NOW peer 
  * for the pairing, the user will have to configure some parameters via a simple web UI. 
  * again, the ESP32 I2S controller (I2S0) will work as I2S master, generating all the required clocks.
  * when receiving a websocket event, the first 28 bytes of the received buffer (7 TDM slots for GK1-6 and normal guitar) are written to the DAC via I2S. Bytes 28-31 are be dissected and handled accordingly: The switch bits are written to two open drain GPIOs. The GK-VOL value is written to an PWM out to a simple low pass filter, restoring the voltage. 
  * Power: via USB-C for the ESP32 and via a separate LDO for the DAC, and by the guitar synth which provides the usual +/-7V for the op-amps via the GK cable. Care will be taken that all supply voltages will be as quiet as possible by using additional LDOs where needed.


## Potential Extensions

I can imagine some extensions for this solution: 

  * [Roland US-20](https://www.roland.com/global/products/us-20/) functionality in the receiver. It would be trivial to add a second or more 13-pin outputs and switch between them, even using the GK-2A or switches, or extra switches in the sender.
  * [RMC fanout box](https://www.rmcpickup.com/fanoutbox.html) functionality in the receiver. Trivial as well. 
  * [RMC subsonic filtering](https://www.joness.com/gr300/Filter-Buffer.htm) in the sender, removing the need for a VG-99 mod when using a piezo pickup equipped guitar. 

These extensions could be optional and pluggable via flat ribbon cables or something. More ideas welcome. In any case I envision a completely open architecture where anybody can add their stuff somehow. 



## Alternative Approaches

As written on my [old web page](https://www.muc.de/~hm/music/Wireless-GK/), it would generally be possible to avoid a microcontroller and a WiFi in the data path, hence reducing latency, and use a generic **ISM band video transmitter** instead. The data format could be AES3 / S/PDIF based which is differential Manchester or biphase-mark (BP-M) encoded for use on a DC free data link. The required bandwidth for BP-M is [twice the data rate](https://www.researchgate.net/figure/PSD-for-Manchester-Coding_fig15_45914350). Said video transmitters in the 2.4 or 5.8 GHz band provide a single channel for NTSC or PAL, and provide about 6.5 MHz of video bandwidth. This limits the data rate to about 3.2 Mbps. This is just sufficient for 3 PCM streams in 44.1 kHz / 24 bit format = 3175200 bps. Which means we would need two digital interface transmitters (DITs) like TI's PCM9211 and two video transmitters on the sender, and vice versa on the receiver. But with 3+3 channels, we would have to either omit the normal guitar signal or temporarily deselect one of the GK signals as outlined on old Wireless-GK page. This makes things more expensive and requires much more power particularly on the sender. The downside is, such a construction does not provide any error correction except CRC checksumming in the S/PDIF transmitter. And everybody would have to roll their own video transmitters/receivers because FCC regulations are different everyware. Beware the cheap Chinese stuff from AliExpress for example. These things may work nicely but have no FCC certification anywhere. At least no vendor I've checked advertises anything like this. 
   
To keep things less complicated one could try to find a UHF transmitter with a higher bandwidth, say, 25 MHz, but I cannot find any at the moment, and they would not be free to use without an FCC license. Which may actually be what killed rockykoplik's project. (which is also why it does work using WiFi 802.11n in HT40 mode with a channel bandwidth of 40 MHz).  On the other hand for an international corporation it would be easy to build such a transmitter and receiver and get the FCC / CE certification, e.g. the likes of Sennheiser. 

So no, I do not think this is a viable alternative for makers.  But feel free to propose ways to get this done. The part up to the input / output of the video transceivers should be trivial to design with the help of a low-power MCU like an Arm Cortex-M0 board. That might even work in CircuitPython because we would just shuffle a couple of ADC / DAC / DIT / DIR configuration bits and misuse the I2S port for TDM clock generation. 

Another rather theoretical variant would be a purely analog one: analog frequency multiplex. For each of the 8 analog signals, take a linear VCO with a center frequency up to 6 MHz, spread these center frequencies in the 6.5 MHz band in a logarithmic-equidistant way, add up the 8 resulting FM signals, feed the resulting frequency mix into your ISM video transmitter, and you are done, sender-side. On the receiver, take apart the frequency mix you get from the video receiver and feed the resulting 8 FM signals into PLLs, restoring the original audio signals. This step (taking the "video" signal apart) is critical as far as filtering because you need to make sure no residuals from neighbouring channels end up in each channel. This crosstalk would confuse the PLLs and ultimately lead to distortions.   
  
In an ideal world, this would be pretty straightforward, but in reality VCOs and PLLs are not linear over a larger bandwidth, and the cheap ISM video transceivers e.g. FPV gadgets aren't linear either because they are designed for a completely different use case where is does not matter much if a video stream is occasionally slightly distorted or something. And we haven't even talked about the signal/noise ratio for FM conversion yet. 

By the way what troubles me about my digital concept from above is that we convert the guitar and GK signals to digital, then back to analog, and the guitar synth will again convert to digital. Sadly, VGs and GRs have analog inputs... A much better way IMHO would be to do the AD conversion in the guitar, convert the resulting unipolar TDM signal to biphase-mark (in hardware using a couple of ICs or a small FPGA or ASIC) and send it over a coax or better a shielded twisted pair cable to the synth, e.g. a simple ethernet cable with more rugged connectors. Is that how the newer [Boss GK-5 interface](https://www.boss.info/us/products/gk-5/) works? Their cables only have a TRS connector at each end but 3 contacts would be sufficient for GND, Vcc and a single-ended HF signal over coax, like consumer S/PDIF. After all, they advertise this as "advanced Serial GK digital interface". 
 
   

## Building Prototypes

In the past. I had various PCBs made and (pre-)assembled by [JLCPCB.com](https://jlcpcb.com/). They provide good quality at a decent price, and together with their partner [LCSC.com](https://www.lcsc.com/) they have a couple of 100,000 common parts on stock. (No I am not affiliated, just a satisfied customer.)  I will try to make this thing as DIY proof as possible. 

The sender will have to be connected to the guitar's GK pickup by a short GK cable, and the receiver to the synth. The shortest pre-manufactured GK cables I've seen so far (on eBay) were 2 m but maybe I'll find even shorter ones. On the other hand, soldering 1 m stubs should not be a major hassle albeit somewhat fiddly. But I did that 20 years ago by cutting 5m cables and soldering a second 13-pin male connector at each half and I'm still alive. On the other hand, no one keeps us from using a different, better connector on the wireless devices.

## Sequence of Events

 * 2024-05-05: ordered two ESP32-C6 DevKits, a TI PCM1808 ADC and a TI PCM5102 DAC breakout board each

## Commercial Thoughts

Personally, I do not think there is a significant market for a wireless GK solution. For a perceived 1000 years people like Robert Fripp, Vernon Reid or Adrian Belew, just to name a few, played their cabled GK equipment on stage without any apparent problem. And none of them
either attempted or managed to talk Roland or any 3rd party into making a custom system for them (remember Bob Bradshaw and his
proverbial floorboards?). At least not as far as I'm aware. (Well Robert Fripp isn't known for walking on stage anyway - he prefers
stools and people without cameras.)

Also, in the VGuitarForum, not even 350 people could be found to crowdfund a solution that looked like well prototyped in the YT videos we
all have seen. BTW I asked this guy if he could imagine to open source his prototype to enable skilled makers but got no answer (yet).
Guess he's utterly frustrated and has no motivation to provide support for half-skilled guys messing up his stuff. Understandable.

I'm not going to make any attempt to market something. Instead, everything will be open source (schematics, PCB layouts, codes, ...)
and hopefully community developed and supported. Which does not mean that a group of nice people could not build and sell small batches of devices at cost price, plus support contracts for pro users.


# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 



