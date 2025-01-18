# User Interface

## Sender

I envision the sender being a cigarette box sized plastic case (3D printed and internally shielded I suppose) containing a ~ 50x100 mm PCB and a LiIon battery. It will have a power slide switch, a SETUP button, a status LED, the GK input, a USB-C charge socket, and maybe an external 5 GHz WiFi antenna rod. You will also need a ~50 cm GK cable and a small carrying pouch to attach the device to your guitar belt. 

## Receiver

The receiver can be very similar, although some people may prefer a 9.5" 1U case. Anyway, it will have similar controls like the sender, and be powered by the synth for the analog part (+/- 7V) and maybe via USB-C for the digital part (because the synth may not be able to deliver enough current). A 9.5" inch case would also provide enough space for any extensions like those mentioned on the master Readme page. 

## Setup

After assembling, you will have to set up the WiFi pairing once on both devices: 

 * press and hold WPS button on the Receiver (which will provide a WiFi access point from then on) and switch on. Wait until the LED blinks in a certain pattern, which should take a few seconds.
 * the Receiver will go to setup mode, generate and store a WiFi random SSID and password, and enter WPS registrar mode. For security reasons, the timeout will be set to 2 minutes.
 * press and hold WPS on the Sender (which will be a WiFi station from then on) and switch on. Wait until the LED blinks in a certain pattern, which should take a few seconds.
 * the Sender will go to WPS enrollee mode, retrieve the SSID and password from the Receiver, and store the credentials. 
 * when successful, both devices will switch to normal mode and will be ready for use. 
 * Should you ever need to re-pair the device(s), simply repeat.

Although WPS sounds outdated, more modern methods like WiFi Easy Connect require either generating and reading a QR code or entering passwords by hand. WPS requires only a button. 

## Software Installation and Updates

For a while, I thought about providing an over-the-air (OTA) update function. On the other hand, if you are able to handle a guitar synthesizer and your guitar rig, odds are that you are able to handle a simple update using a computer and USB cable. 
The USB-C socket will connect to the ESP32 UART chip so that everyone will be able to (re-) flash their devices or if you want to experiment and contribute. And the pros will have their guitar technicians. I mean, it's not rocket science. Should OTA ever come, it may look as follows below. In any case, make sure you flash both devices of a pair with the exact same image file. Changes may be destructive, and devices with different image versions may not cooperate. (In fact, I will prevent mixed setups from working, by using the app version number of the respective build.) 

## Over-the-air Updates (OTA) (TBD) 

From time to time there may be software updates with fixes or improvements, e.g. flash rom images hosted on the Github page. 

 * download the update image to your smartphone or computer.
 * press and hold the OTA button and switch on. the device will set up a temporary WiFi access point and display the credentials on an LCD. 
 * connect your smartphone or computer using the displayed credentials. Navigate to the configuration web page. There will be a section "Over-the-air update". 
 * select the file to be uploaded e.g. from your "Downloads" folder and click "Update" 
 * the device will perform the update and reboot. 
 * needless to say that the integrity and authenticity of the upload image will be verified before the actual update. 

# Copyright and Licensing

The material in this git repository is copyrighted by me and licensed to you by the [GNU General Public License V3](https://www.gnu.org/licenses/gpl-3.0.en.html). Commercial use without written permission is strictly forbidden. If you are interested, feel free to ask for a commercial license. 


