# this must be set *before* idf_component_register
set(CMAKE_CXX_STANDARD 17)

idf_component_register(
		SRC_DIRS . 
		INCLUDE_DIRS . "cspot/include" "cspot/bell/include" "cspot/protos"
		PRIV_REQUIRES mbedtls mdns nvs_flash platform_config services esp_http_server tools
		LDFRAGMENTS "linker.lf"
)

include_directories("../codecs/inc")
add_definitions(-DBELL_USE_MBEDTLS)
add_definitions(-Wno-unused-variable -Wno-unused-const-variable -Wchar-subscripts -Wunused-label -Wmaybe-uninitialized -Wmisleading-indentation)

set(BELL_DISABLE_CODECS 1)
set(BELL_TREMOR_EXTERNAL "idf::codecs")
set(BELL_CJSON_EXTERNAL "idf::json")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cspot ${CMAKE_CURRENT_BINARY_DIR}/cspot)
target_link_libraries(${COMPONENT_LIB} PRIVATE cspot ${EXTRA_REQ_LIBS})

#if (!WIN32)
#	message(${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_protos.sh)
#	execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_protos.sh" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
#endif ()



