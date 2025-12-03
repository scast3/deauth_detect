## Setup Instructions
### Raspberry Pi
- Clone this repository on your Raspberry Pi
```shell
git clone git@github.com:scast3/deauth_detect.git
cd deauth_detect
```
- Run Raspberry Pi setup script to install DuckDB and necessary libraries.
```shell
sudo chmod +x pi_setup.sh
sudo ./pi_setup.sh
```
- Build the C++ program on your Raspberry Pi
```shell
g++ rpi/src/main.cpp rpi/src/esp32_to_uart.cpp -o rpi/build/deauthdetect \ 
	-lduckdb -I /usr/local/include -L /usr/local/lib 
```
- Run the program
```shell
rpi/build/deauthdetect
```
### ESP32 Sensor
- Clone this repository on your local machine
```shell
git clone git@github.com:scast3/deauth_detect.git
cd deauth_detect/esp32sensor
```
- Plug in your ESP32 via USB to your machine
- Find which serial port the ESP32 is using
```shell
ls /dev/* | grep USB
```
- Set permissions I/O permissions for this port
```shell
sudo chmod a+rw [PORT]
```
- Setup ESP-IDF if you haven't done so already
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html
- Add idf.py to PATH
```shell
source /opt/esp-idf/export.fish
```
- Set the idf.py device as the ESP32
```shell
idf.py set-target esp32
```
- Set your WiFi Sniffing channel (default is 11)
```shell
idf.py menuconfig
```
- Component config > Sniffer Configuration > WiFi Channel
- Clean and build
``` shell
idf.py fullclean && idf.py build
```
- For this next step, do this for every ESP32 that you will designate as a sensor
---
- Flash
- This may error on the first try. If it does, try it again
```shell
idf.py -p [PORT] flash
```
- Optionally monitor debug output while ESP32 is plugged in
```shell
idf.py -p [PORT] monitor
```
### ESP32 Gateway
- Clone this repository on your local machine if you haven't done so already
```shell
git clone git@github.com:scast3/deauth_detect.git
cd deauth_detect/esp32sensor
```
- Plug in your ESP32 via USB to your machine
- Find which serial port the ESP32 is using
```shell
ls /dev/* | grep USB
```
- Set permissions I/O permissions for this port
```shell
sudo chmod a+rw [PORT]
```
- Setup ESP-IDF if you haven't done so already
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html
- Add idf.py to PATH
```shell
source /opt/esp-idf/export.fish
```
- Set the idf.py device as the ESP32
```shell
idf.py set-target esp32
```
- Clean and build
```shell
idf.py fullclean && idf.py build
```
- Flash
- This may error on the first try. If it does, try it again
```shell
idf.py -p [PORT] flash
```
- Optionally monitor debug output while ESP32 is plugged in
```shell
idf.py -p [PORT] monitor
```
