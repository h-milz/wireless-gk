# Wireless-GK

Wireless-GK is a project which aims at developing a wireless alternative for Roland's famed (huh ...) GK cables. These cables are used to connect electric guitars which are equipped with a Roland GK-2A or GK-3 hexaphonic pickup to a Roland guitar synthesizer or modeller (e.g. GR-33, GR-55, VG-99 or any other of these models).  If you came here you probably know what you are looking for but anyway, if not, here's where you can get basic [information](https://en.wikipedia.org/wiki/Guitar_synthesizer#Roland_GK_interface). The cable consists of two 13-pin DIN connectors and a 13-wire cable. See the [GK-3 schematics](doc/GK-3-schematics.png). 

## Basic Concept

This project is basically a low-latency generic 8-channel audio bridge over WiFi, where the inputs and outputs are tailored to work with Roland GK guitar synthesizers. You plug in your Roland GK equipped guitar to the sender's input. Analog audio data gets converted to digital, sent over the WiFi bridge, converted back to analog, and fed into your guitar synth. 

The basic building blocks are: 

  * the brand new (not yet commercially available) ESP32-C5 MCUs (of which I have two sample DevKits from Espressif)

  * 8-channel audio ADCs and DACs by AKM

  * low-noise / low THD op-amps like NJM2068 or NE5532
  
  * some power management chips for LiPo charging and creating supply voltages.

Analog/digital conversion takes place at a resolution of 24 bits per sample and a sample rate of 44.1 kHz, just like Roland's recent guitar synthesizer models. 

In order to keep latency low, the sender and receiver set up a private encrypted WiFi network with nothing in between. Data transport is done via UDP, like most if not all audio streaming solutions, to keep the latency low. The ESP32-C5 supports 5 GHz WiFi6 (802.11ax) which makes sure a) we stay away from the usually congested 2.4 GHz network, and b) we have more bandwidth available. I measured a sustained UDP bandwidth of about 62 MBit/s and a UDP packet latency of about 540 µs which is very well suited for such a solution _without audio compression_. 

Please click for more information: 

 * [Functionality](doc/Functionality.md) 
 * [User Interface](doc/Userinterface.md)
 * [WiFi Concept](doc/Wifi.md)
 * [Components](doc/Components.md) 
 * [Analog Filter Design](doc/Filterdesign.md)
 * [Alternative Approaches](doc/Alternatives.md)
 * [Legacy Projects](doc/Legacy.md)

## Current Status

What works: 

 * 2-channel audio streaming using the final 8-channel architecture (i.e. 6 dummy channels are transmitted) 
 * the end-to-end audio latency is less than 10 ms

What needs more development and testing

 * code cleanup
 * actual synthesizer use - this requires an adapter board which provides the supply voltage for the GK-3 kit. At the moment, this can only be tested with 2 strings (2 audio channels)
 * developing a prototype sporting the actual 8-channel A/D and D/A converters, GK-3 interface, etc. 


See also [Timeline / Progress](doc/Progress.md)


## Commercial Thoughts

Personally, I do not think there is a significant market for a wireless GK solution. For what feels like 1000 years the likes of Robert Fripp, Vernon Reid or Adrian Belew, just to name a few, played their cabled GK equipment on stage without any apparent problem. And none of them either attempted or managed to talk Roland or any 3rd party into making a custom wireless system for them (remember Bob Bradshaw and his proverbial floorboards?). At least not as far as I'm aware. (Well Robert Fripp isn't known for walking on stage anyway - he prefers stools and people without cameras.)

Also, in the VGuitarForum, not even 350 people could be found to crowdfund the [Blucoe](doc/Legacy.md) solution that looked like well prototyped in the YT videos we have seen. 

## Building Prototypes

In the past, I had various PCBs made and (pre-)assembled by [JLCPCB.com](https://jlcpcb.com/). They provide good quality at a decent price, and together with their partner [LCSC.com](https://www.lcsc.com/) they have some 100,000 common parts in stock. (No I am not affiliated, just a satisfied customer.)  They will not assemble parts they do not have in stock themselves, like the ESP32-C5 module, the ADC, the DAC, or the GK socket. While the ESP module and the GK socket are easy to solder for someone with good soldering experience, the ADC and DAC chips come in super small QFN (quad flat-pack no leads) packages which can be a challenge if you're not used to working with small SMD parts. 

Basically, everything will be open source (schematics, PCB layouts, codes, ...) using the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). It is my hope that the project will be community developed and supported. Dedicated people could build and ship small batches of devices at cost price, plus support contracts for pro users, as long as the prerequisites of the [LICENSE](COPYING) are observed. If you would like to participate, you should have ample experience with the ESP-IDF, git, and Github. 

There is one major obstacle to **commercial distribution**. Although the individual ESP32 modules and dev kits are FCC and CE (and whatnot) certified, marketing a compound device using such a module requires **certification** of the whole device, which can be time-consuming and extremely costly (which presumably killed the [Blucoe](doc/Legacy.md) solution). 

What _is_ possible, though, is making modules similar to Adafruit's [Featherwings](https://learn.adafruit.com/adafruit-feather/featherwings) or Raspberry Pi [HATs](https://en.wikipedia.org/wiki/Raspberry_Pi#Accessories). You would acquire a kit where you need to plug in the ESP module and LiIon battery, flash the ESP module yourself, and you're ready to go. There would be comprehensive instructions how to get this done. This is not rocket science - if you are able to handle a guitar synthesizer and your guitar rig, odds are that you are able to handle these steps, or you know someone who does. And the pros will have their guitar technicians. 



## Potential Extensions

I can imagine some extensions for this solution, mostly receiver-side: 

  * [Roland US-20](https://www.roland.com/global/products/us-20/) functionality. It would be trivial to add a second or more 13-pin outputs and switch between them, even using the GK-2A or switches, or extra switches in the sender.
  * [RMC fanout box](https://www.rmcpickup.com/fanoutbox.html) functionality. Trivial as well. 
  * [RMC subsonic filtering](https://www.joness.com/gr300/Filter-Buffer.htm), removing the need for a VG-99 mod when using a piezo pickup equipped guitar. 

These extensions could be optional and pluggable via flat ribbon cables or something. More ideas welcome. In any case I envision a completely open architecture where anybody can add their stuff somehow. 



# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 



