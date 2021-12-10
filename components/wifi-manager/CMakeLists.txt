
set( WEBPACK_DIR webapp/webpack/dist )
idf_component_register( SRC_DIRS . webapp UML-State-Machine-in-C/src
						INCLUDE_DIRS . webapp UML-State-Machine-in-C/src ${IDF_PATH}/components/esp_http_server/src ${IDF_PATH}/components/esp_http_server/src/port/esp32  ${IDF_PATH}/components/esp_http_server/src/util ${IDF_PATH}/components/esp_http_server/src/
						REQUIRES squeezelite-ota json mdns 
						PRIV_REQUIRES tools services platform_config esp_common json newlib freertos  spi_flash nvs_flash mdns pthread wpa_supplicant platform_console esp_http_server console driver_bt
)

include(webapp/webapp.cmake)