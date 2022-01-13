idf_component_register( SRCS 
							cmd_i2ctools.c
							cmd_nvs.c
							cmd_ota.c
							cmd_system.c
							cmd_wifi.c
							platform_console.c
							cmd_config.c
						INCLUDE_DIRS .   
						REQUIRES nvs_flash 
						PRIV_REQUIRES console app_update tools services spi_flash  platform_config vfs pthread wifi-manager platform_config newlib  telnet display squeezelite tools)
target_link_libraries(${COMPONENT_LIB}   "-Wl,--undefined=GDS_DrawPixelFast")
target_link_libraries(${COMPONENT_LIB} ${build_dir}/esp-idf/$<TARGET_PROPERTY:RECOVERY_PREFIX>/lib$<TARGET_PROPERTY:RECOVERY_PREFIX>.a 	)
target_add_binary_data( __idf_platform_console presets.json BINARY)

set_source_files_properties(cmd_config.c
    PROPERTIES COMPILE_FLAGS
    -Wno-unused-function
)	

