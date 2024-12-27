# User Interface

## Sender

I envision the sender being a cigarette box sized plastic case (3D printed and internally shielded I suppose) containing a ~ 50x100 mm PCB and a LiIon battery. It will have a power slide switch, a SETUP button, a small LC display, the GK input, a USB-C charge socket, and an external WiFi antenna rod. You will also need a ~50 cm GK cable and a small carrying pouch to attach the device to your guitar belt. 

## Receiver

The receiver can be somewhat bigger. Maybe a 9.5" 1U case would be something? Anyway, it will have similar controls like the sender, and be powered by the synth for the analog part (+/- 7V) and via USB-C for the digital part (because the synth may not be able to deliver enough current). Such a 9.5" inch case would also provide enough space for any extensions like those mentioned on the master Readme page. 

## Setup

After assembling or unpacking, you will have to set up the WiFi pairing once on both devices: 

 * press and hold SETUP button, switch on.
 * the device will then set up a temporary WiFi access point and provide a simple configuration web page.
 * the LCD will guide you through this process. 
 * connect your smartphone or computer to the AP and navigate to the configuration web page. 
 * there, you can enter your favourite WiFi name (SSID) for your device pair (distinct from any other SSID that might be around) and a WPA2/PSK password that only you can remember. 
 * click OK, done. Device will reboot and be ready for use. Should you ever need to reconfigure the device(s), simply repeat.

## Over-the-air Updates (OTA)

From time to time there may be software updates with fixes or improvements, e.g. flash rom images hosted on the Github page. 

 * download the updated image to your smartphone or computer.
 * boot the device to SETUP mode as shown above. 
 * connect your smartphone or computer.
 * on the bottom of the configuration web page, there will be a section "Over-the-air update". 
 * select the file to be uploaded e.g. from your "Downloads" folder and click "Update" 
 * the device will perform the update and reboot. 
 * needless to say that the integrity and authenticity of the upload image will be verified before the actual update. 

## Internal Access

For all you electronics experts out there, the USB-C socket will actually connect to the ESP32 UART chip, in case you want to experiment and contribute. Easy to handle if you're familiar with the ESP-IDF. 


