# Functionality 

At first-order approximation, the basic functionality will look like this:


## Sender

  * the sender will work as a WiFi station.
  * the signals E1 - E6 from the hex pickup and the normal guitar signal will be sampled by 7 channels of the the 8-channel ADC.
  * the two GK switches will be read via GPIO, the GK VOL voltage will be AD converted by the spare ADC channel of the audio ADC, and the data will be inserted in slot #7 of the TDM data stream, which is not used by audio data. 
  * the ESP32 I2S controller will read the sampled data from the ADC. Each TDM frame will be 32 bytes long (8 slots * 4 byte), but we will pack them into 24 byte (8 slots * 3 byte, since we want to sample with 24 bit = 3 byte only) to reduce the over-the-air data rate. (packing a 60-sample packet takes only 18 microseconds on an ESP32-S3 or 9 microseconds on a C6, which is a small price for a 25 percent bandwidth reduction). 
  * use the I2S DMA driver to generate an interrupt each time 60 samples were taken, i.e. every 1.36 ms. 
  * read the data from the I2S DMA buffer and send via WiFi / UDP
  * Power: a single LiPo battery of about 2500 mAh or more. One could use 18650 cells or flat packs with a JST-PH connector for example. Charging and battery voltage measurement will be handled by an equivalent of Adafruit's [PowerBoost 1000C](https://learn.adafruit.com/adafruit-powerboost-1000c-load-share-usb-charge-boost/overview) board. A charge pump and an inverter with LDOs behind them will generate +/- 7V directly from the LiPo cell for the op-amps and the GK-2A or GK-3. The ADC analog supply will be generated from the LiPo cell via a separate LDO. 

## Receiver

  * the receiver will work as a WiFi access point.
  * UDP packets will be read into a ring buffer as they arrive, i.e. asynchronously.
  * the I2S DMA driver will generate an interrupt for each buffer (60 frames) that was just received, i.e. this will be a synchronous transfer. 
  * upon each interrupt, the last received UDP buffer will be unpacked and sent to the DAC via DMA. 
  * Power: via USB-C for the ESP32 and via a separate LDO for the DAC, and by the guitar synth which provides the usual +/-7V for the op-amps via the GK cable. Care will be taken that all supply voltages will be as quiet as possible by using additional LDOs where needed.

## WiFi Pairing

When pressing and holding the WPS button, Sender and Receiver go to WPS mode. The Receiver is the WPS registrar, the Sender is the WPS enrollee. Our WPS process only supports PBC (Push Button Configuration), and WiFi encryption is tied to WPA3-SAE. 

WPS may not be the bleeding edge way of proliferating the WiFi SSID and password, but is Good Enough (TM). 

 * since we're using PBC, the known [weaknesses of WPS](https://en.wikipedia.org/wiki/Wi-Fi_Protected_Setup#Security) are ruled out.
 * ESP-IDF supports also (any only!) WiFi EasyConnect with a QR code but the QR code is printed on the serial console, and the client then has to scan the QR code. A) it is cumbersome to use a PC with a serial console just for pairing, and B) integrating a camera into the client just for pairing seems farfetched. 
 * SmartConfig requires a third party Android (or iOS?) app (of which I can't know how well security is implemented, and I do not want to rely on third party tools) and again: the QR code. 
 * Using a separate channel like Bluetooth or ESP-NOW requires writing own (security relevant) code, and getting that to securely work can be a nightmare. Both ways also require pairing using a password first, which means it's a classical chicken-and-egg thing. 




