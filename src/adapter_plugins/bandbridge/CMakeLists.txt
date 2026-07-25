# SPDX-License-Identifier: GPL-2.0
set(KDIR /lib/modules/${KERNEL_VERSION}/build)
set(MODULE_NAME bandbridge)
set(MODULE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/modules)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" AND EXISTS "${KDIR}")
    file(MAKE_DIRECTORY ${MODULE_OUTPUT_DIR})

    add_custom_target(${MODULE_NAME}_ko ALL
            COMMAND make -C ${KDIR} M=${CMAKE_CURRENT_SOURCE_DIR} clean
            COMMAND make -C ${KDIR} M=${CMAKE_CURRENT_SOURCE_DIR} modules
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_NAME}.ko ${MODULE_OUTPUT_DIR}/
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Building kernel module ${MODULE_NAME} and exporting to ${MODULE_OUTPUT_DIR}"
            BYPRODUCTS ${MODULE_OUTPUT_DIR}/${MODULE_NAME}.ko
            VERBATIM
    )
else()
    message(STATUS "Skipping ${MODULE_NAME} kernel module build: processor=${CMAKE_SYSTEM_PROCESSOR}, KDIR=${KDIR}")
endif()