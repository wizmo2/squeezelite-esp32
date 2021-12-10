idf_component_register(SRCS operator.cpp utf8.c   trace.c
						REQUIRES esp_common  pthread 
                    	INCLUDE_DIRS .
						PRIV_REQUIRES services freertos pthread esp_common
                    	)

#doing our own implementation of new operator for some pre-compiled binaries
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _ZdlPv")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _Znwj")

  