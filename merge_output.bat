
set PROJECT_NAME=tembed
@rem set PROJECT_NAME=pure
set DEPTH=32
idf.py build -DDEPTH%DEPTH%
set PYTHON=C:\Users\Mike\.espressif\python_env\idf4.4_py3.8_env\Scripts\python.exe
set IDF_PATH=C:\Users\Mike\esp\esp-idf
%PYTHON% %IDF_PATH%\components\esptool_py\esptool\esptool.py^
 --chip esp32s3 merge_bin -o build/squeezelite-%PROJECT_NAME%.esp32s3.%DEPTH%.bin^
 --flash_mode dio --flash_freq 80m --flash_size 4MB^
  0x0 build/bootloader/bootloader.bin 0x10000 build/recovery.bin 0x8000 build/partition_table/partition-table.bin 0xd000 build/ota_data_initial.bin 0x150000 build/squeezelite.bin 
copy "build\squeezelite-*.bin" "..\TAudio-case\builds\"



@rem %PYTHON% %IDF_PATH%\tools\idf_monitor.py -p COM7 -b 115200 --toolchain-prefix xtensa-esp32s3-elf- --target esp32s3 build\recovery.elf
@rem C:\Users\Mike\esp\esp-idf\components\esptool_py\esptool;C:\Users\Mike\esp\esp-idf\components\espcoredump;C:\Users\Mike\esp\esp-idf\components\partition_table;C:\Users\Mike\.espressif\tools\xtensa-esp-elf-gdb\11.2_20220823\xtensa-esp-elf-gdb\bin;C:\Users\Mike\.espressif\tools\riscv32-esp-elf-gdb\11.2_20220823\riscv32-esp-elf-gdb\bin;C:\Users\Mike\.espressif\tools\xtensa-esp32-elf\esp-2021r2-patch5-8.4.0\xtensa-esp32-elf\bin;C:\Users\Mike\.espressif\tools\xtensa-esp32s2-elf\esp-2021r2-patch5-8.4.0\xtensa-esp32s2-elf\bin;C:\Users\Mike\.espressif\tools\xtensa-esp32s3-elf\esp-2021r2-patch5-8.4.0\xtensa-esp32s3-elf\bin;C:\Users\Mike\.espressif\tools\riscv32-esp-elf\esp-2021r2-patch5-8.4.0\riscv32-esp-elf\bin;C:\Users\Mike\.espressif\tools\esp32ulp-elf\2.35_20220830\esp32ulp-elf\bin;C:\Users\Mike\.espressif\tools\cmake\3.23.1\bin;C:\Users\Mike\.espressif\tools\openocd-esp32\v0.11.0-esp32-20221026\openocd-esp32\bin;C:\Users\Mike\.espressif\tools\ninja\1.10.2;C:\Users\Mike\.espressif\tools\idf-exe\1.0.3;C:\Users\Mike\.espressif\tools\ccache\4.3\ccache-4.3-windows-64;C:\Users\Mike\.espressif\tools\dfu-util\0.9\dfu-util-0.9-win64;C:\Users\Mike\.espressif\python_env\idf4.4_py3.8_env\Scripts;C:\Users\Mike\esp\esp-idf\tools;C:\Users\Mike\.espressif\tools\idf-git\2.30.1\cmd;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0\;C:\Windows\System32\OpenSSH\;C:\Program Files\Git\cmd;C:\Program Files\dotnet\;C:\Program Files\JAI\SDK\bin;C:\Program Files\JAI\SDK\bin\Win32_i86;C:\Program Files\JAI\SDK\GenICam\bin\Win64_x64;C:\Program Files\JAI\SDK\GenICam\bin\Win32_i86;C:\Program Files (x86)\Windows Live\Shared;C:\Program Files\Docker\Docker\resources\bin;C:\Users\Mike\scoop\shims;C:\Users\Mike\AppData\Local\Microsoft\WindowsApps;;C:\Users\Mike\.dotnet\tools;C:\Users\Mike\AppData\Local\Programs\Microsoft VS Code\bin
