# Functionality 

At first-order approximation, the basic functionality looks like this:


## Sender

  * the sender works as a WiFi station and WPS client. 
  * the signals E1 - E6 from the hex pickup and the normal guitar signal are sampled by 7 channels of the 8-channel ADC. GKVOL is sampled by the 8th (spare) channel. 
  * the ESP32 I2S controller reads the sampled data from the ADC using the [TDM MSB format](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/peripherals/i2s.html#tdm-mode). Each TDM frame contains 8 slots (channels) of 32 bits (4 bytes) each = 32 bytes of data, but we pack them into 24 byte (8 slots * 3 byte, since we want to sample with 24 bit = 3 byte only) to reduce the over-the-air data rate. (packing a 60 frame packet takes only 18 microseconds on an ESP32-S3 or 9 microseconds on a C6, which is a small price for a 25 percent bandwidth reduction). 
  * the two GK switches are read via GPIO, and the bits are inserted in slot #7 of the TDM data stream, which is not used by audio data. 
  * use the I2S DMA driver to generate an interrupt each time 60 frames were received, i.e. every 1.36 ms. 
  * read the data from the I2S DMA buffer and send via WiFi / UDP
  * Power: a single LiPo battery of about 2500 mAh or more. One could use 18650 cells or flat packs with a JST-PH connector for example. Charging and battery voltage measurement will be handled by an equivalent of Adafruit's [PowerBoost 1000C](https://learn.adafruit.com/adafruit-powerboost-1000c-load-share-usb-charge-boost/overview) board. A charge pump and an inverter with LDOs behind them will generate +/- 7V directly from the LiPo cell for the op-amps and the GK-2A or GK-3. The ADC analog supply will be generated from the LiPo cell via a separate LDO. 

## Receiver

  * the receiver works as a WiFi access point and WPS registrar. 
  * UDP packets are read into a ring buffer as they arrive, i.e. asynchronously.
  * the I2S DMA driver generates an interrupt for each buffer (60 frames) that was just received, i.e. this is a synchronous transfer. 
  * upon each interrupt, the last received UDP buffer is unpacked and sent to the DAC via DMA. 
  * Power: via USB-C for the ESP32 and via a separate LDO for the DAC, and by the guitar synth which provides the usual +/-7V for the op-amps via the GK cable. Care will be taken that all supply voltages will be as quiet as possible by using additional LDOs where needed.

## Wifi Channel Selection

In order to avoid congested channels, the Receiver performs a channel scan when initializing the Wifi AP and chooses the lowest numbered channel with the lowest AP count. Scan starts at channel 36 and goes up to 177. 

## WiFi Pairing

When pressing and holding the WPS button, Sender and Receiver go to WPS mode. The Receiver is the WPS registrar, the Sender is the WPS enrollee. The device only supports PBC (Push Button Configuration), and WiFi encryption is tied to WPA3-SAE. 

WPS may not be the bleeding edge way of proliferating the WiFi SSID and password, but is Good Enough (TM). 

 * since we're using PBC, the known [weaknesses of WPS](https://en.wikipedia.org/wiki/Wi-Fi_Protected_Setup#Security) are ruled out.
 * ESP-IDF supports also (any only!) WiFi EasyConnect with a QR code but the QR code is printed on the serial console, and the client then has to scan the QR code. A) it is cumbersome to use a PC with a serial console just for pairing, and B) integrating a camera into the client just for pairing seems farfetched. 
 * SmartConfig requires a third party Android (or iOS?) app (of which I can't know how well security is implemented, and I do not want to rely on third party tools) --  and again: the QR code. 
 * Using a separate channel like Bluetooth or ESP-NOW requires writing own (security relevant) code, and getting that to securely work and rule out vulnerabilities can be a nightmare. Both ways also require pairing using a password first, which means it's a classical chicken-and-egg thing. 

# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 




