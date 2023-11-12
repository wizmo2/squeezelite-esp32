# for the forgetful, REQUIRES cannot use CONFIG_XXX due to parsing order
if(IDF_TARGET STREQUAL "esp32")
    set(bt_requires "driver_bt")
endif()

message(STATUS, "squeezelite: PURE:${PURE} NOPURE${NOPURE}")
# runs prelink with no d3finition, then with definition.
if (NOT DEFINED PURE)
    message(STATUS "*** Squeezelite: PURE is NOT defined ***")
	set(cspot_required "spotify")
	set(airplay_required "raop")
else()
	message(STATUS "*** Squeezelite: PURE is defined ***")
endif()

idf_component_register( SRC_DIRS . external ac101 tas57xx wm8978
						INCLUDE_DIRS .  ac101 
						PRIV_REQUIRES 	
									codecs
									newlib 
									esp_common 
									esp-dsp
						  			platform_config 
						 			services 
						 			display
						 			tools
						 			audio
									led_strip
									_override
									${cspot_required}
									${airplay_required}
									${bt_requires}  
									adc                                  
						EMBED_FILES vu_s.data arrow.data
)

set_source_files_properties(mad.c pcm.c flac.c alac.c helix-aac.c vorbis.c opus.c
    PROPERTIES COMPILE_FLAGS
    -Wno-maybe-uninitialized 
)

set_source_files_properties(wm8978/wm8978.c
    PROPERTIES COMPILE_FLAGS
    -Wno-unused-function
)	

add_definitions(-DLINKALL -DLOOPBACK -DNO_FAAD -DEMBEDDED -DTREMOR_ONLY -DCUSTOM_VERSION=${BUILD_NUMBER})

message(STATUS "*** Squeezelite: DEPTH is ${DEPTH} ***")
if (${DEPTH} EQUAL "32")
	add_definitions(-DBYTES_PER_FRAME=8)
else()	
	add_definitions(-DRESAMPLE16 -DBYTES_PER_FRAME=4)
endif()	

if (NOT DEFINED AAC_DISABLED_SBR)
	add_definitions(-DAAC_ENABLE_SBR)
endif()	

add_compile_options (-O3 ) 
