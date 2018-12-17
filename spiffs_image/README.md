
Build image using mkspiffs tool from https://github.com/igrr/mkspiffs
$ ../../mkspiffs/mkspiffs -c ~/esp/disPOD/spiffs_image/image -s 0x36F000 -b 4096 -p 256 spiffs_image.img

List content
$ ../../mkspiffs/mkspiffs -l -b 4096 spiffs_image.img

Upload SPIFFS
$ python /home/AKAEM/esp/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port COM6 --baud 115200 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 40m --flash_size detect --no-compress --verify 0xc91000 ./spiffs_image.img

Update partition table
$ make flash
(Remark: check during boot up sequence)