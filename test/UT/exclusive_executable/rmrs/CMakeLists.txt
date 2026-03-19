file(
  GLOB_RECURSE RMRS_TEST_SOURCES
  LIST_DIRECTORIES false
  *.h *.cpp)

add_executable(${CMAKE_PROJECT_NAME}_rmrs_plugin_ut EXCLUDE_FROM_ALL
               ${RMRS_TEST_SOURCES})

set_target_properties(${CMAKE_PROJECT_NAME}_rmrs_plugin_ut
                      PROPERTIES LINK_FLAGS "-Wl,--as-needed")

target_compile_options(
  ${CMAKE_PROJECT_NAME}_rmrs_plugin_ut
  PRIVATE -fPIC
          -w
          -g
          -Wno-all
          -Wno-error
          -Wno-non-template-friend
          -Wno-template-id-mismatch
          -Wno-invalid-offsetof
          -fno-access-control)

target_include_directories(
  ${CMAKE_PROJECT_NAME}_rmrs_plugin_ut BEFORE
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/deps/mockcpp/include
    ${PROJECT_SOURCE_DIR}/deps/mockcpp/include/mockcpp
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/deps/rapidjson/include
    ${PROJECT_SOURCE_DIR}/deps/rapidjson/include/rapidjson
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/export
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/exportV2
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/smap
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/executor
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/sync
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/heart
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/libvirt
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/event
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/util
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/message/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/message
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/mem_fragment/manager
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/mem_fragment/intranode_strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/framework/misc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/framework/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/include/adapter_plugins/mti
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/collect
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/page_file
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/mem_migrate_transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/vm_mem_migrate_strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/election
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/fault_management
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/ucache
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/over_commit/remote_mem_query
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/mem_fragment/fault_management
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/mem_fragment/internode_strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/exportV2/LibvirtHelper
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/rmrs/common/exportV2/OsHelper
)

target_link_options(${CMAKE_PROJECT_NAME}_rmrs_plugin_ut PRIVATE -Wl,--relax
                    --coverage -rdynamic -Wl,--as-needed)

target_compile_definitions(${CMAKE_PROJECT_NAME}_rmrs_plugin_ut
                           PRIVATE UB_ENVIRONMENT ENABLE_COV=OFF)

target_link_libraries(
  ${CMAKE_PROJECT_NAME}_rmrs_plugin_ut
  PUBLIC test_exclusive_main mempooling_static rapidjson GTest::gmock_main
         mockcpp boundscheck)

add_independent_exec_ut(${CMAKE_PROJECT_NAME}_rmrs_plugin_ut)
