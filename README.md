**Set perms to access USB if not already**
sudo chmod a+rw PORT

**After opening a new terminal window**
source /opt/esp-idf/export.fish

**Set target if not set**
idf.py set-target esp32

**set channel**
idf.py menuconfig
Component config > Sniffer Configuration > WiFi Channel

**clean**
idf.py fullclean

**build and flash**
idf.py build && idf.py -p PORT flash

**see command output**
idf.py monitor

**erase flash -> optional**
idf.py -p PORT erase-flash
