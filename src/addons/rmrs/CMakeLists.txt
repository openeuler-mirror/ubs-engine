file(GLOB_RECURSE RMRS_SRCS *.cpp *.h)
add_library(mempooling SHARED ${RMRS_SRCS})
add_library(mempooling_static EXCLUDE_FROM_ALL STATIC ${RMRS_SRCS})

add_library(rmrs_deps INTERFACE)
target_include_directories(
  rmrs_deps
  INTERFACE ${PROJECT_SOURCE_DIR}/src/include
            ${PROJECT_SOURCE_DIR}/deps/rapidjson/include
            ${PROJECT_SOURCE_DIR}/deps/rapidjson/include/rapidjson
            ${CMAKE_CURRENT_SOURCE_DIR}/common/export
            ${CMAKE_CURRENT_SOURCE_DIR}/common/exportV2
            ${CMAKE_CURRENT_SOURCE_DIR}/common/config
            ${CMAKE_CURRENT_SOURCE_DIR}/common/smap
            ${CMAKE_CURRENT_SOURCE_DIR}/common/executor
            ${CMAKE_CURRENT_SOURCE_DIR}/common/sync
            ${CMAKE_CURRENT_SOURCE_DIR}/common/heart
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common/libvirt
            ${CMAKE_CURRENT_SOURCE_DIR}/common/event
            ${CMAKE_CURRENT_SOURCE_DIR}/common/util
            ${CMAKE_CURRENT_SOURCE_DIR}/common/message/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common/message
            ${CMAKE_CURRENT_SOURCE_DIR}/mem_fragment/manager
            ${CMAKE_CURRENT_SOURCE_DIR}/mem_fragment/intranode_strategy
            ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/misc
            ${CMAKE_CURRENT_SOURCE_DIR}/../../framework/config
            ${CMAKE_CURRENT_SOURCE_DIR}/../../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../include/adapter_plugins/mti
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/interface
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/include
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/collect
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/page_file
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/mem_migrate_transport
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/vm_mem_migrate_strategy
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/election
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/fault_management
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/storage
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/ucache
            ${CMAKE_CURRENT_SOURCE_DIR}/over_commit/remote_mem_query
            ${CMAKE_CURRENT_SOURCE_DIR}/mem_fragment/fault_management
            ${CMAKE_CURRENT_SOURCE_DIR}/mem_fragment/internode_strategy
            ${CMAKE_CURRENT_SOURCE_DIR}/common/exportV2/LibvirtHelper
            ${CMAKE_CURRENT_SOURCE_DIR}/common/exportV2/OsHelper)

target_link_libraries(rmrs_deps INTERFACE rapidjson ubse_utils)

target_link_libraries(mempooling PUBLIC rmrs_deps)
target_link_libraries(mempooling_static PUBLIC rmrs_deps)

set_target_properties(mempooling PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                             ${CMAKE_BINARY_DIR}/lib)
set_target_properties(mempooling_static PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                                    ${CMAKE_BINARY_DIR}/lib)
