# Functionality 

At first-order approximation, the basic functionality will look like this:


## Sender

  * the sender will work as a WiFi station or ESP-NOW peer.
  * for the pairing, the user will have to configure some WiFi options depending on the protocol and encryption via a simple web UI.  
  * the signals E1 - E6 from the hex pickup and the normal guitar signal will be sampled by the ADC.
  * the two GK switches will be read via GPIO, the GK VOL voltage will be AD converted by an internal ADC of the ESP32 every, say, 100ms in the background, and the data will be inserted in slot #7 of the TDM data stream, which is not used by audio data. 
  * the ESP32 I2S controller (I2S0) will work as I2S master, generating all the required clocks. Each data packet in the TDM frame will be 32 bytes long, but we may pack them into 24 to reduce the over-the-air data rate if we have spare CPU cycles. (packing takes only 0.3 microseconds on an ESP32-S2). 
  * use the WS / FSYNC / LRCK pin of I2S0 as an interrupt source in order to pick up digital audio data as quickly as possible. 
  * read the data from the I2S DMA buffer and send via WiFi
  * Power: a single LiPo battery of about 2500 mAh or more. One could use 18650 cells or flat packs with a JST-PH connector for example. Charging and battery voltage measurement will be handled by an equivalent of Adafruit's [PowerBoost 1000C](https://learn.adafruit.com/adafruit-powerboost-1000c-load-share-usb-charge-boost/overview) board. A charge pump and an inverter with LDOs behind them will generate +/- 7V directly from the LiPo cell for the op-amps and the GK-2A or GK-3. The ADC analog supply will be generated from the LiPo cell via a separate LDO. 

## Receiver

  * the receiver will work as a WiFi AP or ESP-NOW peer 
  * for the pairing, the user will have to configure some parameters via a simple web UI. 
  * again, the ESP32 I2S controller (I2S0) will work as I2S master, generating all the required clocks.
  * upon a "data received" event, the digital audio data will be unpacked and sent to the DAC. 
  * when receiving a checksum error on a sample, the previous sample will be repeated for example (for UDP or ESP-NOW)
  * Power: via USB-C for the ESP32 and via a separate LDO for the DAC, and by the guitar synth which provides the usual +/-7V for the op-amps via the GK cable. Care will be taken that all supply voltages will be as quiet as possible by using additional LDOs where needed.


