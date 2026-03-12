cmake_minimum_required(VERSION 3.22)
project(ubs-engine VERSION 1.0 LANGUAGES C CXX)

# 回显 execute_process 命令（调试用），默认为 NONE
set(EXECUTE_PROCESS_COMMAND_ECHO NONE)

# 打印基本信息
message(STATUS "CMake version: ${CMAKE_VERSION}")
message(STATUS "Source dir: ${CMAKE_SOURCE_DIR}")
message(STATUS "Build dir: ${CMAKE_BINARY_DIR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build generator: ${CMAKE_GENERATOR}")

# 获取并打印编译器信息
message(STATUS "C compiler for the host machine: ${CMAKE_C_COMPILER} (${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION})")
set(C_LINKER_PREFERENCE "gold")
set(CXX_LINKER_PREFERENCE "gold")
message(STATUS "CXX compiler for the host machine: ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")

execute_process(
        COMMAND sh -c "${CMAKE_LINKER} --version | head -n 1"
        OUTPUT_VARIABLE LINKER_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "C linker for the host machine: ${CMAKE_LINKER} (${LINKER_VERSION})")

# 获取并打印主机信息
message(STATUS "Host machine system: ${CMAKE_HOST_SYSTEM}")
message(STATUS "Host machine cpu family: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
cmake_host_system_information(RESULT number_of_logical_cores QUERY NUMBER_OF_LOGICAL_CORES)
message(STATUS "Host machine logical cpu cores: ${number_of_logical_cores}")
cmake_host_system_information(RESULT number_of_physical_cores QUERY NUMBER_OF_PHYSICAL_CORES)
message(STATUS "Host machine physical cpu cores: ${number_of_physical_cores}")
cmake_host_system_information(RESULT total_physical_memory QUERY TOTAL_PHYSICAL_MEMORY)
message(STATUS "Host machine total physical memory: ${total_physical_memory} MB")
cmake_host_system_information(RESULT available_physical_memory QUERY AVAILABLE_PHYSICAL_MEMORY)
message(STATUS "Host machine available physical memory: ${available_physical_memory} MB")


# 生成编译数据库
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 默认使用 C11
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS NO)

# 默认使用 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_SKIP_RPATH TRUE)

message(STATUS "Build C standard: C${CMAKE_C_STANDARD}")
message(STATUS "Build C++ standard: C++${CMAKE_CXX_STANDARD}")

set(DEPS_DIR ${PROJECT_SOURCE_DIR}/deps)
set(SRC_DIR ${PROJECT_SOURCE_DIR}/src)

include(GNUInstallDirs)
set(INSTALL_LIBDIR lib)

message(DEBUG "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(DEBUG "CMAKE_INSTALL_BINDIR: ${CMAKE_INSTALL_BINDIR}")
message(DEBUG "INSTALL_LIBDIR: ${INSTALL_LIBDIR}")
message(DEBUG "CMAKE_INSTALL_INCLUDEDIR: ${CMAKE_INSTALL_INCLUDEDIR}")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/scripts/cmake" ${CMAKE_MODULE_PATH})
include(building)
include(ccache)
include(utils)
include(doxygen)
include(iwyu)

# 设置所有共享库的输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# 设置所有可执行的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(DEBUG "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(DEBUG "CXX_FLAGS_DEBUG: ${CXX_FLAGS_DEBUG}")
message(DEBUG "CXX_FLAGS_RELEASE: ${CXX_FLAGS_RELEASE}")
message(DEBUG "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(DEBUG "CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(DEBUG "CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
message(DEBUG "CMAKE_CXX_FLAGS_MINSIZEREL: ${CMAKE_CXX_FLAGS_MINSIZEREL}")
add_compile_definitions(VOS_WORDSIZE=64)
add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)

option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." OFF)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options(-fcolor-diagnostics)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-fdiagnostics-color=always)
    endif ()
else ()
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-fdiagnostics-color=auto)
    endif ()
endif ()

# coverage option
option(ENABLE_COVERAGE "Use gcov" OFF)
message(DEBUG ENABLE_COVERAGE=${ENABLE_COVERAGE})
if (ENABLE_COVERAGE)
    setup_coverage()
endif ()


# 全局作用域的头文件定义，不依赖具体模块
include_directories(
        ${PROJECT_SOURCE_DIR}/include   # 外部共享头文件
        ${SRC_DIR}/include              # 内部共享头文件
        ${CMAKE_BINARY_DIR}/include     # 自动生成头文件
        ${PROJECT_SOURCE_DIR}           # 项目根目录（解头文件依赖）
        ${SRC_DIR}/include/adapter_plugins/mmi            # 内部共享头文件
)
option(BUILD_TESTS "Build tests" OFF)

option(ENABLE_UB "Enable UB optimization" ON)
if (ENABLE_UB)
    add_definitions(-DUB_ENVIRONMENT)
endif ()

add_subdirectory(3rdparty)
add_subdirectory(src)

set(ENABLE_UBSE_TESTING 0)

if (BUILD_TESTS)
    set(CMAKE_SKIP_RPATH OFF)
    set(ENABLE_UBSE_TESTING 1)
    add_subdirectory(test)
endif ()

# 生成构建环境相关的头文件
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/include/config.h
        @ONLY
)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/register_xalarm.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/include/register_xalarm.h
        @ONLY
)

file(COPY ${PROJECT_SOURCE_DIR}/scripts/command_completion/cli_commands.sh
        DESTINATION ${CMAKE_BINARY_DIR}/scripts
        FILE_PERMISSIONS OWNER_READ OWNER_EXECUTE
)

file(COPY ${PROJECT_SOURCE_DIR}/conf/
        DESTINATION ${CMAKE_BINARY_DIR}/conf
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE
        PATTERN "README.md" EXCLUDE
)

add_custom_target(install_all
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component ubse_com_sdk_c
        COMMENT "Install all files to ${CMAKE_INSTALL_PREFIX}"
)

add_custom_target(package_ubse_com
        COMMAND mkdir -p ${PROJECT_SOURCE_DIR}/output
        COMMAND tar -czvf ${PROJECT_SOURCE_DIR}/output/ubse_com_aarch64.tar.gz .
        COMMENT "Package ubse_com to zip"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/install_ubse_com
)

add_dependencies(package_ubse_com install_all)