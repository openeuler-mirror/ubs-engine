aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SMBIOS_SRC)
add_library(ubse_smbios STATIC ${SMBIOS_SRC})
target_include_directories(ubse_smbios PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${SRC_DIR}/framework/security ${SRC_DIR}/framework/misc/lock)
target_link_libraries(ubse_smbios PUBLIC ubse_security ubse_utils)
