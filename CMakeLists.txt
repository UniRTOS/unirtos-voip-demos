# Imported by the UniRTOS SDK external-application build.
if(DEFINED ENV{UNIRTOS_APP_TARGET_NAME})
    set(target $ENV{UNIRTOS_APP_TARGET_NAME})
else()
    get_filename_component(target ${CMAKE_CURRENT_SOURCE_DIR} NAME)
endif()

if(NOT TARGET unirtos-voip)
    message(FATAL_ERROR "VoIP library missing: run unirtos-cli env-setup with env_config.json")
endif()

# The shipped header is the single configuration entry point.
set(voip_config_header "${CMAKE_CURRENT_SOURCE_DIR}/voip_demo_config.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${voip_config_header}")
file(STRINGS "${voip_config_header}" voip_placeholder_lines
    REGEX "^#[ \t]*define[ \t]+VOIP_DEMO_(SERVER|PASSWORD)[ \t]+\"(YOUR_SIP_SERVER|YOUR_SIP_PASSWORD|REPLACE_ME)?\"")
if(voip_placeholder_lines)
    message(WARNING "VoIP configuration incomplete: edit VOIP_DEMO_SERVER and VOIP_DEMO_PASSWORD in ${voip_config_header}. This image will not auto-register while placeholders remain.")
endif()
unset(voip_placeholder_lines)

add_library(${target} STATIC
    voip_demo_entry.c
    voip_demo.c
    voip_sim.c
    voip_ringtone.c
    boards/eg800z_quecduino/voip_board.c
)
add_apps_libraries($<TARGET_FILE:${target}>)
set_target_properties(${target} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${out_unir_lib_dir})
target_include_directories(${target} PRIVATE
    ${SOURCE_TOP_DIR}
    ${SOURCE_SYS_DEBUG_DIR}
    ${SOURCE_SYS_OS_DIR}
    ${SOURCE_COMPONENTS_PUBLIC_INC}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/eg800z_quecduino
)
target_link_libraries(${target} PRIVATE unirtos-voip)
if(CONFIG_QOSA_LINK_SYSTEM_LIBRARIES_FUNC)
    target_link_libraries(${target} PRIVATE system_os)
endif()
