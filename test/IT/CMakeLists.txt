if(NOT TARGET ubse)
    message(FATAL_ERROR "IT framework requires the production ubse target")
endif()

get_target_property(UBSE_DAEMON_SOURCE_DIR ubse SOURCE_DIR)
get_target_property(UBSE_DAEMON_TARGET_SOURCES ubse SOURCES)
if(NOT UBSE_DAEMON_TARGET_SOURCES)
    message(FATAL_ERROR "Unable to derive IT daemon sources from the ubse target")
endif()
set(IT_DAEMON_SOURCES)
foreach(source IN LISTS UBSE_DAEMON_TARGET_SOURCES)
    if(IS_ABSOLUTE "${source}")
        list(APPEND IT_DAEMON_SOURCES "${source}")
    else()
        list(APPEND IT_DAEMON_SOURCES "${UBSE_DAEMON_SOURCE_DIR}/${source}")
    endif()
endforeach()

# =====================================================
# Directory layout:
#   infra/      - Framework infrastructure (spec, cluster, node, client, process, config, mock, helper, test_infra, launcher)
#   scenarios/  - Scenario definitions (cluster lifecycle per TestSuite)
#   tests/      - Test cases organized by business domain
#   stubs/      - Mock/Stub libraries for hardware dependency replacement
# =====================================================

set(IT_INFRA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/infra)
set(IT_SCENARIOS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scenarios)
set(IT_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# IT test infrastructure library
add_library(ubse_it_infra STATIC
    ${IT_INFRA_DIR}/spec/it_cluster_spec.cpp
    ${IT_INFRA_DIR}/node/it_node.cpp
    ${IT_INFRA_DIR}/cluster/it_cluster.cpp
    ${IT_INFRA_DIR}/cluster/it_cluster_builder.cpp
    ${IT_INFRA_DIR}/client/it_sdk_client.cpp
    ${IT_INFRA_DIR}/client/it_cli_invoker.cpp
    ${IT_INFRA_DIR}/config/it_config_builder.cpp
    ${IT_INFRA_DIR}/process/node_process_manager.cpp
    ${IT_INFRA_DIR}/mock/mock_lcne_server.cpp
    ${IT_INFRA_DIR}/mock/mock_lcne_xml.cpp
    ${IT_INFRA_DIR}/helper/it_wait_helper.cpp
    ${IT_INFRA_DIR}/helper/it_xalarm_helper.cpp
)

add_executable(ubse_it_node_launcher EXCLUDE_FROM_ALL
    ${IT_INFRA_DIR}/launcher/node_launcher.cpp
)
target_compile_options(ubse_it_node_launcher PRIVATE ${DEBUG_FLAGS})

target_include_directories(ubse_it_infra PUBLIC
    ${IT_INFRA_DIR}/spec
    ${IT_INFRA_DIR}/node
    ${IT_INFRA_DIR}/cluster
    ${IT_INFRA_DIR}/client
    ${IT_INFRA_DIR}/process
    ${IT_INFRA_DIR}/config
    ${IT_INFRA_DIR}/mock
    ${IT_INFRA_DIR}/helper
    ${IT_INFRA_DIR}/test_infra
    ${CMAKE_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/framework/ha
)

target_link_libraries(ubse_it_infra PUBLIC
    ubse_sdk_static
    cpp-httplib
    GTest::gtest
)

target_compile_options(ubse_it_infra PRIVATE ${DEBUG_FLAGS})

# Generate IT_DIRECTORY macro
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/ubse_it_dir.h.in"
    "${CMAKE_BINARY_DIR}/include/ubse_it_dir.h"
    @ONLY
)

# Build stub libraries
add_subdirectory(stubs)

# =====================================================
# IT daemon binary with mock SMBIOS
# Same source files as ubse, but links ubse_smbios_mock via --whole-archive
# to override real ubse_smbios symbols. --allow-multiple-definition
# lets the linker accept duplicate symbols and use the first occurrence (mock).
# =====================================================

add_executable(ubse_it_daemon EXCLUDE_FROM_ALL
    ${IT_DAEMON_SOURCES}
)

# mock SMBIOS must appear first in link order so its symbols override real ones
target_link_libraries(ubse_it_daemon PRIVATE
        -Wl,--whole-archive
        ubse_smbios_mock
        ubse_mti_mock
        ubse_node_controller_it_mock
        ubse_com_engine_it_mock
        ubse_capability_it_stub
        ubse_npu_controller_it_mock
        -Wl,--no-whole-archive
        ubse_security
        ubse_context
        ubse_config
        ubse_com
        ubse_event
        ubse_storage
        ubse_log
        ubse_plugin
        ubse_http
        ubse_election
        ubse_mti
        ubse_node_controller
        ubse_ras
        syssentry
        cert_api_static
        api_server
        rapidjson
        trace_context
        ubse_urma_uvs
        ubse_urma_controller
        npu_controller
        ubse_controller
        -Wl,--whole-archive
        ubse_node_controller_register_config
        -Wl,--no-whole-archive
)

# Duplicate symbols are expected: mock vs real ubse_smbios. Linker uses first occurrence.
target_link_options(ubse_it_daemon PRIVATE
        -Wl,--allow-multiple-definition
        -rdynamic
        -pie
)

target_compile_options(ubse_it_daemon PRIVATE ${DEBUG_FLAGS})
target_include_directories(ubse_it_daemon PRIVATE ${CMAKE_BINARY_DIR}/rpc)
add_dependencies(ubse_it_daemon ubse_it_node_launcher)

add_custom_target(ubse_it_runtime)
add_dependencies(ubse_it_runtime
    ubsectl
    ubse_it_daemon
    ubse_it_node_launcher
    ubse_interface_preload
    obmm_stub
    urma_uvs_stub
    xalarm_stub
    mock_plugin
)

# =====================================================
# Shared test logic (cases modules) — compiled as static libraries
# =====================================================
add_subdirectory(${IT_TESTS_DIR} ${CMAKE_BINARY_DIR}/test/IT/cases)

# =====================================================
# Scene-driven IT tests — one binary per scene
# =====================================================
set(UBSE_IT_NON_MODULE_DIRS
    infra
    scenarios
    stubs
    tests
)

file(GLOB UBSE_IT_CHILDREN LIST_DIRECTORIES true RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")
foreach(child IN LISTS UBSE_IT_CHILDREN)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${child}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${child}/CMakeLists.txt")
        list(FIND UBSE_IT_NON_MODULE_DIRS "${child}" non_module_index)
        if(non_module_index EQUAL -1)
            message(FATAL_ERROR "Unexpected directory with CMakeLists.txt: '${child}' — add to UBSE_IT_NON_MODULE_DIRS if intentional")
        endif()
    endif()
endforeach()

set(UBSE_IT_SCENARIOS
    tongsuan_1d_single_node
    tongsuan_1d_two_nodes
    tongsuan_1d_two_nodes_election_candidate_false
    tongsuan_1d_four_nodes
    zhisuan_npu_single_node
)

foreach(scene IN LISTS UBSE_IT_SCENARIOS)
    if(NOT EXISTS "${IT_SCENARIOS_DIR}/${scene}/CMakeLists.txt")
        message(FATAL_ERROR "Registered IT scene '${scene}' does not have CMakeLists.txt in ${IT_SCENARIOS_DIR}/${scene}")
    endif()
    add_subdirectory(${IT_SCENARIOS_DIR}/${scene} ${CMAKE_BINARY_DIR}/test/IT/scene_${scene})
endforeach()

get_property(IT_SCENE_BINARIES GLOBAL PROPERTY UBSE_IT_SCENE_BINARIES)

set(IT_SCENE_BINARY_PATHS "")
foreach(b ${IT_SCENE_BINARIES})
    list(APPEND IT_SCENE_BINARY_PATHS ${CMAKE_BINARY_DIR}/bin/${b})
endforeach()

if(SKIP_RUN_TESTS)
    set(IT_RUN_ALL_COMMAND ${CMAKE_COMMAND} -E echo "Skip run IT tests, only build scene binaries")
elseif(NOT IT_SCENE_BINARY_PATHS)
    set(IT_RUN_ALL_COMMAND ${CMAKE_COMMAND} -E echo "No IT scene binaries registered" && ${CMAKE_COMMAND} -E false)
else()
    set(IT_RUN_ALL_COMMAND
        ${CMAKE_SOURCE_DIR}/scripts/run_all_it_tests.sh
        ${CMAKE_BINARY_DIR}/coverage/it
        ${IT_SCENE_BINARY_PATHS}
    )
endif()
add_custom_target(it
    COMMAND ${IT_RUN_ALL_COMMAND}
    COMMENT "Run all scene-driven IT tests (bash build.sh it)"
)

if(IT_SCENE_BINARIES)
    add_dependencies(it ${IT_SCENE_BINARIES})
else()
    message(WARNING "No IT scene binaries registered.")
endif()
