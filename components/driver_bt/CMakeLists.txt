if(IDF_TARGET STREQUAL "esp32")
    idf_component_register(	SRC_DIRS .
                            INCLUDE_DIRS .
                            PRIV_REQUIRES services bt display console tools platform_config

)
endif()

