if (NOT BUILD_TESTS)
    return()
endif()

# mock_plugin - unified mock plugin for IT test
# Simulates: mempooling (PANIC/REBOOT fault migration) + virt_agent (OOM escape borrow)
add_library(mock_plugin SHARED mock_plugin.cpp)

target_include_directories(mock_plugin PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/include/adapter_plugins/mti
        ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti
        ${PROJECT_SOURCE_DIR}/src/adapter_plugins/mti/lcne
        ${PROJECT_SOURCE_DIR}/src/framework/context
        ${PROJECT_SOURCE_DIR}/src/framework/election
        ${CMAKE_BINARY_DIR}/include
)

set_target_properties(mock_plugin PROPERTIES
    PREFIX "lib"
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

target_link_libraries(mock_plugin PRIVATE
        -Wl,--no-as-needed
        -Wl,--allow-shlib-undefined
)
