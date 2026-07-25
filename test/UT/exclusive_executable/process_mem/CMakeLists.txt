file(GLOB_RECURSE PROCESS_MEM_TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)
list(FILTER PROCESS_MEM_TEST_SOURCES EXCLUDE REGEX ".*/mock/.*")
file(GLOB MOCK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mock/ubse/*.cpp)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/addons/process_mem)
add_executable(${CMAKE_PROJECT_NAME}_process_mem_ut EXCLUDE_FROM_ALL
               ${PROCESS_MEM_TEST_SOURCES} ${MOCK_SOURCES}
               ${SRC_DIR}/process_mem_pid_bridge.cpp ${SRC_DIR}/process_mem_pid_collect.cpp
               ${SRC_DIR}/process_mem_pid_config_manager.cpp ${SRC_DIR}/process_mem_pid_info_manager.cpp
               ${SRC_DIR}/process_mem_plugin.cpp ${PROJECT_SOURCE_DIR}/test/UT/main.cpp)
set_target_properties(${CMAKE_PROJECT_NAME}_process_mem_ut PROPERTIES LINK_FLAGS "-Wl,--as-needed")
target_compile_options(${CMAKE_PROJECT_NAME}_process_mem_ut PRIVATE
    -fPIC -w -g -Wno-all -Wno-error -Wno-non-template-friend
    -Wno-template-id-mismatch -Wno-invalid-offsetof -fno-access-control)
target_include_directories(${CMAKE_PROJECT_NAME}_process_mem_ut BEFORE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/mock/rack_manager/include
    ${PROJECT_SOURCE_DIR}/deps/mockcpp/include ${PROJECT_SOURCE_DIR}/deps/mockcpp/include/mockcpp
    ${PROJECT_SOURCE_DIR}/src/include ${PROJECT_SOURCE_DIR}/src/addons/process_mem
    ${PROJECT_SOURCE_DIR}/src/addons/rmrs/common/smap ${PROJECT_SOURCE_DIR}/src/addons/rmrs/include
    ${PROJECT_SOURCE_DIR}/src/framework/config ${PROJECT_SOURCE_DIR}/src/framework/storage
    ${PROJECT_SOURCE_DIR}/src/framework/ipc/include ${PROJECT_SOURCE_DIR}/src/framework/serde
    ${PROJECT_SOURCE_DIR}/src/framework/misc ${PROJECT_SOURCE_DIR}/src/framework/misc/referable
    ${PROJECT_SOURCE_DIR}/src/framework/misc/lock ${PROJECT_SOURCE_DIR}/src/framework/misc/container
    ${PROJECT_SOURCE_DIR}/src/framework/security ${PROJECT_SOURCE_DIR}/src/framework/com
    ${PROJECT_SOURCE_DIR}/src/framework/timer ${PROJECT_SOURCE_DIR}/src/framework/thread_pool
    ${PROJECT_SOURCE_DIR}/src/api_server ${PROJECT_SOURCE_DIR}/src/controllers/include
    ${PROJECT_SOURCE_DIR}/src/ras)
target_link_options(${CMAKE_PROJECT_NAME}_process_mem_ut PRIVATE -Wl,--relax --coverage -rdynamic -Wl,--as-needed)
target_compile_definitions(${CMAKE_PROJECT_NAME}_process_mem_ut PRIVATE UB_ENVIRONMENT ENABLE_COV=OFF)
target_link_libraries(${CMAKE_PROJECT_NAME}_process_mem_ut PUBLIC
    test_exclusive_main ubse_serial mempooling_static rapidjson GTest::gmock_main mockcpp boundscheck)
add_independent_exec_ut(${CMAKE_PROJECT_NAME}_process_mem_ut)
