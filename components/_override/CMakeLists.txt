if(IDF_TARGET STREQUAL esp32)
    set(lib_dir ${build_dir}/esp-idf)
    set(driver esp32/i2s.c esp32/i2s_hal.c esp32/spi_bus_lock.c)
    string(REPLACE ".c" ".c.obj" driver_obj "${driver}")

    idf_component_register( SRCS ${driver}
                        REQUIRES driver           
                        INCLUDE_DIRS ${IDF_PATH}/components/driver
                        PRIV_INCLUDE_DIRS ${IDF_PATH}/components/driver/include/driver
    )

    # CMake is just a pile of crap
    message(STATUS "!! overriding ${driver} !!")
    message(STATUS "CAREFUL, LIBRARIES STRIPPING FROM DUPLICATED COMPONENTS DEPENDS ON THIS BEING REBUILD")

    add_custom_command(
                TARGET ${COMPONENT_LIB}
                PRE_LINK
                COMMAND xtensa-esp32-elf-ar -d ${lib_dir}/driver/libdriver.a ${driver_obj}
                VERBATIM
    )
endif()
