idf_component_register(SRC_DIRS . 
					PRIV_REQUIRES _override esp_common wifi-manager pthread squeezelite-ota platform_console telnet display targets
					LDFRAGMENTS "linker.lf"
                    	)
#get_target_property(ill ${COMPONENT_LIB} INTERFACE_LINK_LIBRARIES)
#message("${COMPONENT_LIB} INTERFACE_LINK_LIBRARIES = ${ill}")

