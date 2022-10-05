# this must be set *before* idf_component_register
set(CMAKE_CXX_STANDARD 17)

idf_component_register(
		SRC_DIRS . 
		INCLUDE_DIRS . "cspot/include" "cspot/bell/include"
		PRIV_REQUIRES mbedtls mdns nvs_flash platform_config services esp_http_server tools codecs
		LDFRAGMENTS "linker.lf"
)

add_definitions(-DBELL_USE_MBEDTLS)
add_definitions(-Wno-unused-variable -Wno-unused-const-variable -Wchar-subscripts -Wunused-label -Wmaybe-uninitialized -Wmisleading-indentation)

set(BELL_DISABLE_CODECS ON)
set(BELL_DISABLE_SINKS ON)
set(CSPOT_TARGET_ESP32 ON)
# becase CMake is so broken, the cache set below overrides a normal "set" for the first build
set(BELL_EXTERNAL_TREMOR "idf::codecs" CACHE STRING "provide own codecs")
set(BELL_EXTERNAL_CJSON "idf::json" CACHE STRING "provide own CJSON")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cspot ${CMAKE_CURRENT_BINARY_DIR}/cspot)
target_link_libraries(${COMPONENT_LIB} PRIVATE cspot ${EXTRA_REQ_LIBS})

