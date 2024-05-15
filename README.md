# Wireless-GK

Wireless-GK is a project which aims at developing a wireless alternative for Roland's famed (huh ...) GK cables. These cables are used to connect electric guitars which are equipped with a Roland GK-2A or GK-3 hexaphonic pickup to a Roland guitar synthesizer or modeller (e.g. GR-33, GR-55, VG-99 or any other of these models).  If you came here you probably know what you are looking for but anyway, if not, here's where you can get basic [information](https://en.wikipedia.org/wiki/Guitar_synthesizer#Roland_GK_interface). The cable consists of two 13-pin DIN connectors and a 13-wire cable. See the GK-3 schematics (sorry, this is the exact same blurred Roland sourced schematics that has been circulating forever...) to see the signals and wire assignments. 

# Previous Attempts and Developments

My first thoughts about a Wireless GK interface date back to the early 2000's. We did not have any powerful microcontrollers and no WiFi back then, so what I was thinking about was a sender consisting of four 2-channel low-power audio ADCs, a 8-input controller packing the audio data into a data  stream, and a normal video transmitter.  On the receiving side, another controller would unpack the data stream to 4 stereo DACs. In principle.  Here's my old web page with the [ramble](https://www.muc.de/~hm/music/Wireless-GK/).

I did not pursue the idea for a number of years but in April 2024, I stumbled across a [Youtube video](https://www.youtube.com/watch?v=Ek9ydo4c_C4) by a Czech guy named "rockykoplik" demoing a device he had prototyped. Sadly, this project seems to fly under the radar for some reason. The web shop is closed. In any case, an earlier post in the [VGuitarForums](https://www.vguitarforums.com/smf/index.php?msg=257890) states that the system causes a latency of 16 ms. Another [post](https://www.vguitarforums.com/smf/index.php?msg=251550) says 13-17 ms. We'll talk about latency later. 

This triggered my wish to restart my former idea with more recent components, like 

  * an ESP32-C5 MCU (as soon as the C5 is available)

  * 8-channel ADCs and DACs by AKM

  * low-noise / low THD op-amps like OPA4134, 4227 or 1679

  * some power management chips for LiPo charging and creating supply voltages.
 
Primarly for marketing reasons, everything will work with 24 bit resolution, 44.1 kHz sample rate. The VG-99 and the GR-55 work with these parameters, and it would be hard to advertise a device that does less, although from a technical standpoint 16/32 or 16/36 would probably be enough. The downside to this is, it will require a higher over-the-air bandwidth (see **Alternative Approaches** below). 

Please check the [Components](Components.md) file for more information. 

## A Word About Using WiFi

In order to reduce latency, the combo will be using a private peer-to-peer WiFi with one of these configs: 

 * with the receiver as the AP running a websocket server and the sender as the client, 
 * same, but running UDP instead of websockets
 * running ESP-NOW which was designed for high throughput and low latency. 

The net WiFi data rate is 8 channels x 44.1 kHz x 32 byte (I2S TDM data format) = 11289600 bps, which is well within the theoretical net data rate for WiFi6 / MCS9. (Although the [ESP-IDF says](https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32c6/api-guides/wifi.html) 20 Mbps for TCP and 30 for UDP - guess that's a copy&paste error from earlier architectures.) Spare cpu cycles permitting, one could compress the data format to have 24 bit slots instead of 32. 

Speaking of which, with a private P2P WiFi and  under reasonably good environmental conditions we should end up with a latency of 5-15 ms. With pure UDP or ESP-NOW we should  be able to get this down to small single digits but would have to add some basic error detection like XOR checksumming and packet numbering. This remains to be tested which is the first step I'll do as soon as the development boards arrive (i.e. raise a GPIO in the sender, then send a couple hundred 32 byte packets one way, then on the receiver pick up the packets and raise a GPIO when finished. Measure the time between the GPIOs with my oscilloscope).  

To keep latency caused by the protocol overhead down, one can collect a number of 8-slot samples and send batches of samples across, at the expense of a short delay which, for 10 sample batches, is about 0.2 ms. Presumably, when doing this, the latency caused by this delay will go up, but the latency caused by protocol overhead will go down _per frame_. During development, I will experiment with Websockets, UDP and ESP-NOW and try to find this sweet spot in terms of per-frame latency. 


At the end of the day you need to choose between the devil and the deep blue sea I suppose. I'm all open for plausible proposals for a better (and technically feasible) technical design. 


At first-order approximation, the basic functionality will look described in the [Functionality](Functionality.md) section. 



## Potential Extensions

I can imagine some extensions for this solution: 

  * [Roland US-20](https://www.roland.com/global/products/us-20/) functionality in the receiver. It would be trivial to add a second or more 13-pin outputs and switch between them, even using the GK-2A or switches, or extra switches in the sender.
  * [RMC fanout box](https://www.rmcpickup.com/fanoutbox.html) functionality in the receiver. Trivial as well. 
  * [RMC subsonic filtering](https://www.joness.com/gr300/Filter-Buffer.htm) in the sender, removing the need for a VG-99 mod when using a piezo pickup equipped guitar. 

These extensions could be optional and pluggable via flat ribbon cables or something. More ideas welcome. In any case I envision a completely open architecture where anybody can add their stuff somehow. 

## Filter Design

See [Filter Design](Filterdesign.md). 

## Alternative Approaches

See [Alternatives](Alternatives.md). 

## Building Prototypes

In the past. I had various PCBs made and (pre-)assembled by [JLCPCB.com](https://jlcpcb.com/). They provide good quality at a decent price, and together with their partner [LCSC.com](https://www.lcsc.com/) they have some 100,000 common parts in stock. (No I am not affiliated, just a satisfied customer.)  



## Sequence of Events

See [Progress](Progress.md). 

## Commercial Thoughts

Personally, I do not think there is a significant market for a wireless GK solution. For what feels like 1000 years the likes of Robert Fripp, Vernon Reid or Adrian Belew, just to name a few, played their cabled GK equipment on stage without any apparent problem. And none of them either attempted or managed to talk Roland or any 3rd party into making a custom system for them (remember Bob Bradshaw and his proverbial floorboards?). At least not as far as I'm aware. (Well Robert Fripp isn't known for walking on stage anyway - he prefers stools and people without cameras.)

Also, in the VGuitarForum, not even 350 people could be found to crowdfund a solution that looked like well prototyped in the YT videos we all have seen. 

I'm not going to make any attempt to market something. Instead, everything will be open source (schematics, PCB layouts, codes, ...) and hopefully community developed and supported. Which does not mean that a group of nice people could not build and sell small batches of devices at cost price, plus support contracts for pro users.


# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 



