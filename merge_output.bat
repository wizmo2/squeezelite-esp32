C:\Users\Mike\.espressif\python_env\idf4.4_py3.8_env\Scripts\python.exe C:\Users\Mike\esp\esp-idf\components\esptool_py\esptool\esptool.py^
 --chip esp32 merge_bin -o build/output.bin --flash_mode dio --flash_freq 40m --flash_size 4MB^
  0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xd000 build/ota_data_initial.bin 0x10000 build/recovery.bin 0x150000 build/squeezelite.bin
copy "build\output.bin" "C:\Users\Mike\Documents\TAudio-case\builds\squeezelite-taudio.esp32.16.bin"