DEPTH=16
bash ./local_build.sh"
DEPTH=32
bash ./local_build.sh"
@echo 16bit Bin Sizes:
@type "build\size_squeezelite-16.txt"
@echo 32bit Bin Sizes:
@type "build\size_squeezelite-32.txt"

copy "build\squeezelite-*.bin" "C:\Users\Mike\Documents\TAudio-case\builds\"