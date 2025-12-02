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
./pi_setup.sh
```
- Build the C++ program on your Raspberry Pi
```shell
g++ rpi/src/main.cpp rpi/src/esp32_to_uart.cpp -o rpi/build/deauthdetect \ 
	-lduckdb -I /usr/local/include -L /usr/local/lib \ 
```
- Run the program
```shell
rpi/build/deauthdetect
```
### ESP32 Sensor
### ESP32 Gateway
