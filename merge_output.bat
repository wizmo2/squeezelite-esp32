set PYTHON=C:\Users\Mike\.espressif\python_env\idf4.4_py3.8_env\Scripts\python.exe
set IDF_PATH=C:\Users\Mike\esp\esp-idf
%PYTHON% %IDF_PATH%\tools\idf.py build -DDEPTH16
%PYTHON% %IDF_PATH%\components\esptool_py\esptool\esptool.py^
 --chip esp32s3 merge_bin -o build/output.bin^
 --flash_mode dio --flash_freq 40m --flash_size 4MB^
  0x0 build/bootloader/bootloader.bin 0x10000 build/recovery.bin 0x8000 build/partition_table/partition-table.bin 0xd000 build/ota_data_initial.bin 0x150000 build/squeezelite.bin 
copy "build\output.bin" "C:\Users\Mike\Documents\TAudio-case\builds\squeezelite-tembed.esp32s3.16.bin"
@rem %PYTHON% %IDF_PATH%\tools\idf_monitor.py -p COM7 -b 115200 --toolchain-prefix xtensa-esp32s3-elf- --target esp32s3 build\recovery.elf