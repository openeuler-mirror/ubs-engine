add_library(mock_libssu SHARED mock_libssu.cpp)

target_link_libraries(mock_libssu PRIVATE rapidjson)

target_include_directories(mock_libssu PRIVATE ${CMAKE_BINARY_DIR}/include)
target_include_directories(mock_libssu PRIVATE ${CMAKE_SOURCE_DIR}/src/include/adapter_plugins/ssu)

set_target_properties(mock_libssu PROPERTIES
        OUTPUT_NAME "ssu"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/mock_ssu_config_path.h.in"
        "${CMAKE_BINARY_DIR}/include/mock_ssu_config_path.h"
        @ONLY
)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/mock_ssu_config.json"
        "${CMAKE_BINARY_DIR}/bin/mock_ssu_config.json"
        COPYONLY
)


