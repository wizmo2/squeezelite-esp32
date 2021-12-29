cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS components/platform_console/app_recovery components/platform_console/app_squeezelite  )
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_definitions(-DMODEL_NAME=SqueezeESP32)

# State machine hierarchy enabled and logging enabled
add_definitions(-DSTATE_MACHINE_LOGGER=1)
add_definitions(-DHIERARCHICAL_STATES=1)

# Uncomment line below to get memory usage trace details
#add_definitions(-DENABLE_MEMTRACE=1)
#uncomment line below to get network ethernet debug logs 
#add_definitions(-DNETWORK_ETHERNET_LOG_LEVEL=ESP_LOG_DEBUG)
#uncomment line below to get network status debug logs
#add_definitions(-DNETWORK_STATUS_LOG_LEVEL=ESP_LOG_DEBUG)
#add_definitions(-DNETWORK_HANDLERS_LOG_LEVEL=ESP_LOG_DEBUG)
#add_definitions(-DNETWORK_WIFI_LOG_LEVEL=ESP_LOG_DEBUG)
#add_definitions(-DNETWORK_MANAGER_LOG_LEVEL=ESP_LOG_DEBUG)
#add_definitions(-DNETWORK_HTTP_SERVER_LOG_LEVEL=ESP_LOG_DEBUG)

if(NOT DEFINED DEPTH)
	set(DEPTH "16")
endif()	
message(STATUS  "Building RECOVERY")
project(recovery)
set_property(TARGET recovery.elf PROPERTY RECOVERY_PREFIX app_recovery )
include(squeezelite.cmake)
set(PROJECT_VER $ENV{PROJECT_VER})

