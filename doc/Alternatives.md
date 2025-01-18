# Alternative Approaches


As written on my [old web page](https://www.muc.de/~hm/music/Wireless-GK/), it would generally be possible to avoid a microcontroller and a WiFi in the data path, hence reducing latency, and use a generic **ISM band video transmitter** instead. The data encoding could be differential Manchester or biphase-mark (BP-M). The required bandwidth for BP-M is [twice the data rate](https://www.researchgate.net/figure/PSD-for-Manchester-Coding_fig15_45914350) (i.e. circa 2 MHz per channel). Said video transmitters in the 2.4 or 5.8 GHz band provide a single video channel for NTSC or PAL, and provide about 6.5 MHz of video bandwidth, which makes them unfeasible for 8-channel uncompressed audio. I don't think anybody wants to schlep 2 such transmitters plus batteries on stage. And I don't like the idea of audio compression in this stage. 
  
Using a UHF transmitter with a higher modulation bandwidth (e.g. 25 MHz) would require an amateur radio license. Not feasible.  

Enter **HDMI**. Even HDMI 1.0 supports 8-channel audio with 16 or 24 bits up to 192 kHz sample rate in I2S or DSD formats. Converter chips are readily available and well documented e.g. from Lattice. Input/output from our device would be HDMI so everybody would have to bring their own transceiver pair depending on the country. Latency? One advert for a wireless HDMI kit honestly said "signal experiences only 0.1s latency (almost real-time)".  OK, thank you, case closed.

What troubles me about all of these concepts is that we convert the guitar and GK signals to digital, send them over the air, convert back to analog, and the guitar synth will again convert to digital. Sadly, VGs and GRs do have analog inputs (or maybe someone builds an alternative connection board?). A much better way IMHO would be to do the AD conversion in the guitar, convert the resulting unipolar TDM signal to differential and send it over shielded twisted pair cable to the synth, e.g. a simple ethernet cable with more rugged connectors. That is precisely how the newer [Boss GK-5 interface](https://www.boss.info/us/products/gk-5/) works. Their cables only have a TRS connector at each end but 3 contacts are sufficient for GND and a differential I2S signal (plus supply voltage) over a shielded twisted pair cable. After all, they advertise this as "advanced Serial GK digital interface". 
 
A rather theoretical variant would be a purely analog one: **analog frequency multiplex**. For each of the 7 analog signals, you create an FM signal in the range between, say, 500 kHz and 4 MHz, add up the 7 resulting signals, feed the resulting frequency mix into your ISM video transmitter, and you are done, sender-side. On the receiver, demodulate using the same center frequencies, restoring the original audio signals. 

In an ideal world, this would be pretty straightforward, but in reality mixers, VCOs and PLLs are not linear over a larger bandwidth, and the cheap ISM video transceivers e.g. FPV gadgets aren't linear either because they are designed for a completely different use case where is does not matter much if a video stream is occasionally slightly distorted or something. And we haven't even talked about the signal/noise ratio for the modulation / demodulation. The other issue is that the signal is not protected against jamming at all. And it would probably sound like a cheap FM radio. It's for a reason why professional audio transmitter solutions from Sennheiser et al. are so darn expensive, and everyone goes digital now.
   
It appears that the WiFi based solution is the least bad one. 

A completely different approach is the [Fishman TriplePlay Bridge Wireless MIDI Pickup](https://www.fishman.com/portfolio/tripleplay-wireless-midi-guitar-controller/). As the name suggests, this part transmits MIDI data over wireless which is far easier than audio due to the much lower amount of data. Basically, this part presumably has an audio-to-MIDI pitch and dynamics converter on board, and it does not interface to Roland's GK synth series. 

# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 


