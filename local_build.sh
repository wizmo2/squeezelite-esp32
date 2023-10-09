#!/bin/bash
echo "Build process started"
echo "Setting up build name and build number"
if [ -z "${TARGET_BUILD_NAME}" ]
then
    export TARGET_BUILD_NAME="tembed"
    echo "TARGET_BUILD_NAME is not set. Defaulting to ${TARGET_BUILD_NAME}"
fi
if [ -z "${BUILD_NUMBER}" ]
then
    export BUILD_NUMBER="1620"
    echo "BUILD_NUMBER is not set. Defaulting to ${BUILD_NUMBER}"
fi
if [ -z "$DEPTH" ]
then
    export DEPTH="16"
    echo "DEPTH is not set. Defaulting to ${DEPTH}"
fi
if [ -z "$IDF_TARGET" ]
then
    export IDF_TARGET="esp32s3"
    echo "IDF_TARGET is not set. Defaulting to ${IDF_TARGET}"
fi

if [ -z "$tag" ]
then
    app_name="squeezelite-${TARGET_BUILD_NAME}.${IDF_TARGET}.${DEPTH}" 
    version=$(date '+%Y.%m.%d')
    echo "${app_name}-v${version}" >version.txt
    echo "app_name is not set. Defaulting to ${app_name}"
else
    echo "${tag}" >version.txt
fi

echo "Copying target sdkconfig"
cp build-scripts/${TARGET_BUILD_NAME}-sdkconfig.defaults sdkconfig
echo "Building project"
idf.py build -DDEPTH=${DEPTH} -DBUILD_NUMBER=${BUILD_NUMBER}-${DEPTH} 
echo "Generating size report"
#idf.py size-components >build/size_components.txt
ls -l build/*.bin >"build/size_squeezelite-${DEPTH}.txt"

output_zip_file="${app_name}.zip"
output_bin_file="${app_name}.bin"
echo "Generating merged zip file. ${output_zip_file}"
zip build/${output_zip_file} partitions*.csv components/ build/*.bin build/bootloader/bootloader.bin build/partition_table/partition-table.bin build/flash_project_args build/size_*.txt
echo "Generating merged file. ${output_bin_file}"
esptool.py --chip ${IDF_TARGET} merge_bin -o build/${output_bin_file} --flash_mode dio --flash_size 4MB 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xd000 build/ota_data_initial.bin 0x10000 build/recovery.bin 0x150000 build/squeezelite.bin