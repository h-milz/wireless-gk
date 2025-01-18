# Legacy Projects

My first thoughts about a Wireless GK interface date back to the early 2000's. There were no powerful microcontrollers and no WiFi back then, so what I was thinking about was a sender consisting of four 2-channel low-power audio ADCs, a 8-input controller packing the audio data into a data  stream, and a normal video transmitter.  On the receiving side, another controller would unpack the data stream to 4 stereo DACs. In principle.  Here's my old web page with the [ramble](https://www.muc.de/~hm/music/Wireless-GK/).

I did not pursue the idea for a number of years but in April 2024, I stumbled across a [Youtube video](https://www.youtube.com/watch?v=Ek9ydo4c_C4) by a Czech guy named "rockykoplik" demoing a device he had prototyped. Sadly, this project never made it past the prototype stage. The [web shop](https://www.blucoe.eu/) is closed. The [FAQ](https://www.blucoe.eu/faq/) says the device was supposed to use the 2.4 GHz band with an unspecified "adaptive frequency hopping" algorithm, and it uses audio compression to get along with a RF bandwidth of 4 MHz. Audio samples were 24/48, and the specified latency was 16 ms (mostly caused by audio compression I suppose). Sadly, nothing is open source, and the PCB images are blurred, so there is no way to verify these claims. 

# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 

