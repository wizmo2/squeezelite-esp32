#!/bin/bash



echo "Build process started"
echo "Setting up build name and build number"
if [ -z "${TARGET_BUILD_NAME}" ]
then
    export TARGET_BUILD_NAME="I2S-4MFlash"
    echo "TARGET_BUILD_NAME is not set. Defaulting to ${TARGET_BUILD_NAME}"
fi
if [ -z "${BUILD_NUMBER}" ]
then
    export BUILD_NUMBER="500"
    echo "BUILD_NUMBER is not set. Defaulting to ${BUILD_NUMBER}"
fi
if [ -z "$DEPTH" ]
then
    export DEPTH="16"
    echo "DEPTH is not set. Defaulting to ${DEPTH}"
fi
if [ -z "$tag" ]
then
    branch_name="$(git rev-parse --abbrev-ref HEAD)"
    branch_name="${branch_name//[^a-zA-Z0-9\-~!@_\.]/}"
    app_name="${TARGET_BUILD_NAME}.${DEPTH}.dev-$(git log --pretty=format:'%h' --max-count=1).${branch_name}" 
    echo "${app_name}">version.txt
    echo "app_name is not set. Defaulting to ${app_name}"
else
    echo "${tag}" >version.txt
fi

echo "Copying target sdkconfig"
cp build-scripts/${TARGET_BUILD_NAME}-sdkconfig.defaults sdkconfig
echo "Building project"
idf.py build -DDEPTH=${DEPTH} -DBUILD_NUMBER=${BUILD_NUMBER}-${DEPTH} 
echo "Generating size report"
idf.py size-components >build/size_components.txt
idf.py size-components-squeezelite build/size_components_squeezelite.txt
if [ -z "${artifact_file_name}" ]
then
    echo "No artifact file name set.  Will not generate zip file."
else
    echo "Generating build artifact zip file"
    zip -r build_output.zip build
    zip build/${artifact_file_name} partitions*.csv components/ build/*.bin build/bootloader/bootloader.bin build/partition_table/partition-table.bin build/flash_project_args build/size_*.txt
fi
