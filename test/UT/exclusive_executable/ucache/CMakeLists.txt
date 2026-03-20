file(
  GLOB_RECURSE UCACHE_TEST_SOURCES
  LIST_DIRECTORIES false
  *.h *.cpp)

add_executable(${CMAKE_PROJECT_NAME}_ucache_plugin_ut EXCLUDE_FROM_ALL
               ${UCACHE_TEST_SOURCES})

set_target_properties(${CMAKE_PROJECT_NAME}_ucache_plugin_ut
                      PROPERTIES LINK_FLAGS "-Wl,--as-needed")

target_compile_options(
  ${CMAKE_PROJECT_NAME}_ucache_plugin_ut
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
  ${CMAKE_PROJECT_NAME}_ucache_plugin_ut BEFORE
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/deps/mockcpp/include
    ${PROJECT_SOURCE_DIR}/deps/mockcpp/include/mockcpp
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/deps/rapidjson/include
    ${PROJECT_SOURCE_DIR}/deps/rapidjson/include/rapidjson
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/common/util
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/ucache_agent
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/ucache_agent/agent_task_processor
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/ucache_master
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/ucache_master/bottleneck_strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/ucache_master/data_collect
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/ucache_master/master_task_controller
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/ucache_master/node_fault_handler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/ucache/ucache_master/rack_msg
)

target_link_options(${CMAKE_PROJECT_NAME}_ucache_plugin_ut PRIVATE
                    --coverage -rdynamic -Wl,--as-needed)

target_compile_definitions(
  ${CMAKE_PROJECT_NAME}_ucache_plugin_ut PRIVATE UB_ENVIRONMENT ENABLE_COV=OFF)

target_link_libraries(
  ${CMAKE_PROJECT_NAME}_ucache_plugin_ut
  PUBLIC test_exclusive_main ucache_plugin_static rapidjson GTest::gmock_main mockcpp
         boundscheck ubse_log)

add_independent_exec_ut(${CMAKE_PROJECT_NAME}_ucache_plugin_ut)
