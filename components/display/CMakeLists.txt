idf_component_register(SRC_DIRS . core core/ifaces  fonts
						INCLUDE_DIRS . fonts core
						REQUIRES platform_config tools esp_common
						PRIV_REQUIRES services freertos driver           
						EMBED_FILES note.jpg
)

set_source_files_properties(display.c
    PROPERTIES COMPILE_FLAGS
   -Wno-format-overflow
)
