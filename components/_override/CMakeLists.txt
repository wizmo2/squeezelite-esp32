set(srcs i2s.c i2s_hal.c spi_master.c)
idf_component_register( SRCS ${srcs}
						INCLUDE_DIRS ${IDF_PATH}/components/driver
)

message("overriding ${srcs} !! THIS MUST BE REQUIRED BY MAIN !!")