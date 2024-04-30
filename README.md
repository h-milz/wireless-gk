# Wireless-GK

Wireless-GK is a project which aims at developing a wireless alternative for Roland's famed (huh ...) GK cables. These cables are used to connect electric guitars which are equipped with a Roland GK-2A or GK-3 hexaphonic pickup to a Roland guitar synthesizer or modeller (e.g. GR-33, GR-55, VG-99 or any other of these models).  If you came here you probably know what you are looking for but anyway, if not, here's where you can get basic [information](https://en.wikipedia.org/wiki/Guitar_synthesizer#Roland_GK_interface). The cable consists of two 13-pin DIN connectors and a 13-wire cable. See the GK-3 schematics (sorry, this is the exact same blurry Roland sourced schamtics that has been circulation forever...) to see the signals and wire assignments. 

# Previous Attempts and Developments

My first thoughts about a Wireless GK interface date back to the early 2000's. We did not have any compact microcontrollers and no WiFi back then, so what I was thinking about was a sender consisting of four 2-channel low-power audio ADCs, a 8-input controller packing the audio data into a data  stream, and a normal video transmitter.  On the receiving side, another controller would unpack the data stream to 4 stereo DACs. In principle.  Can't remember what the controllers were supposed to be.  

I did not pursue the idea for a number of years but in April 2024, I stumbled across a Czech guy's [Youtube video](https://www.youtube.com/watch?v=Ek9ydo4c_C4) of a device he had prototyped. Sadly, this project seems to have been abandoned in the meanwhile. I could not find any specification or even the general idea how he did it. 

This triggered my wish to restart my former idea with modern components, like 

  * [Unexpected Maker's Feather S3](https://esp32s3.com/feathers3.html) boards for sender and receiver. This board has a couple of advantages over other ESP32-S3 boards: A JST-PH connector and LiPo battery charger; a second 3.3V LDO which keeps supply voltages separate and clean; a u.fl WiFi antenna connector which allows me to attach an external antenna for better signal quality. 
  * sender: a [Cirrus Logic CS5368](https://www.cirrus.com/products/cs5364-66-68/) 8-channel audio DAC which is [I2S / TDM](https://en.wikipedia.org/wiki/I%C2%B2S) capable. Yes it is discontinued but still readily available. Cirrus announced the next generation product (dubbed [CS5308P](https://www.cirrus.com/products/cs5308p/)) about a year ago, and you can supposedly place orders since December 2023, but as of end of April 2024, you cannot even get samples. Maybe if the device comes up in forseeable time I will change to the new device, also because it works with a single supply voltage and needs only about a quarter of the power, which is good for battery life. 
  * receiver: a [Texas Instruments / Burr-Brown PCM1691](https://www.ti.com/product/de-de/PCM1681-Q1) 8-channel audio DAC also with I2S / TDM
  * a couple of low-noise op-amps like TL074 or OPA4134 or any other low-noise type with the same standard footprint.
  * some power management chips creating supply voltages.
  * ultra low-power LCD or e-ink displays (SPI or I2C) for the simple user interface 

At first approximation, the basic functionality will look like this: 

**Receiver**: 

  * the receiver will work as a WiFi AP and create a private WiFi and a [websocket](https://en.wikipedia.org/wiki/WebSocket) server. Websockets are TCP based without the normal HTTP overhead, so I get checksumming, packet re-sending and packet sequencing for free. 
  * using a private WiFi makes sure that you do not need to install a separate WiFi router - which would also cause a certain latency which I'd rather avoid. The net data rate of the multichannel audio stream is 32 kHz x 32 byte = 8.192 MBit/s, which is well within the documented 20 MBit/s TCP throughput provided by the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32s3/api-guides/wifi.html). 
  * on the receiver, you will have to select a WiFi AP SSID and store it. You can change the name any time later. This has no other function than allowing you to run several of these devices within WiFi range, e.g. on a stage. The WiFi password will probably be a hardcoded random string. 
  * the ESP32-S3 will work as I2S master, the ADC as I2S slave. The data format will be 24 bit resolution, 32 kHz sample rate, and 8 TDM slots. Each data packet will thus be 32 bytes long. 
  * when receiving a websocket event, the two switch bits will be extracted, and the data block will be sent to the DAC via I2S. The switch bits will be written to two open drain GPIOs. 
  * the receiver will be powered by USB-C (3.3V, 5V) and by the guitar synth which sports +/-7V LDOs for the op-amps via the GK cable. Care will be taken that all supply voltages will be as quiet as possible by using additional LDOs where needed.


**Sender**: 

  * the sender will be the WiFi client in station mode connecting to the server via websocket. 
  * on the sender, you can run a WiFi scan and select the desired receiver SSID which will stored permamently and can be changed any time later. The WiFi password will probably be a hardcoded random string. 
  * the signals E1 - E6 from the hex pickup, the normal guitar signal and the GK VOL voltage will be sampled by the ADC, and the two GK switches read via GPIO and their bits hidden in the TDM data stream (i.e. as the two LSB of the GK-VOL TDM slot 7). 
  * as soon as a TDM packet is available, it will be merged with the GK switch bits and sent to the receiver. 
  * the sender will be powered by a single LiPo battery of about 2500 mAh or larger if I can get one. Charging and battery voltage measurement are supported by the FeatherS3 board.  Power management circuitry will generate +5V for the ADC and +/- 7V for the op-amps. Care will be taken that all supply voltages will be as quiet as possible by using additional LDOs where needed.


**Building Prototypes**:

In the past. I had various PCBs made and (pre-)assembled by [JLCPCB.com](https://jlcpcb.com/). They provide good quality at a decent price.  I will try to make this thing as rebuild-proof as possible. 

The sender will have to be connected to the guitar's GK pickup by a short GK cable, and the reciver to the synth. The shortest pre-manufactured GK cables I've seen so far (on eBay) were 2 m but maybe I'll find even shorter ones. On the other hand, soldering 1 m stubs should not be a major hassle albeit somewhat fiddly. I did that 20 years ago and I'm still alive. 



# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 



