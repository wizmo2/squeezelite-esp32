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
set(BELL_EXTERNAL_TREMOR "idf::codecs")
set(BELL_EXTERNAL_CJSON "idf::json")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cspot ${CMAKE_CURRENT_BINARY_DIR}/cspot)
target_link_libraries(${COMPONENT_LIB} PRIVATE cspot ${EXTRA_REQ_LIBS})

