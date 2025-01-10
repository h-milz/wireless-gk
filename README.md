# Wireless-GK

Wireless-GK is a project which aims at developing a wireless alternative for Roland's famed (huh ...) GK cables. These cables are used to connect electric guitars which are equipped with a Roland GK-2A or GK-3 hexaphonic pickup to a Roland guitar synthesizer or modeller (e.g. GR-33, GR-55, VG-99 or any other of these models).  If you came here you probably know what you are looking for but anyway, if not, here's where you can get basic [information](https://en.wikipedia.org/wiki/Guitar_synthesizer#Roland_GK_interface). The cable consists of two 13-pin DIN connectors and a 13-wire cable. See the [GK-3 schematics](doc/GK-3-schematics.png). 

# Previous Attempts and Developments

My first thoughts about a Wireless GK interface date back to the early 2000's. We did not have any powerful microcontrollers and no WiFi back then, so what I was thinking about was a sender consisting of four 2-channel low-power audio ADCs, a 8-input controller packing the audio data into a data  stream, and a normal video transmitter.  On the receiving side, another controller would unpack the data stream to 4 stereo DACs. In principle.  Here's my old web page with the [ramble](https://www.muc.de/~hm/music/Wireless-GK/).

I did not pursue the idea for a number of years but in April 2024, I stumbled across a [Youtube video](https://www.youtube.com/watch?v=Ek9ydo4c_C4) by a Czech guy named "rockykoplik" demoing a device he had prototyped. Sadly, this project seems to fly under the radar for some reason. The web shop is closed. In any case, an earlier post in the [VGuitarForums](https://www.vguitarforums.com/smf/index.php?msg=257890) states that the system causes a latency of 16 ms. Another [post](https://www.vguitarforums.com/smf/index.php?msg=251550) says 13-17 ms. We'll talk about latency later. 

This triggered my wish to restart my former idea with more recent components using WiFi, like 

  * ESP32-C5 MCUs (of which I have two sample DevKits from Espressif)

  * 8-channel ADCs and DACs by AKM

  * low-noise / low THD op-amps like NJM2068 or NE5532
  
  * some power management chips for LiPo charging and creating supply voltages.
 
Primarly for marketing reasons, everything is supposed to work with 24 bit resolution, 44.1 kHz sample rate. The VG-99 and the GR-55 work with these parameters, and it would be hard to advertise a device that does less, although from a technical standpoint 16/32 or 16/36 would probably be enough. The downside to this is, it will require a higher over-the-air bandwidth (see **Alternative Approaches** below). 

Please check the [Components](doc/Components.md) file for more information. 

## A Word About Using WiFi

In order to keep latency low and resilience against network congestion high, the combo uses a private peer-to-peer WiFi6 802.11AX network with WPA3 authentication on the 5 GHz band. 

The net WiFi data rate is 8 channels x 24 bytes per channel x 44100 samples per second = 8.4672 Mbps, which is well within the theoretical net data rate for [WIFI_PHY_RATE_MCS7_SGI](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/network/esp_wifi.html#_CPPv415wifi_phy_rate_t) -- in fact, I measured a maximum UDP throughput between 2 ESP32-C5 boards of around 63 MBit/s using iperf. 

To keep latency caused by the UDP protocol overhead low, I collect a number of 8-slot samples and send batches of samples across, at the expense of a short delay. The maximum UDP payload of 1472 bytes allows for collecting 60 samples per batch, which sums up to 1440 bytes. ADC conversion of 60 samples takes 1.36 ms. 

The measured UDP latency for 1440 byte packets between the two boards proved to be in the 800 µs range. This means the overall latency will be of the order of magnitude of 2.2 ms. 

At the end of the day you need to choose between the devil and the deep blue sea I suppose. I'm all open for plausible proposals for a better (and technically feasible) technical design. 

At first-order approximation, the basic functionality will look described in the [Functionality](doc/Functionality.md) section. 

## User Interface

See [User Interface](doc/Userinterface.md). 


## Filter Design

See [Filter Design](doc/Filterdesign.md). 

## Alternative Approaches

See [Alternatives](doc/Alternatives.md). 

## Potential Extensions

I can imagine some extensions for this solution, mostly receiver-side: 

  * [Roland US-20](https://www.roland.com/global/products/us-20/) functionality. It would be trivial to add a second or more 13-pin outputs and switch between them, even using the GK-2A or switches, or extra switches in the sender.
  * [RMC fanout box](https://www.rmcpickup.com/fanoutbox.html) functionality. Trivial as well. 
  * [RMC subsonic filtering](https://www.joness.com/gr300/Filter-Buffer.htm), removing the need for a VG-99 mod when using a piezo pickup equipped guitar. 

These extensions could be optional and pluggable via flat ribbon cables or something. More ideas welcome. In any case I envision a completely open architecture where anybody can add their stuff somehow. 

## Commercial Thoughts

Personally, I do not think there is a significant market for a wireless GK solution. For what feels like 1000 years the likes of Robert Fripp, Vernon Reid or Adrian Belew, just to name a few, played their cabled GK equipment on stage without any apparent problem. And none of them either attempted or managed to talk Roland or any 3rd party into making a custom system for them (remember Bob Bradshaw and his proverbial floorboards?). At least not as far as I'm aware. (Well Robert Fripp isn't known for walking on stage anyway - he prefers stools and people without cameras.)

Also, in the VGuitarForum, not even 350 people could be found to crowdfund a solution that looked like well prototyped in the YT videos we all have seen. 

## Building Prototypes

In the past. I had various PCBs made and (pre-)assembled by [JLCPCB.com](https://jlcpcb.com/). They provide good quality at a decent price, and together with their partner [LCSC.com](https://www.lcsc.com/) they have some 100,000 common parts in stock. (No I am not affiliated, just a satisfied customer.)  They will not assemble the ESP32-C5 module, the ADC, the DAC, or the GK socket, though, simply because they usually don't have any in stock. While the ESP module and the GK socket are easy to solder for someone with good soldering experience, the ADC and DAC have a super small QFN case which can be a challenge if you're not used to working with small SMD parts. 

I'm not going to make any attempt to commercialize something. Instead, everything will be open source (schematics, PCB layouts, codes, ...) and hopefully community developed and supported. Which does not mean that a group of dedicated people could not build and ship small batches of devices at cost price, plus support contracts for pro users, as long as the prerequisites of the [LICENSE](COPYING) are observed. If you want to participate, please be familiar with the ESP-IDF, git, and Github. 

There is one more issue to **commercial distribution**. Although the individual ESP32 modules and dev boards are FCC and CE certified, marketing a compound device using such a module requires **certification** of the whole device, which can be time-consuming and extremely costly (which presumably killed the previously mentioned solution). Should I ever ship pre-built (and fully tested!) devices, then they will be preassembled kits without ESP module and LiIon battery, but with comprehensive instructions how to complete the device. The final steps involve purchasing the appropriate ESP modules and a battery, flash the modules using your computer and a USB cable, plug them in the printed circuit boards, plug the battery in the sender, and you should be ready to go. If anyone has a simpler idea, feel free to comment! Ah yes, and 3D printed cases would be nifty. 

## Sequence of Events

See [Progress](doc/Progress.md). 

# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 



