#!/bin/bash

# Linux only. 
CPUS=$(grep ^processor /proc/cpuinfo |wc -l) 

# get the required components

COMPONENTS="components/bt/host/nimble/nimble               \
            components/esp_wifi                            \
            components/esptool_py/esptool                  \
            components/lwip/lwip                           \
            components/mbedtls/mbedtls                     \
			components/tinyusb                             "

(
cd lib/esp-idf
for COMPONENT in ${COMPONENTS} ; do
    git submodule update --init --depth=1 --jobs ${CPUS} -- ${COMPONENT} 
done    
./install.sh 
)
. ../esp-idf/export.sh 

exit

# build 

# idf.py set-target esp32s3
idf.py -p /dev/ttyUSB0 erase-flash flash
esptool.py --chip esp32s3 write_flash 0x200000 image.bin   # SPIFFS image
idf.py -p /dev/ttyUSB0 monitor




exit


$ cat partitions.csv 
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
phy_init, data, phy,     0xe000,  0x1000
factory,  app,  factory, 0x10000, 0x1E0000
storage,  data, spiffs,  0x200000,0x200000
 2751  idf.py -p /dev/ttyUSB0 flash 
 2752  /home/hm/local/esp-idf/components/spiffs/spiffsgen.py 0x200000 main image.bin
 2753  esptool.py --chip esp32s3 write_flash 0x200000 image.bin
 2754  idf.py monitor




