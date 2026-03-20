# 获取当前内核版本
execute_process(
        COMMAND uname -r
        OUTPUT_VARIABLE KERNEL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 设置内核构建目录
set(KDIR /lib/modules/${KERNEL_VERSION}/build)
set(MODULE_NAME bandbridge)

# 设置模块输出目录（可自定义路径）
set(MODULE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/modules)  # 默认为CMake二进制目录下的modules文件夹
# 或者设置为项目源码目录下的output文件夹：set(MODULE_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/output)

# 检查内核构建目录是否存在
if(NOT EXISTS "${KDIR}")
    message(FATAL_ERROR "内核构建目录未找到: ${KDIR}")
endif()

# 定义源文件
set(SRCS
        bandbridge_main.c
        bandbridge_device.c
        bandbridge_ctrlq.c
        bandbridge_log.c
)

# 确保输出目录存在
file(MAKE_DIRECTORY ${MODULE_OUTPUT_DIR})

add_library(${MODULE_NAME} MODULE ${SRCS})
target_include_directories(${MODULE_NAME} PRIVATE ${KDIR}/arch/arm64/include/ ${KDIR}/include)
set_target_properties(${MODULE_NAME} PROPERTIES
        LINK_FLAGS "-Wl,-rpath-link,${KDIR}/lib"
)

# 主构建目标 - 适配Ninja
add_custom_target(${MODULE_NAME}_ko ALL
        COMMAND make -C ${KDIR} M=${CMAKE_CURRENT_SOURCE_DIR} modules INSTALL_MOD_PATH=${MODULE_OUTPUT_DIR} modules_install
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}.ko ${MODULE_OUTPUT_DIR}/
        WORKING_DIRECTORY .
        COMMENT "使用Ninja编译内核模块 ${MODULE_NAME} 并导出到 ${MODULE_OUTPUT_DIR}"
        BYPRODUCTS ${MODULE_OUTPUT_DIR}/${MODULE_NAME}.ko
        COMMAND make -C ${KDIR} M=${CMAKE_CURRENT_SOURCE_DIR} clean
        VERBATIM
)