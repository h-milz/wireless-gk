#!/bin/bash

# prepare the repo and its submodules
# no user servicable parts inside. This is for the maintainer only. 

ESP_IDF="git@github.com:espressif/esp-idf.git"
IDF_VER="master"

mkdir -p lib
(
cd lib
# clone the submodules
git clone -b ${IDF_VER} ${ESP_IDF} esp-idf
)

# add submodules
git submodule add ${ESP_IDF} lib/esp-idf/

# make permanent
git add lib/esp-idf/ 
git commit -m "Added submodules"
git push origin main


