docker run --rm -e DEPTH=16 -v %cd%:/project -w /project -it sle118/squeezelite-esp32-idfv435 /bin/bash -c "./local_build.sh"
docker run --rm -e DEPTH=32 -v %cd%:/project -w /project -it sle118/squeezelite-esp32-idfv435 /bin/bash -c "./local_build.sh"
@echo 16bit Bin Sizes:
@type "build\size_squeezelite-16.txt"
@echo 32bit Bin Sizes:
@type "build\size_squeezelite-32.txt"

copy "build\squeezelite-*.bin" "C:\Users\Mike\Documents\TAudio-case\builds\"