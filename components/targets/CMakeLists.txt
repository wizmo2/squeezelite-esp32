# This should be made a pure CMake component but as CMake is even
# more shitty under Windows, backslash in path screws it all

if(CONFIG_MUSE)
	message("Compiling for MUSE") 
	set(src_dirs "muse")
else()
	set(src_dirs ".")		
endif()

idf_component_register( SRC_DIRS ${src_dirs}    				  
					    PRIV_REQUIRES services
)
