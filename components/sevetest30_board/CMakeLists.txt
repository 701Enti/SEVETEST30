set(COMPONENT_ADD_INCLUDEDIRS ./include)

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES audio_sal audio_hal esp_peripherals)

if(CONFIG_SEVETEST30_BOARD)
message(STATUS "Current board name is " CONFIG_AUDIO_BOARD_CUSTOM)
list(APPEND COMPONENT_ADD_INCLUDEDIRS .)
set(COMPONENT_SRCS
./board.c
./board_pins_config.c
./board_ctrl.h
./board_pins_config.h
./base64_re.h
./base64_re.c
)
endif()

register_component()

IF (IDF_VERSION_MAJOR GREATER 3)
idf_component_get_property(audio_board_lib audio_board COMPONENT_LIB)
set_property(TARGET ${audio_board_lib} APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${COMPONENT_LIB})

ELSEIF (IDF_VERSION_MAJOR EQUAL 3)
set_property(TARGET idf_component_audio_board APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES  $<TARGET_PROPERTY:${COMPONENT_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)

ENDIF (IDF_VERSION_MAJOR GREATER 3)

