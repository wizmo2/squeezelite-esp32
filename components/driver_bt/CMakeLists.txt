message(STATUS, "driver_bt: PURE:${PURE} NOPURE${NOPURE}")
#if (DEFINED PURE)
if (NOT DEFINED NOPURE)
    message(STATUS "*** PURE: Skipping bluetooth")
    return()
endif()

if(IDF_TARGET STREQUAL "esp32")
    idf_component_register(	SRC_DIRS .
                            INCLUDE_DIRS .
                            PRIV_REQUIRES services bt display console tools platform_config

)
endif()

